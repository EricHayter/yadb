#pragma once

#include "spdlog/logger.h"
#include <filesystem>
#include <memory>
#include <string>

struct DatabaseConfig {
    static inline const std::filesystem::path DEFAULT_DATABASE_FILE { "data.db" };

    static inline const std::string DEFAULT_DISK_MANAGER_LOGGER_NAME { "disk_manager" };
    static inline const std::string DEFAULT_DISK_SCHEDULER_LOGGER_NAME { "disk_scheduler" };
    static inline const std::string DEFAULT_PAGE_BUFFER_MANAGER_LOGGER_NAME { "page_buffer_manager" };

    std::filesystem::path database_file { DEFAULT_DATABASE_FILE };

    std::shared_ptr<spdlog::logger> disk_manager_logger;
    std::shared_ptr<spdlog::logger> disk_scheduler_logger;
    std::shared_ptr<spdlog::logger> page_buffer_manager_logger;

    /**
     * \brief Create database config for writing logs to console
     *
     * \returns a database config for console logging
     */
    static DatabaseConfig CreateDefaultConsole();

    /**
     * \brief Create database config for writing logs to file
     *
     * \returns a database config for file logging
     */
    static DatabaseConfig CreateDefaultFile();

    /**
     * \brief Create database config that logs no output anywhere
     *
     * \returns a database config for no logging
     */
    static DatabaseConfig CreateNull();
};
