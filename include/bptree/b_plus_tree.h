#pragma once
#include <concepts>

template<typename T>
concept Comparable = requires(T ls, T rs)
{
    { ls < rs } -> std::convertible_to<bool>;
    { ls == rs } -> std::convertible_to<bool>;
};

