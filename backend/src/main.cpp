// =============================================================================
//  main.cpp - Servidor HTTP del Sistema Cine
//
//  Hace cuatro cosas en orden (igual que el demo):
//    1. Abre la base de datos database/cine.db
//    2. Crea el servidor y sirve las paginas de frontend/
//    3. Registra las rutas de la API (ver docs/API.md)
//    4. Se queda escuchando en http://localhost:8080
//
//  Se ejecuta desde la carpeta raiz del proyecto (usar iniciar.bat).
// =============================================================================
#include <iostream>
#include <string>

#include "httplib.h"
#include "json.hpp"
#include "Database.h"

using json = nlohmann::json;

// -----------------------------------------------------------------------------
//  Ayudantes para responder (mismo formato que el demo)
// -----------------------------------------------------------------------------
template <typename T>
void responderJson(httplib::Response& res, const T& datos, int estado = 200) {
    res.status = estado;
    res.set_content(json(datos).dump(), "application/json");
}

void responderError(httplib::Response& res, int estado, const std::string& mensaje) {
    responderJson(res, json{{"error", mensaje}}, estado);
}

// -----------------------------------------------------------------------------
//  Validaciones (el servidor NUNCA confia en lo que manda el navegador)
// -----------------------------------------------------------------------------
const int MAX_AGRUPACIONES = 26;   // A..Z, para poder nombrar las filas con letras
const int MAX_BUTACAS      = 30;

// Lee el JSON del cuerpo. Si no es valido o no es un objeto, devuelve false.
bool leerCuerpo(const httplib::Request& req, json& j) {
    j = json::parse(req.body, nullptr, false);   // no lanza excepcion
    return !j.is_discarded() && j.is_object();
}

// Revisa los datos de una sala nueva. Devuelve "" si todo esta bien,
// o el mensaje de error para el usuario.
std::string validarSala(const json& j, std::string& nombre, int& agrupaciones, int& por_agrupacion) {
    if (!j.contains("nombre") || !j["nombre"].is_string())
        return "Falta el nombre de la sala";
    if (!j.contains("agrupaciones") || !j["agrupaciones"].is_number_integer())
        return "La cantidad de agrupaciones debe ser un numero entero";
    if (!j.contains("butacas_por_agrupacion") || !j["butacas_por_agrupacion"].is_number_integer())
        return "La cantidad de butacas por agrupacion debe ser un numero entero";

    nombre         = j["nombre"].get<std::string>();
    agrupaciones   = j["agrupaciones"].get<int>();
    por_agrupacion = j["butacas_por_agrupacion"].get<int>();

    // Quitar espacios al inicio y al final del nombre
    nombre.erase(0, nombre.find_first_not_of(" \t"));
    nombre.erase(nombre.find_last_not_of(" \t") + 1);

    if (nombre.empty())
        return "El nombre de la sala no puede estar vacio";
    if (agrupaciones < 1 || agrupaciones > MAX_AGRUPACIONES)
        return "Las agrupaciones deben estar entre 1 y " + std::to_string(MAX_AGRUPACIONES);
    if (por_agrupacion < 1 || por_agrupacion > MAX_BUTACAS)
        return "Las butacas por agrupacion deben estar entre 1 y " + std::to_string(MAX_BUTACAS);
    return "";
}

// Para las rutas .../habilitada: el cuerpo debe ser {"habilitada": 0} o {"habilitada": 1}
std::string validarHabilitada(const json& j, int& habilitada) {
    if (!j.contains("habilitada") || !j["habilitada"].is_number_integer())
        return "Falta el campo habilitada (0 o 1)";
    habilitada = j["habilitada"].get<int>();
    if (habilitada != 0 && habilitada != 1)
        return "habilitada solo puede ser 0 o 1";
    return "";
}

