// Database.h
// la unica clase que habla con sqlite, las rutas de main.cpp no escriben
// sql, solo llaman estos metodos
#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "modelos.h"

// con esto alcanza para no incluir sqlite3.h en el header
struct sqlite3;

class Database {
public:
    // el archivo tiene que existir, no lo crea
    explicit Database(const std::string& ruta_archivo);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // trae tambien las inhabilitadas
    std::vector<Sala> listarSalas();

    std::optional<Sala> obtenerSala(int id);

    // crea tambien todas las butacas, nullopt si ya hay una sala con ese nombre
    std::optional<int> crearSala(const std::string& nombre, int agrupaciones,
                                 int butacas_por_agrupacion);

    // false si no existe
    bool cambiarHabilitadaSala(int id, int habilitada);

    std::vector<Butaca> listarButacas(int sala_id);

    // false si no existe
    bool cambiarHabilitadaButaca(int id, int habilitada);

    // con solo_cartelera trae solo las que se exhiben hoy en una sala habilitada
    std::vector<Pelicula> listarPeliculas(bool solo_cartelera = false);

    // inserta la pelicula y una funcion por horario, devuelve el id nuevo
    int crearPelicula(const Pelicula& p, const std::vector<std::string>& horarios);

    std::optional<Administrador> obtenerAdmin(const std::string& usuario);

    int contarAdmins();

    // si el usuario ya existe solo le cambia la clave
    void guardarAdmin(const std::string& usuario, const std::string& sal,
                      const std::string& clave_hash);

    // devuelve el id nuevo
    int crearCliente(const std::string& nombre, const std::string& telefono);

    bool existeCliente(int id);

    // la fecha la da sqlite para que sea la misma que usa en las consultas,
    // las reservas se guardan por dia porque la misma funcion se repite todos los dias
    std::string fechaHoy();

    std::optional<InfoFuncion> obtenerFuncion(int funcion_id);

    // llena matriz con 0 disponible, 1 ocupada, 2 danada, y en ids deja el id
    // de la butaca de cada posicion para poder insertar despues
    void cargarMatriz(const InfoFuncion& f, const std::string& fecha,
                      std::vector<std::vector<int>>& matriz,
                      std::vector<std::vector<int>>& ids);

    // vuelve a revisar que las butacas sigan libres antes de guardar, si alguna
    // ya no lo esta no guarda ninguna y devuelve los ids con problema,
    // vector vacio es que todo salio bien
    std::vector<int> confirmarReserva(int funcion_id, int cliente_id,
                                      const std::string& fecha,
                                      const std::vector<int>& butaca_ids);

private:
    sqlite3*   db_ = nullptr;
    std::mutex mutex_;          // cada peticion corre en su hilo y la conexion es una sola

    // para sql que no devuelve filas
    void ejecutar(const std::string& sql);
};
