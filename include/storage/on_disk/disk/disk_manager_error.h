#pragma once

#include "core/error.h"
#include "storage/on_disk/types.h"
#include <format>
#include <string>

namespace yadb {

class CreateFileError : public Error {
public:
    CreateFileError(file_id_t file_id, std::string_view reason)
        : file_id_m(file_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to create file {}: {}", file_id_m, reason_m);
    }

    file_id_t FileId() const noexcept { return file_id_m; }

private:
    file_id_t file_id_m;
    std::string reason_m;
};

class AllocatePageError : public Error {
public:
    AllocatePageError(file_id_t file_id, std::string_view reason)
        : file_id_m(file_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to allocate page in file {}: {}", file_id_m, reason_m);
    }

    file_id_t FileId() const noexcept { return file_id_m; }

private:
    file_id_t file_id_m;
    std::string reason_m;
};

class PageReadError : public Error {
public:
    PageReadError(file_page_id_t fp_id, std::string_view reason)
        : fp_id_m(fp_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to read page {} in file {}: {}",
            fp_id_m.page_id, fp_id_m.file_id, reason_m);
    }

    file_page_id_t FilePageId() const noexcept { return fp_id_m; }

private:
    file_page_id_t fp_id_m;
    std::string reason_m;
};

class PageWriteError : public Error {
public:
    PageWriteError(file_page_id_t fp_id, std::string_view reason)
        : fp_id_m(fp_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to write page {} in file {}: {}",
            fp_id_m.page_id, fp_id_m.file_id, reason_m);
    }

    file_page_id_t FilePageId() const noexcept { return fp_id_m; }

private:
    file_page_id_t fp_id_m;
    std::string reason_m;
};

class DeletePageError : public Error {
public:
    DeletePageError(file_page_id_t fp_id, std::string_view reason)
        : fp_id_m(fp_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to delete page {} in file {}: {}",
            fp_id_m.page_id, fp_id_m.file_id, reason_m);
    }

    file_page_id_t FilePageId() const noexcept { return fp_id_m; }

private:
    file_page_id_t fp_id_m;
    std::string reason_m;
};

class OpenFileError : public Error {
public:
    OpenFileError(file_id_t file_id, std::string_view reason)
        : file_id_m(file_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to open file {}: {}", file_id_m, reason_m);
    }

    file_id_t FileId() const noexcept { return file_id_m; }

private:
    file_id_t file_id_m;
    std::string reason_m;
};

class DeleteFileError : public Error {
public:
    DeleteFileError(file_id_t file_id, std::string_view reason)
        : file_id_m(file_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to delete file {}: {}", file_id_m, reason_m);
    }

    file_id_t FileId() const noexcept { return file_id_m; }

private:
    file_id_t file_id_m;
    std::string reason_m;
};

} // namespace yadb
