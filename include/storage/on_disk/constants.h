#pragma once

#include "storage/on_disk/types.h"
#include <cstdint>
#include <string_view>

// File type definitions
constexpr file_id_t INVALID_FILE_ID = UINT32_MAX;

// Reserved file IDs
// Only the mapping table needs a fixed id: it is the bootstrap root that every
// other table (including the catalogs) is looked up through, so it can't be
// resolved via the mapping itself. All other tables get auto-allocated ids.
constexpr file_id_t DISK_TABLE_MAPPING_FILE_ID = 0;

// File name constants
constexpr std::string_view DISK_TABLE_MAPPING_FILE = "disk_table_mapping";
constexpr std::string_view DATABASE_FILE_EXTENSION = ".yadb";
constexpr std::string_view TABLE_FILE_EXTENSION = ".db";

// Page constants
constexpr page_id_t NULL_PAGE_ID = UINT32_MAX;
constexpr page_id_t ROOT_PAGE_ID = 0;

// File generation constants
constexpr uint32_t MAX_FILE_ID_RETRIES = 10;

// Helper function to check if a file ID is reserved
constexpr bool IsReservedFileId(file_id_t file_id)
{
    return file_id == DISK_TABLE_MAPPING_FILE_ID;
}
