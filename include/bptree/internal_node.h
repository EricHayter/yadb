#pragma once
#include "b_plus_tree.h"
#include "common/type_definitions.h"
#include <array>
#include <cstddef>

template <Comparable KeyType, typename ValueType>
class InternalNode {
    static constexpr std::size_t PAGE_CAPACITY = PAGE_SIZE / ( sizeof(KeyType) + sizeof(KeyType));

    std::size_t current_size_m;

    std::array<KeyType, PAGE_CAPACITY> keys_m;
    std::array<ValueType, PAGE_CAPACITY> values_m;
};
