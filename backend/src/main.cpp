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
#include <regex>
#include <set>
#include <string>
#include <vector>

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

// ---- Fechas y horas -------------------------------------------------------
// Fecha valida en formato AAAA-MM-DD (revisa tambien que el dia exista:
// no acepta 2026-02-30).
bool fechaValida(const std::string& f) {
    static const std::regex formato(R"(^\d{4}-\d{2}-\d{2}$)");
    if (!std::regex_match(f, formato)) return false;

    int anio = std::stoi(f.substr(0, 4));
    int mes  = std::stoi(f.substr(5, 2));
    int dia  = std::stoi(f.substr(8, 2));
    if (anio < 2000 || anio > 2100 || mes < 1 || mes > 12 || dia < 1) return false;

    int dias_mes[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool bisiesto = (anio % 4 == 0 && anio % 100 != 0) || anio % 400 == 0;
    if (bisiesto) dias_mes[1] = 29;
    return dia <= dias_mes[mes - 1];
}

// Hora valida en formato HH:MM de 24 horas (00:00 a 23:59).
bool horaValida(const std::string& h) {
    static const std::regex formato(R"(^([01]\d|2[0-3]):[0-5]\d$)");
    return std::regex_match(h, formato);
}

// Revisa los datos de una pelicula nueva. Devuelve "" si todo esta bien.
// La sala se valida aparte en la ruta, porque hay que consultar la BD.
std::string validarPelicula(const json& j, Pelicula& p, std::vector<std::string>& horarios) {
    if (!j.contains("nombre") || !j["nombre"].is_string())
        return "Falta el nombre de la pelicula";
    if (!j.contains("fecha_inicio") || !j["fecha_inicio"].is_string() ||
        !j.contains("fecha_fin")    || !j["fecha_fin"].is_string())
        return "Faltan las fechas de exhibicion";
    if (!j.contains("precio") || !j["precio"].is_number())
        return "El precio debe ser un numero";
    if (!j.contains("sala_id") || !j["sala_id"].is_number_integer())
        return "Debe seleccionar una sala";
    if (!j.contains("horarios") || !j["horarios"].is_array())
        return "Faltan los horarios";

    p.nombre       = j["nombre"].get<std::string>();
    p.fecha_inicio = j["fecha_inicio"].get<std::string>();
    p.fecha_fin    = j["fecha_fin"].get<std::string>();
    p.precio       = j["precio"].get<double>();
    p.sala_id      = j["sala_id"].get<int>();

    p.nombre.erase(0, p.nombre.find_first_not_of(" \t"));
    p.nombre.erase(p.nombre.find_last_not_of(" \t") + 1);
    if (p.nombre.empty())
        return "El nombre de la pelicula no puede estar vacio";

    if (!fechaValida(p.fecha_inicio))
        return "La fecha de inicio no es valida (use AAAA-MM-DD)";
    if (!fechaValida(p.fecha_fin))
        return "La fecha de finalizacion no es valida (use AAAA-MM-DD)";
    // En formato AAAA-MM-DD el orden de texto es el orden de las fechas.
    if (p.fecha_fin < p.fecha_inicio)
        return "La fecha de finalizacion no puede ser antes de la fecha de inicio";

    if (p.precio <= 0)
        return "El precio debe ser mayor que 0";

    if (j["horarios"].empty())
        return "Debe agregar al menos un horario";

    std::set<std::string> vistos;   // para detectar horarios repetidos
    for (const auto& h : j["horarios"]) {
        if (!h.is_string() || !horaValida(h.get<std::string>()))
            return "Hay un horario que no es valido (use HH:MM, por ejemplo 19:30)";
        std::string hora = h.get<std::string>();
        if (!vistos.insert(hora).second)
            return "El horario " + hora + " esta repetido";
        horarios.push_back(hora);
    }
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

        // Peliculas: todas, para el administrador (GET /api/peliculas)
        svr.Get("/api/peliculas", [&](const httplib::Request&, httplib::Response& res) {
            responderJson(res, db.listarPeliculas(false));
        });

        // Cartelera: solo las que se exhiben hoy (GET /api/cartelera)
        svr.Get("/api/cartelera", [&](const httplib::Request&, httplib::Response& res) {
            responderJson(res, db.listarPeliculas(true));
        });

        // Peliculas: registrar (POST /api/peliculas)
        svr.Post("/api/peliculas", [&](const httplib::Request& req, httplib::Response& res) {
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");

            Pelicula p;
            std::vector<std::string> horarios;
            std::string error = validarPelicula(j, p, horarios);
            if (!error.empty()) return responderError(res, 400, error);

            // La sala tiene que existir y estar habilitada.
            auto sala = db.obtenerSala(p.sala_id);
            if (!sala)                 return responderError(res, 400, "La sala seleccionada no existe");
            if (sala->habilitada == 0) return responderError(res, 400, "La sala seleccionada esta inhabilitada");

            int id = db.crearPelicula(p, horarios);
            responderJson(res, json{{"ok", true}, {"id", id}}, 201);
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
