#include "core/buffer_builder.h"
#include <cstdlib>
#include <cstring>
#include "common/definitions.h"
#include <new>

BufferBuilder::~BufferBuilder() {
    if (data_m)
        free(data_m);
}

BufferBuilder::BufferBuilder(BufferBuilder&& other) noexcept
    : size_m(other.size_m),
      capacity_m(other.capacity_m),
      data_m(other.data_m) {
    other.size_m = 0;
    other.capacity_m = 0;
    other.data_m = nullptr;
}

BufferBuilder& BufferBuilder::operator=(BufferBuilder&& other) noexcept {
    if (this != &other) {
        // Free our current resources
        if (data_m)
            free(data_m);

        // Steal other's resources
        size_m = other.size_m;
        capacity_m = other.capacity_m;
        data_m = other.data_m;

        // Leave other in valid state
        other.size_m = 0;
        other.capacity_m = 0;
        other.data_m = nullptr;
    }
    return *this;
}

void BufferBuilder::AllocateSpace() {
    void *new_data;
    if (!data_m) {
        new_data = malloc(DEFAULT_ALLOC_SIZE);
        capacity_m = DEFAULT_ALLOC_SIZE;
    } else {
        capacity_m = capacity_m * GROWTH_RATE;
        new_data = realloc(data_m, capacity_m);
    }

    if (!new_data)
        throw std::bad_alloc();

    data_m = (std::byte*) new_data;
}

void BufferBuilder::AllocateSpace(std::size_t size) {
    if (size <= capacity_m)
        return;

    void *new_data;
    if (!data_m) {
        new_data = malloc(size);
    } else {
        new_data = realloc(data_m, size);
    }
    capacity_m = size;

    if (!new_data)
        throw std::bad_alloc();

    data_m = (std::byte*) new_data;
}