int main(int argc, char* argv[]) {
    int puerto = (argc > 1) ? std::stoi(argv[1]) : 8080;

    // ---------------------------------------------------------------- 1. BD
    try {
        static Database db("database/cine.db");

        // ------------------------------------------------------- 2. Servidor
        httplib::Server svr;

        if (!svr.set_mount_point("/", "frontend")) {
            std::cerr << "No se encontro la carpeta frontend/. "
                         "Ejecute el servidor desde la carpeta raiz (iniciar.bat).\n";
            return 1;
        }

        // ------------------------------------------------------- 3. Rutas
        // Prueba
        svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
            responderJson(res, json{{"mensaje", "pong"}});
        });

        // Salas: lista todas (GET /api/salas)
        svr.Get("/api/salas", [&](const httplib::Request&, httplib::Response& res) {
            responderJson(res, db.listarSalas());
        });

        // Salas: crear (POST /api/salas) -> genera tambien las butacas
        svr.Post("/api/salas", [&](const httplib::Request& req, httplib::Response& res) {
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");

            std::string nombre;
            int agrupaciones = 0, por_agrupacion = 0;
            std::string error = validarSala(j, nombre, agrupaciones, por_agrupacion);
            if (!error.empty()) return responderError(res, 400, error);

            auto id = db.crearSala(nombre, agrupaciones, por_agrupacion);
            if (!id) return responderError(res, 400, "Ya existe una sala llamada " + nombre);

            responderJson(res, json{{"ok", true}, {"id", *id}}, 201);
        });

        // Salas: habilitar / inhabilitar (PUT /api/salas/{id}/habilitada)
        svr.Put(R"(/api/salas/(\d+)/habilitada)", [&](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");

            int habilitada = 0;
            std::string error = validarHabilitada(j, habilitada);
            if (!error.empty()) return responderError(res, 400, error);

            if (!db.cambiarHabilitadaSala(id, habilitada))
                return responderError(res, 404, "Sala no encontrada");
            responderJson(res, json{{"ok", true}});
        });

        // Butacas de una sala (GET /api/salas/{id}/butacas)
        svr.Get(R"(/api/salas/(\d+)/butacas)", [&](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            auto sala = db.obtenerSala(id);
            if (!sala) return responderError(res, 404, "Sala no encontrada");

            responderJson(res, json{
                {"agrupaciones",           sala->agrupaciones},
                {"butacas_por_agrupacion", sala->butacas_por_agrupacion},
                {"butacas",                db.listarButacas(id)}
            });
        });

        // Butaca: marcar danada / reparada (PUT /api/butacas/{id}/habilitada)
        svr.Put(R"(/api/butacas/(\d+)/habilitada)", [&](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");

            int habilitada = 0;
            std::string error = validarHabilitada(j, habilitada);
            if (!error.empty()) return responderError(res, 400, error);

            if (!db.cambiarHabilitadaButaca(id, habilitada))
                return responderError(res, 404, "Butaca no encontrada");
            responderJson(res, json{{"ok", true}});
        });

        // Si una ruta lanza una excepcion (por ejemplo un error de SQLite),
        // se responde 500 en vez de cerrar el servidor.
        svr.set_exception_handler([](const httplib::Request&, httplib::Response& res,
                                     std::exception_ptr ep) {
            std::string msg = "Error interno";
            try { std::rethrow_exception(ep); }
            catch (const std::exception& e) { msg = e.what(); }
            catch (...) {}
            std::cerr << "[ERROR] " << msg << "\n";
            responderError(res, 500, msg);
        });

        // Imprime en la consola cada peticion recibida
        svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
            std::cout << req.method << " " << req.path << " -> " << res.status << "\n";
        });

        // ------------------------------------------------------- 4. Escuchar
        std::cout << "Servidor en http://localhost:" << puerto << "\n";
        std::cout << "Ctrl + C para detenerlo.\n";
        if (!svr.listen("127.0.0.1", puerto)) {
            std::cerr << "No se pudo abrir el puerto " << puerto << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
