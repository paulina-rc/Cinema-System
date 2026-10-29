#pragma once

class Database;

class PeliculaService {
public:
    explicit PeliculaService(Database& database);
private:
    Database& database_;
};
