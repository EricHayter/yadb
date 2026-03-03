#pragma once

#include <vector>
#include <optional>

class Iterator {
    public:
    virtual ~Iterator() = 0;
    virtual std::optional<std::vector<std::byte>> next() = 0;
    virtual void close() = 0;
};
