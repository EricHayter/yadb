#pragma once

#include <filesystem>
#include <memory>
#include "spdlog/logger.h"

struct DatabaseConfig {
    std::filesystem::path database_file { "data.db" };

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
