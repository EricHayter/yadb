#pragma once

#include "buffer/page_buffer_manager.h"
#include "page/page.h"

class BPTree {
public:
    /* Creates an entirely new BPTree */
    BPTree(PageBufferManager* page_buffer_manager);

    /* Get a handle to an existing B+ Tree */
    BPTree(PageBufferManager* page_buffer_manager, page_id_t root_page_id);

    std::optional<record_id_t> Search(std::span<const char> key);
    void Insert(std::span<const char> key, record_id_t);
    void Delete(std::span<const char> key);
private:
    bool InsertOptimistic(std::span<const char> key, record_id_t record_id);
    void InsertPessimistic(std::span<const char> key, record_id_t record_id);

    bool DeleteOptimistic(std::span<const char> key);
    void DeletePessimistic(std::span<const char> key);

private:
    page_id_t root_page_id_m;
    PageBufferManager* page_buffer_manager_m;
};
