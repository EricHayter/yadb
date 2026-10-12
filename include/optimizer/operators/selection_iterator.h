#pragma once

#include "Parser.h"
#include "common/definitions.h"
#include "optimizer/operators/iterator.h"
#include <memory>
#include <span>

class SelectionIterator : public Iterator {
public:
    SelectionIterator(std::unique_ptr<Iterator> in, Schema schema, const Condition& condition);

    Iterator& operator++() override;

private:
    void find_next_match();

    bool evaluate_condition(const Condition& cond, std::span<const std::byte> tuple) const;
    bool evaluate_logical_condition(const LogicalCondition& cond, std::span<const std::byte> tuple) const;
    bool evaluate_comparison(const Comparison& cmp, std::span<const std::byte> tuple) const;
    std::size_t column_index(const ColumnRef& column) const;
    // Columns are read from the tuple, literals are parsed as the given type
    Value resolve_operand(const Operand& operand, TypeId type, std::span<const std::byte> tuple) const;
    bool compare_values(const Value& left, ComparisonOp op, const Value& right) const;

    std::unique_ptr<Iterator> in_m;
    Schema schema_m;
    const Condition& condition_m;
};
