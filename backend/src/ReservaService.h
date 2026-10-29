#pragma once

class Database;

class ReservaService {
public:
    explicit ReservaService(Database& database);
private:
    Database& database_;
};
