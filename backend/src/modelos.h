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
#include <vector>
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

// -------------------------------------------------------------- Tabla butacas
// (agrupacion, numero) = (fila, columna) de la matriz de la sala.
struct Butaca {
    int id = 0;
    int sala_id = 0;
    int agrupacion = 0;   // Empieza en 0
    int numero = 0;       // Empieza en 0
    int habilitada = 1;   // 1 = sirve, 0 = danada / fuera de servicio
};

inline void to_json(json& j, const Butaca& b) {
    j = json{
        {"id",         b.id},
        {"sala_id",    b.sala_id},
        {"agrupacion", b.agrupacion},
        {"numero",     b.numero},
        {"habilitada", b.habilitada}
    };
}

// ------------------------------------------------------------ Tabla funciones
// Un horario de una pelicula. funcion_id es lo que se usa al reservar.
struct Funcion {
    int         id = 0;
    std::string hora;      // "HH:MM"
};

inline void to_json(json& j, const Funcion& f) {
    j = json{ {"funcion_id", f.id}, {"hora", f.hora} };
}

// ------------------------------------------------------------ Tabla peliculas
struct Pelicula {
    int         id = 0;
    std::string nombre;
    std::string fecha_inicio;   // "AAAA-MM-DD"
    std::string fecha_fin;      // "AAAA-MM-DD"
    double      precio = 0.0;
    int         sala_id = 0;
    std::string sala;           // Nombre de la sala (viene del JOIN con salas)
    std::vector<Funcion> horarios;
};

inline void to_json(json& j, const Pelicula& p) {
    j = json{
        {"id",           p.id},
        {"nombre",       p.nombre},
        {"fecha_inicio", p.fecha_inicio},
        {"fecha_fin",    p.fecha_fin},
        {"precio",       p.precio},
        {"sala_id",      p.sala_id},
        {"sala",         p.sala},
        {"horarios",     p.horarios}
    };
}
