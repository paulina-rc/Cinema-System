// =============================================================================
//  Database.h
//  -----------------------------------------------------------------------------
//  CAPA DE ACCESO A DATOS (igual que en el demo del profe).
//
//  Esta clase es la UNICA parte del programa que "habla" con SQLite.
//  Las rutas HTTP de main.cpp no escriben SQL: solo llaman a sus metodos.
// =============================================================================
#pragma once

#include <mutex>
#include <optional>
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

    // R: busca una sala por id. Si no existe devuelve std::nullopt.
    std::optional<Sala> obtenerSala(int id);

    // C: crea la sala Y genera todas sus butacas (una por posicion de la
    //    matriz agrupaciones x butacas_por_agrupacion).
    //    Devuelve el id nuevo, o std::nullopt si ya existe una sala con ese nombre.
    std::optional<int> crearSala(const std::string& nombre, int agrupaciones,
                                 int butacas_por_agrupacion);

    // U: habilita (1) o inhabilita (0) una sala sin borrarla.
    //    Devuelve false si la sala no existe.
    bool cambiarHabilitadaSala(int id, int habilitada);

    // ---------------- Butacas ----------------
    // R: todas las butacas de una sala, ordenadas como la matriz.
    std::vector<Butaca> listarButacas(int sala_id);

    // U: marca una butaca como danada (0) o la repara (1).
    //    Devuelve false si la butaca no existe.
    bool cambiarHabilitadaButaca(int id, int habilitada);

private:
    sqlite3*   db_ = nullptr;   // "Conexion" a la base de datos
    std::mutex mutex_;          // Evita que dos peticiones (hilos) usen la
                                // conexion al mismo tiempo.

    // Ejecuta SQL que no devuelve filas (PRAGMA, CREATE TABLE, etc.)
    void ejecutar(const std::string& sql);
};
