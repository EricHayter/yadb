#include "optimizer/operators/selection_iterator.h"
#include "Parser.h"
#include "core/row_reader.h"
#include <format>
#include <stdexcept>
#include <type_traits>
#include <variant>

SelectionIterator::SelectionIterator(std::unique_ptr<Iterator> in, Schema schema, const Condition& condition)
    : in_m(std::move(in))
    , schema_m(std::move(schema))
    , condition_m(condition)
{
    find_next_match();
}

Iterator& SelectionIterator::operator++()
{
    ++(*in_m);
    find_next_match();
    return *this;
}

void SelectionIterator::find_next_match()
{
    while (!(*in_m == std::default_sentinel_t {})) {
        if (evaluate_condition(condition_m, **in_m)) {
            current_m = **in_m;
            return;
        }
        ++(*in_m);
    }
    current_m = std::nullopt;
}

bool SelectionIterator::evaluate_condition(const Condition& cond, std::span<const std::byte> tuple) const
{
    return std::visit([&](auto&& expr) -> bool {
        using T = std::decay_t<decltype(expr)>;
        if constexpr (std::is_same_v<T, Comparison>) {
            return evaluate_comparison(expr, tuple);
        } else if constexpr (std::is_same_v<T, LogicalCondition>) {
            return evaluate_logical_condition(expr, tuple);
        }
        return false;
    },
        cond.expr);
}

bool SelectionIterator::evaluate_logical_condition(const LogicalCondition& cond, std::span<const std::byte> tuple) const
{
    bool left_result = evaluate_condition(*cond.left, tuple);

    if (cond.op == LogicalOp::AND && !left_result)
        return false;
    if (cond.op == LogicalOp::OR && left_result)
        return true;

    bool right_result = evaluate_condition(*cond.right, tuple);

    if (cond.op == LogicalOp::AND)
        return left_result && right_result;
    else
        return left_result || right_result;
}

bool SelectionIterator::evaluate_comparison(const Comparison& cmp, std::span<const std::byte> tuple) const
{
    // Literals take on the type of the column they're compared against. If
    // there's no column at all both literals are just compared as strings.
    TypeId type = type_id<String>;
    if (const auto* left_column = std::get_if<ColumnRef>(&cmp.left))
        type = schema_m[column_index(*left_column)].type;
    else if (const auto* right_column = std::get_if<ColumnRef>(&cmp.right))
        type = schema_m[column_index(*right_column)].type;

    Value left_val = resolve_operand(cmp.left, type, tuple);
    Value right_val = resolve_operand(cmp.right, type, tuple);
    return compare_values(left_val, cmp.op, right_val);
}

std::size_t SelectionIterator::column_index(const ColumnRef& column) const
{
    for (std::size_t i = 0; i < schema_m.size(); ++i) {
        if (schema_m[i].name == column.name)
            return i;
    }
    throw std::runtime_error(std::format("Unknown column '{}'", column.name));
}

Value SelectionIterator::resolve_operand(const Operand& operand, TypeId type, std::span<const std::byte> tuple) const
{
    if (const auto* column = std::get_if<ColumnRef>(&operand)) {
        RowReader reader(tuple, schema_m);
        return reader.GetValue(column_index(*column));
    }

    const auto& literal = std::get<Literal>(operand);
    auto value = ValueFromString(type, literal.text);
    if (!value)
        throw std::runtime_error(value.error()->what());
    return std::move(*value);
}

bool SelectionIterator::compare_values(const Value& left, ComparisonOp op, const Value& right) const
{
    return std::visit([op](auto&& lhs, auto&& rhs) -> bool {
        using L = std::decay_t<decltype(lhs)>;
        using R = std::decay_t<decltype(rhs)>;

        if constexpr (!std::is_same_v<L, R>) {
            throw std::runtime_error(std::format("Cannot compare {} with {}", L::type_str, R::type_str));
        } else {
            switch (op) {
            case ComparisonOp::EQ:
                return lhs == rhs;
            case ComparisonOp::NEQ:
                return lhs != rhs;
            case ComparisonOp::LT:
                return lhs < rhs;
            case ComparisonOp::GT:
                return lhs > rhs;
            case ComparisonOp::LE:
                return lhs <= rhs;
            case ComparisonOp::GE:
                return lhs >= rhs;
            }
            return false;
        }
    },
        left, right);
}
