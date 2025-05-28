#pragma once

#include <cstdint>
#include <span>

constexpr std::size_t PAGE_SIZE = 4096;

using page_id_t = uint32_t;
using frame_id_t = uint32_t;

using PageData = std::span<char, PAGE_SIZE>;
