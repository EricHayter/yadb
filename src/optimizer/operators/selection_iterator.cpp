#include "optimizer/operators/selection_iterator.h"
#include "Parser.h"
#include "core/row_reader.h"
#include <type_traits>
#include <variant>

SelectionIterator::SelectionIterator(std::unique_ptr<Iterator> in, const Schema& schema, const Condition& condition)
    : in_m(std::move(in))
    , schema_m(schema)
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
    Value left_val = resolve_value(cmp.left, tuple);
    Value right_val = resolve_value(cmp.right, tuple);
    return compare_values(left_val, cmp.op, right_val);
}

Value SelectionIterator::resolve_value(const Value& val, std::span<const std::byte> tuple) const
{
    if (std::holds_alternative<std::string>(val)) {
        const std::string& str = std::get<std::string>(val);

        for (std::size_t i = 0; i < schema_m.size(); ++i) {
            if (schema_m[i].name == str) {
                RowReader reader(tuple, schema_m);

                if (schema_m[i].type == DataType::INTEGER) {
                    return Value { reader.Get<DataType::INTEGER>(i) };
                } else if (schema_m[i].type == DataType::TEXT) {
                    return Value { reader.Get<DataType::TEXT>(i) };
                }
            }
        }
        return val;
    }

    return val;
}

bool SelectionIterator::compare_values(const Value& left, ComparisonOp op, const Value& right) const
{
    return std::visit([op](auto&& lhs, auto&& rhs) -> bool {
        using L = std::decay_t<decltype(lhs)>;
        using R = std::decay_t<decltype(rhs)>;

        if constexpr (!std::is_same_v<L, R>) {
            return false;
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
