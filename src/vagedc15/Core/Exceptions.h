#pragma once
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace vagedc15 {

    class Error : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
    class FileError : public Error {
    public:
        FileError(const std::string& msg, const std::filesystem::path& path) : Error(msg + ": " + path.string()), _path(path) {}
        const std::filesystem::path& path() const noexcept { return _path; }
    private:
        std::filesystem::path _path;
    };

    class FileNotFoundError : public FileError {
    public:
        explicit FileNotFoundError(const std::filesystem::path& p) : FileError("File not found", p) {}
    };
}