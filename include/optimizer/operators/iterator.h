#pragma once

#include "core/assert.h"
#include <iterator>
#include <optional>
#include <vector>

class Iterator {
public:
    using value_type = std::vector<std::byte>;

    // Thin non-owning handle returned by begin() so range-for can store it by value.
    struct Ref {
        Iterator* it;
        const value_type& operator*() const { return **it; }
        Ref& operator++() { ++(*it); return *this; }
        bool operator==(std::default_sentinel_t) const { return *it == std::default_sentinel_t {}; }
    };

    Iterator() = default;
    virtual ~Iterator() = default;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    const value_type& operator*() const
    {
        YADB_ASSERT(current_m.has_value(), "Dereferencing exhausted iterator");
        return *current_m;
    }

    virtual Iterator& operator++() = 0;

    bool operator==(std::default_sentinel_t) const { return !current_m.has_value(); }

    Ref begin() { return Ref { this }; }
    std::default_sentinel_t end() const { return {}; }

protected:
    std::optional<value_type> current_m;
};
