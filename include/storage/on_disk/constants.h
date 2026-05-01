#pragma once

#include "storage/on_disk/types.h"
#include <cstdint>
#include <string_view>

// File type definitions
constexpr file_id_t INVALID_FILE_ID = UINT32_MAX;

// Reserved file IDs
constexpr file_id_t CATALOG_FILE_ID = 0;

// File name constants
constexpr std::string_view CATALOG_TABLE_NAME = "catalog";
constexpr std::string_view DISK_TABLE_MAPPING_FILE = "disk_table_mapping";
constexpr std::string_view DATABASE_FILE_EXTENSION = ".yadb";
constexpr std::string_view TABLE_FILE_EXTENSION = ".db";

// Page constants
constexpr page_id_t NULL_PAGE_ID = UINT32_MAX;
constexpr page_id_t ROOT_PAGE_ID = 0;

// File generation constants
constexpr uint32_t MAX_FILE_ID_RETRIES = 10;
