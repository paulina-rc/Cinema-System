// modelos.h
// un struct por tabla, cada to_json lo encuentra solo la libreria json
// y por eso en main.cpp se puede hacer responderJson(res, sala) directo
#pragma once

#include <string>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

struct Sala {
    int         id = 0;
    std::string nombre;
    int         agrupaciones = 0;           // filas de la matriz
    int         butacas_por_agrupacion = 0; // columnas de la matriz
    int         habilitada = 1;
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

// no tiene to_json a proposito, esto nunca se manda al navegador
struct Administrador {
    int         id = 0;
    std::string usuario;
    std::string sal;          // texto aleatorio distinto por usuario
    std::string clave_hash;   // hashClave(sal, contrasena), ver Seguridad.h
};

struct Butaca {
    int id = 0;
    int sala_id = 0;
    int agrupacion = 0;   // fila de la matriz, empieza en 0
    int numero = 0;       // columna de la matriz, empieza en 0
    int habilitada = 1;   // 0 es que esta danada
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

struct Funcion {
    int         id = 0;
    std::string hora;      // "HH:MM"
};

inline void to_json(json& j, const Funcion& f) {
    j = json{ {"funcion_id", f.id}, {"hora", f.hora} };
}

struct Pelicula {
    int         id = 0;
    std::string nombre;
    std::string fecha_inicio;   // "AAAA-MM-DD"
    std::string fecha_fin;      // "AAAA-MM-DD"
    double      precio = 0.0;
    int         sala_id = 0;
    std::string sala;           // nombre de la sala, viene del join
    std::string imagen;         // ruta del poster, "" si no tiene
    std::vector<Funcion> horarios;
};

// junta funcion + pelicula + sala en una sola consulta, es lo que se
// necesita para reservar
struct InfoFuncion {
    int         funcion_id = 0;
    std::string hora;                  // "HH:MM"
    int         pelicula_id = 0;
    std::string pelicula;
    double      precio = 0.0;
    int         sala_id = 0;
    std::string sala;
    int         agrupaciones = 0;
    int         butacas_por_agrupacion = 0;
    int         sala_habilitada = 1;
    int         en_cartelera = 0;      // 1 si hoy esta dentro de las fechas de exhibicion
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
        {"imagen",       p.imagen},
        {"horarios",     p.horarios}
    };
}
