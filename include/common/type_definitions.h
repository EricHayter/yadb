#pragma once

#include <cstdint>
#include <span>

constexpr std::size_t PAGE_SIZE = 4096;

using page_id_t = uint32_t;
using frame_id_t = uint32_t;

// view/span into a page-sized buffer. Immutable by default so I don't blow my
// leg off by accidentally handling a mutable view.
using MutPageView = std::span<char, PAGE_SIZE>;
using PageView = std::span<const char, PAGE_SIZE>;
