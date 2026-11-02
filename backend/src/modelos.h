// =============================================================================
//  modelos.h
//  -----------------------------------------------------------------------------
//  MODELOS DE DATOS: un struct por tabla de la base de datos.
//  Cada campo del struct corresponde a una columna.
//
//  Cada modelo tiene su funcion to_json(): nlohmann/json la busca sola, asi
//  en main.cpp basta con responderJson(res, sala) o responderJson(res, salas).
// =============================================================================
#pragma once

#include <string>
#include "json.hpp"

using json = nlohmann::json;

// ---------------------------------------------------------------- Tabla salas
struct Sala {
    int         id = 0;                     // Clave primaria (la asigna SQLite)
    std::string nombre;
    int         agrupaciones = 0;           // Filas de la matriz de butacas
    int         butacas_por_agrupacion = 0; // Columnas de la matriz de butacas
    int         habilitada = 1;             // 1 = activa, 0 = inhabilitada
};

inline void to_json(json& j, const Sala& s) {
    j = json{
        {"id",                     s.id},
        {"nombre",                 s.nombre},
        {"agrupaciones",           s.agrupaciones},
        {"butacas_por_agrupacion", s.butacas_por_agrupacion},
        {"habilitada",             s.habilitada}
    };
}
