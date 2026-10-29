#pragma once

class Database;

class SalaService {
public:
    explicit SalaService(Database& database);
private:
    Database& database_;
};
