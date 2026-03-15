#pragma once

#include "common/definitions.h"
#include "optimizer/operators/iterator.h"
#include "Parser.h"
#include <functional>
#include <memory>
#include <span>

class SelectionIterator : public Iterator {
    public:
    /* What is this going to need?
     * going to need an iterator
     * going to need a funciton of some sort I guess.
     * Schema
     * I guess the parameters? A vector mapping most likely
     */
    using SelectionFunction = std::function<bool(const std::vector<std::byte>&)>;

    // TODO need to figure out if I hsould be using pointers or references it seems ugly to mix them...
    SelectionIterator(std::unique_ptr<Iterator> in, const Schema& schema, const Condition& condition);
    ~SelectionIterator();
    std::optional<std::vector<std::byte>> next() override;
    void close() override;

    private:
    bool evaluate_condition(const Condition& cond, std::span<const std::byte> tuple) const;
    bool evaluate_logical_condition(const LogicalCondition& cond, std::span<const std::byte> tuple) const;
    bool evaluate_comparison(const Comparison& cmp, std::span<const std::byte> tuple) const;
    Value resolve_value(const Value& val, std::span<const std::byte> tuple) const;
    bool compare_values(const Value& left, ComparisonOp op, const Value& right) const;

    private:
    bool closed_m{false};
    std::unique_ptr<Iterator> in_m;
    const Schema& schema_m;
    const Condition& condition_m;
};
