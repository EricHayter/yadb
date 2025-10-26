#pragma once

#include "buffer/page_buffer_manager.h"
#include "page/page.h"

class BPTree {
public:
    /* Creates an entirely new BPTree */
    BPTree(PageBufferManager* page_buffer_manager);

    /* Get a handle to an existing B+ Tree */
    BPTree(PageBufferManager* page_buffer_manager, page_id_t root_page_id);

    std::optional<std::vector<char>> Search(std::span<const char> key);
    void Insert(std::span<const char> key, std::span<const char> value);
    void Delete(std::span<const char> key);
private:
    bool InsertOptimistic(std::span<const char> key, std::span<const char> value);
    void InsertPessimistic(std::span<const char> key, std::span<const char> value);

    bool DeleteOptimistic(std::span<const char> key);
    void DeletePessimistic(std::span<const char> key);

private:
    page_id_t root_page_id_m;
    PageBufferManager* page_buffer_manager_m;
};
