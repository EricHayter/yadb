#include "storage/on_disk/table/disk_table_factory.h"
#include "storage/on_disk/constants.h"
#include "core/assert.h"
#include "storage/on_disk/page/page_format.h"
#include "storage/on_disk/table/disk_table.h"
#include <memory>
#include <shared_mutex>

DiskTableFactory::MappingManager::MappingManager()
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

bool DiskTableFactory::TableExists(std::string_view table_name) const
{
    // Special case for catalog tables - check file mapping directly
    // since they exist before the catalog is set up
    if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME || table_name == Catalog::COLUMN_CATALOG_TABLE_NAME) {
        return mapping_manager_m.GetFileId(table_name).has_value();
    }

    // For regular tables, use catalog if available
    auto catalog = GetCatalog();
    if (catalog.has_value()) {
        return catalog->TableExists(table_name);
    }

    // Fallback: check if file mapping exists (table was created but catalog not set yet)
    return mapping_manager_m.GetFileId(table_name).has_value();
}

bool DiskTableFactory::CreateTableFile(std::string_view table_name, std::optional<file_id_t> file_id)
{
    file_id_t actual_file_id;

    if (file_id) {
        // Use the provided file ID (for catalog tables)
        actual_file_id = *file_id;
        page_buffer_manager_m.CreateFile(actual_file_id);
    } else {
        // Generate a new file ID (for regular tables)
        actual_file_id = page_buffer_manager_m.CreateFile();
    }

    mapping_manager_m.SaveMapping(table_name, actual_file_id);

    // Initialize first page as data page
    Page page = page_buffer_manager_m.GetPage({ actual_file_id, 0 });
    std::lock_guard<Page> lk(page);
    page::InitPage(page.GetMutView(), page::PageType::Data);

    return true;
}

DiskTableFactory::DiskTableFactory(PageBufferManager& page_buffer_manager)
    : page_buffer_manager_m(page_buffer_manager)
    , mapping_manager_m()
{
}

DiskTableFactory::DiskTableFactory(PageBufferManager& page_buffer_manager, const Catalog& catalog)
    : DiskTableFactory(page_buffer_manager)
{
    SetCatalog(catalog);
}

bool DiskTableFactory::CreateTable(std::string_view table_name, const Schema& schema)
{
    // Check if this is a catalog table
    if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME || table_name == Catalog::COLUMN_CATALOG_TABLE_NAME) {
        YADB_ASSERT(!GetCatalog().has_value(), "Cannot create catalog tables after catalog has been set");

        // Create catalog table with reserved file ID
        file_id_t reserved_id = (table_name == Catalog::TABLE_CATALOG_TABLE_NAME)
            ? TABLE_CATALOG_FILE_ID
            : COLUMN_CATALOG_FILE_ID;

        if (!CreateTableFile(table_name, reserved_id)) {
            return false;
        }

        // Create table object directly and initialize it with catalog metadata
        std::shared_ptr<DiskTable> table(new DiskTable(reserved_id, schema, page_buffer_manager_m));
        if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME) {
            Catalog::InitializeTableCatalogTable(*table);
        } else {
            Catalog::InitializeColumnCatalogTable(*table);
        }

        return true;
    }

    // Regular table creation - requires catalog to be set
    YADB_ASSERT(GetCatalog().has_value(), "Cannot create regular tables before catalog is set");

    if (GetCatalog()->TableExists(table_name)) {
        return false;
    }

    // Create regular table with dynamic file ID
    if (!CreateTableFile(table_name)) {
        return false;
    }

    return true;
}

std::shared_ptr<Table> DiskTableFactory::GetTable(std::string_view table_name)
{
    auto catalog = GetCatalog();
    if (!catalog || !catalog->TableExists(table_name)) {
        return nullptr;
    }

    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id) {
        return nullptr;
    }

    Schema schema = catalog->GetSchema(table_name);
    return std::shared_ptr<Table>(new DiskTable(
        *file_id,
        schema,
        page_buffer_manager_m
    ));
}

bool DiskTableFactory::DeleteTable(std::string_view table_name)
{
    if (!TableExists(table_name)) {
        return false;
    }

    // TODO: Delete the actual disk file and clean up mappings
    // This would involve:
    // 1. Removing the file mapping
    // 2. Deleting the physical file
    // 3. Cleaning up page buffers

    return true;
}

std::optional<file_id_t> DiskTableFactory::MappingManager::GetFileId(std::string_view table_name) const
{
    std::string table_name_str = std::string(table_name);
    std::shared_lock<std::shared_mutex> lk(mut_m);
    if (!file_id_map_m.contains(table_name_str))
        return {};
    return file_id_map_m.at(table_name_str);
}

bool DiskTableFactory::MappingManager::SaveMapping(std::string_view table_name, file_id_t file_id)
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
