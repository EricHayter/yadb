#include "storage/on_disk/table/disk_table_factory.h"
#include "storage/on_disk/constants.h"
#include "core/assert.h"
#include "core/row_reader.h"
#include "storage/on_disk/heap/heap_page.h"
#include "storage/on_disk/table/disk_table.h"
#include <memory>

bool DiskTableFactory::TableExists(std::string_view table_name) const
{
    return mapping_manager_m.GetFileId(table_name).has_value();
}

bool DiskTableFactory::CreateTableFile(std::string_view table_name, std::optional<file_id_t> file_id)
{
    file_id_t actual_file_id;

    if (file_id) {
        actual_file_id = *file_id;
        auto err = page_buffer_manager_m->CreateFile(actual_file_id);
        YADB_ASSERT(!err.has_value(), (*err)->what().c_str());
    } else {
        auto result = page_buffer_manager_m->CreateFile();
        YADB_ASSERT(result.has_value(), result.error()->what().c_str());
        actual_file_id = *result;
    }

    // Reserved tables (mapping table) are resolved by their fixed
    // file id and are never recorded in the mapping table.
    if (!IsReservedFileId(actual_file_id))
        mapping_manager_m.SaveMapping(table_name, actual_file_id);

    InitTableRootPage(actual_file_id);
    return true;
}

void DiskTableFactory::InitTableRootPage(file_id_t file_id)
{
    // A freshly created file has no pages; allocate the root page to grow it.
    auto alloc = page_buffer_manager_m->AllocatePage(file_id);
    YADB_ASSERT(alloc.has_value(), alloc.error()->what().c_str());
    YADB_ASSERT(*alloc == ROOT_PAGE_ID, "First allocated page must be the root page");

    auto page_result = page_buffer_manager_m->GetPage({ file_id, ROOT_PAGE_ID });
    YADB_ASSERT(page_result.has_value(), page_result.error()->what().c_str());
    Page page = std::move(*page_result);
    std::lock_guard<Page> lk(page);

    // Initialize as a heap page so the root's next/prev fields (which double as
    // the full/partial page-list heads) start as NULL_PAGE_ID, i.e. empty table.
    heap_page::InitPage(page.GetMutView());
}

TableHandle DiskTableFactory::OpenOrCreateMappingTable()
{
    if (!page_buffer_manager_m->FileExists(DISK_TABLE_MAPPING_FILE_ID)) {
        auto err = page_buffer_manager_m->CreateFile(DISK_TABLE_MAPPING_FILE_ID);
        YADB_ASSERT(!err.has_value(), (*err)->what().c_str());
        InitTableRootPage(DISK_TABLE_MAPPING_FILE_ID);
    }

    auto storage = std::shared_ptr<Table>(new DiskTable(DISK_TABLE_MAPPING_FILE_ID, *page_buffer_manager_m));
    return TableHandle { std::move(storage), MappingManager::MAPPING_SCHEMA };
}

DiskTableFactory::DiskTableFactory()
    : page_buffer_manager_m(std::make_unique<PageBufferManager>())
    , mapping_manager_m(OpenOrCreateMappingTable())
{
}

bool DiskTableFactory::CreateTable(std::string_view table_name)
{
    return CreateTableFile(table_name);
}

std::shared_ptr<Table> DiskTableFactory::GetTable(std::string_view table_name)
{
    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id)
        return nullptr;
    return std::shared_ptr<Table>(new DiskTable(*file_id, *page_buffer_manager_m));
}

bool DiskTableFactory::DeleteTable(std::string_view table_name)
{
    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id) {
        return false;
    }

    auto err = page_buffer_manager_m->DeleteFile(*file_id);
    YADB_ASSERT(!err.has_value(), (*err)->what().c_str());
    mapping_manager_m.DeleteMapping(table_name);

    return true;
}

DiskTableFactory::MappingManager::MappingManager(TableHandle mapping_table)
    : mapping_table_m(std::move(mapping_table))
{}

std::optional<file_id_t> DiskTableFactory::MappingManager::GetFileId(std::string_view table_name) const
{
    for (auto [row_id, data] : mapping_table_m) {
        RowReader rr(data, MAPPING_SCHEMA);
        if (rr.Get<DataType::TEXT>(0) == table_name)
            return static_cast<file_id_t>(rr.Get<DataType::INTEGER>(1));
    }
    return {};
}

bool DiskTableFactory::MappingManager::SaveMapping(std::string_view table_name, file_id_t file_id)
{
    mapping_table_m.insert_row({
        Value(std::string(table_name)),
        Value(static_cast<std::int32_t>(file_id)),
    });
    return true;
}

bool DiskTableFactory::MappingManager::DeleteMapping(std::string_view table_name)
{
    std::optional<row_id_t> found_rid;
    for (auto [row_id, data] : mapping_table_m) {
        RowReader rr(data, MAPPING_SCHEMA);
        if (rr.Get<DataType::TEXT>(0) == table_name) {
            found_rid = row_id;
            break;
        }
    }
    if (found_rid) {
        mapping_table_m.delete_row(*found_rid);
        return true;
    }
    return false;
}
