#include "storage/on_disk/table/disk_table_manager.h"
#include "storage/on_disk/constants.h"
#include "core/assert.h"
#include "storage/on_disk/page/page_format.h"
#include <memory>

DiskTableManager::MappingManager::MappingManager()
{
    // Open the metadata file for reading and writing in binary mode
    fstream_m.open(DISK_TABLE_MAPPING_FILE.data(), std::ios::in | std::ios::out | std::ios::binary | std::ios::app);

    // If file doesn't exist, create it
    if (!fstream_m.is_open()) {
        fstream_m.clear();
        fstream_m.open(DISK_TABLE_MAPPING_FILE.data(), std::ios::out | std::ios::binary);
        fstream_m.close();
        fstream_m.open(DISK_TABLE_MAPPING_FILE.data(), std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    }

    // Read existing mappings from the beginning of the file
    fstream_m.seekg(0, std::ios::beg);

    while (fstream_m.peek() != EOF) {
        file_id_t file_id;
        uint32_t name_length;

        // Read file_id
        fstream_m.read(reinterpret_cast<char*>(&file_id), sizeof(file_id));
        if (!fstream_m) break;

        // Read table name length
        fstream_m.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        if (!fstream_m) break;

        // Read table name
        std::string table_name(name_length, '\0');
        fstream_m.read(table_name.data(), name_length);
        if (!fstream_m) break;

        // Add to in-memory map
        file_id_map_m[table_name] = file_id;
    }

    // Clear any error flags and seek to end for appending
    fstream_m.clear();
    fstream_m.seekp(0, std::ios::end);
}

void DiskTableManager::SetCatalog(const Catalog& catalog)
{
    catalog_m = catalog;
}

DiskTableManager::DiskTableManager(PageBufferManager& page_buffer_manager)
    : page_buffer_manager_m(page_buffer_manager)
    , mapping_manager_m()
{
}

bool DiskTableManager::CreateTable(std::string_view table_name, const Schema& schema)
{
    // haven't set the catalog yet
    if (!catalog_m) {
        YADB_ASSERT(table_name == TABLE_CATALOG_TABLE_NAME || table_name == COLUMN_CATALOG_TABLE_NAME,
            std::format("Trying to create non-catalog table '%s' without registering a catalog", table_name)
        );
        // TODO set this up creating the table...
    }

    if (catalog_m.TableExists(table_name)) {
        return false;
    }

    file_id_t file_id = page_buffer_manager_m.CreateFile();
    mapping_manager_m.SaveMapping(table_name, file_id);
    Page page = page_buffer_manager_m.GetPage({ file_id, 0 });
    std::lock_guard<Page> lk(page);
    page::InitPage(page.GetMutView(), page::PageType::Data);

    return catalog_m.AddTable(table_name, TableType::Disk, schema);
}

std::shared_ptr<DiskTable> DiskTableManager::GetTable(std::string_view table_name)
{
    if (!catalog_m.TableExists(table_name)) {
        return nullptr;
    }

    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id) {
        return nullptr;
    }

    Schema schema = catalog_m.GetSchema(table_name);
    return std::shared_ptr<DiskTable>(new DiskTable(
        *file_id,
        schema,
        page_buffer_manager_m
    ));
}

std::optional<file_id_t> DiskTableManager::MappingManager::GetFileId(std::string_view table_name) const
{
    std::string table_name_str = std::string(table_name);
    std::shared_lock<std::shared_mutex> lk(mut_m);
    if (!file_id_map_m.contains(table_name_str))
        return {};
    return file_id_map_m.at(table_name_str);
}

bool DiskTableManager::MappingManager::SaveMapping(std::string_view table_name, file_id_t file_id)
{
    std::lock_guard<std::shared_mutex> lg(mut_m);

    // Write file_id (4 bytes)
    fstream_m.write(reinterpret_cast<const char*>(&file_id), sizeof(file_id));

    // Write table name length (4 bytes)
    uint32_t name_length = static_cast<uint32_t>(table_name.length());
    fstream_m.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));

    // Write table name (variable bytes)
    fstream_m.write(table_name.data(), name_length);

    // Flush to ensure data is written to disk
    fstream_m.flush();

    // Update in-memory map
    file_id_map_m[std::string(table_name)] = file_id;

    return fstream_m.good();
}
