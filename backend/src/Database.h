#pragma once

#include <string>

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool isOpen() const noexcept;

private:
    struct Impl;
    Impl* impl_;
};
