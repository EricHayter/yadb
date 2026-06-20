#pragma once

#include "table/table.h"
#include "storage/on_disk/types.h"
#include "storage/on_disk/buffer_manager/page_buffer_manager.h"
#include <filesystem>

class DiskTable : public Table {
    friend class DiskTableFactory;

public:
    ~DiskTable() override = default;

    TableCursor begin() override;
    row_id_t insert_row(std::span<const std::byte> row) override;
    row_id_t update_row(const row_id_t& rid, std::span<const std::byte> data) override;
    void delete_row(const row_id_t& rid) override;

    TableType GetType() const override;

private:
    static std::filesystem::path GetTableFileName(std::string_view table_name);

private:
    DiskTable(file_id_t file_id, PageBufferManager& page_buffer_manager);
    file_id_t file_id_m;
    PageBufferManager& page_buffer_manager_m;
};
