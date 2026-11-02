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
#include "database.h"

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
