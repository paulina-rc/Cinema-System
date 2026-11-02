// =============================================================================
//  database.h
//  -----------------------------------------------------------------------------
//  CAPA DE ACCESO A DATOS (igual que en el demo del profe).
//
//  Esta clase es la UNICA parte del programa que "habla" con SQLite.
//  Las rutas HTTP de main.cpp no escriben SQL: solo llaman a sus metodos.
// =============================================================================
#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "modelos.h"

// Declaracion adelantada: basta con saber que existe el tipo sqlite3.
struct sqlite3;

class Database {
public:
    // Abre el archivo de base de datos (debe existir: database/cine.db).
    explicit Database(const std::string& ruta_archivo);

    // Cierra la conexion automaticamente al destruirse el objeto (RAII).
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // ---------------- Salas ----------------
    // R: lista todas las salas, incluidas las inhabilitadas.
    std::vector<Sala> listarSalas();

private:
    sqlite3*   db_ = nullptr;   // "Conexion" a la base de datos
    std::mutex mutex_;          // Evita que dos peticiones (hilos) usen la
                                // conexion al mismo tiempo.

    // Ejecuta SQL que no devuelve filas (PRAGMA, CREATE TABLE, etc.)
    void ejecutar(const std::string& sql);
};
