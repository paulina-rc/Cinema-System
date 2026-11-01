// =============================================================================
//  main.cpp - Servidor HTTP del Sistema Cine
//
//  Paso 5 de la guia: servidor minimo.
//   - Entrega las paginas de la carpeta frontend/ (HTML, CSS, JS).
//   - Responde la ruta de prueba GET /api/ping.
//
//  Se ejecuta desde la carpeta raiz del proyecto (usar iniciar.bat),
//  porque busca frontend/ y database/ de forma relativa.
// =============================================================================
#include <iostream>
#include <string>
#include "httplib.h"

int main(int argc, char* argv[]) {
    // Puerto: 8080 por defecto, o el que se pase al ejecutar (servidor.exe 3000)
    int puerto = (argc > 1) ? std::stoi(argv[1]) : 8080;

    httplib::Server svr;

    // Servir el frontend: http://localhost:8080/ -> frontend/index.html
    if (!svr.set_mount_point("/", "frontend")) {
        std::cerr << "No se encontro la carpeta frontend/. "
                     "Ejecute el servidor desde la carpeta raiz (iniciar.bat).\n";
        return 1;
    }

    // Ruta de prueba
    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"mensaje\": \"pong\"}", "application/json");
    });

    // Imprime en la consola cada peticion recibida
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << req.method << " " << req.path << " -> " << res.status << "\n";
    });

    std::cout << "Servidor en http://localhost:" << puerto << "\n";
    std::cout << "Ctrl + C para detenerlo.\n";

    if (!svr.listen("127.0.0.1", puerto)) {
        std::cerr << "No se pudo abrir el puerto " << puerto << "\n";
        return 1;
    }
    return 0;
}
