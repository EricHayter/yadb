#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "common/datatypes.h"

// Row ID is a simple 64-bit integer
using row_id_t = uint64_t;

struct RelationAttribute {
    std::string name;
    TypeId type;
};

using Schema = std::vector<RelationAttribute>;

using Row = std::pair<row_id_t, std::span<const std::byte>>;

enum class TableError {
    INVALID_ROW_ID,
};

enum class TableType {
    InMemory,
    Disk
};
