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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <thread>
#include <string>
#include <vector>

#include "httplib.h"
#include "json.hpp"
#include "Database.h"
#include "ReservaTemporal.h"
#include "Seguridad.h"

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
const int MAX_AGRUPACIONES = 10;   // A..J: cada agrupacion (bloque) se nombra con una letra
const int MAX_BUTACAS      = 60;   // butacas por agrupacion
const int MAX_POR_RESERVA  = 10;   // butacas que un cliente puede reservar de una vez

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

// ---- Poster de la pelicula -------------------------------------------------
// El navegador manda la imagen dentro del JSON como texto "data URL":
//     "data:image/jpeg;base64,/9j/4AAQSk..."
// Aqui se convierte de base64 a bytes y se guarda como archivo en
// frontend/img/peliculas/. En la base de datos solo queda la RUTA.
const std::string CARPETA_POSTERS = "img/peliculas/";        // relativa a frontend/
const size_t      MAX_IMAGEN      = 3 * 1024 * 1024;         // 3 MB en base64

// Base64 -> bytes. Devuelve false si el texto trae caracteres invalidos.
bool decodificarBase64(const std::string& texto, std::string& bytes) {
    static const std::string tabla =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int valor = 0, bits = 0;
    bytes.clear();
    for (char c : texto) {
        if (c == '=') break;                      // relleno del final
        size_t pos = tabla.find(c);
        if (pos == std::string::npos) return false;
        valor = (valor << 6) | static_cast<int>(pos);   // cada caracter son 6 bits
        bits += 6;
        if (bits >= 8) {                          // ya se junto un byte completo
            bits -= 8;
            bytes.push_back(static_cast<char>((valor >> bits) & 0xFF));
        }
    }
    return true;
}

// Guarda el poster y deja en `ruta` lo que se guarda en la BD.
// Devuelve "" si todo salio bien, o el mensaje de error.
std::string guardarPoster(const std::string& data_url, std::string& ruta) {
    // Solo se aceptan JPEG y PNG (el navegador siempre manda JPEG)
    std::string extension;
    std::string prefijo;
    if (data_url.rfind("data:image/jpeg;base64,", 0) == 0) { prefijo = "data:image/jpeg;base64,"; extension = ".jpg"; }
    else if (data_url.rfind("data:image/png;base64,", 0) == 0) { prefijo = "data:image/png;base64,"; extension = ".png"; }
    else return "La imagen debe ser JPG o PNG";

    if (data_url.size() > MAX_IMAGEN)
        return "La imagen es demasiado grande";

    std::string bytes;
    if (!decodificarBase64(data_url.substr(prefijo.size()), bytes) || bytes.empty())
        return "La imagen no es valida";

    // Nombre unico con la hora actual en milisegundos
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    ruta = CARPETA_POSTERS + "pelicula-" + std::to_string(ms) + extension;

    std::error_code ec;
    std::filesystem::create_directories("frontend/" + CARPETA_POSTERS, ec);
    std::ofstream archivo("frontend/" + ruta, std::ios::binary);
    if (!archivo) return "No se pudo guardar la imagen en el servidor";
    archivo.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return "";
}

// ---- Reservacion -----------------------------------------------------------
// Quita espacios al inicio y al final.
std::string recortar(std::string t) {
    t.erase(0, t.find_first_not_of(" \t"));
    t.erase(t.find_last_not_of(" \t") + 1);
    return t;
}

// Paso 1: nombre y telefono del cliente. El telefono se guarda solo con
// digitos (se aceptan "8888 8888" o "8888-8888" y se guardan como "88888888").
std::string validarCliente(const json& j, std::string& nombre, std::string& telefono) {
    if (!j.contains("nombre") || !j["nombre"].is_string())
        return "Falta el nombre";
    if (!j.contains("telefono") || !j["telefono"].is_string())
        return "Falta el numero de telefono";

    nombre = recortar(j["nombre"].get<std::string>());
    if (nombre.size() < 3)  return "Escriba el nombre completo";
    if (nombre.size() > 60) return "El nombre es demasiado largo";

    telefono.clear();
    for (char c : j["telefono"].get<std::string>()) {
        if (c == ' ' || c == '-') continue;
        if (c < '0' || c > '9') return "El telefono solo puede tener numeros";
        telefono += c;
    }
    if (telefono.size() != 8) return "Ingrese un numero de 8 digitos";
    return "";
}

// Una funcion solo se puede reservar si existe, su pelicula esta en
// exhibicion HOY y su sala esta habilitada. Devuelve "" si se puede.
std::string validarFuncionReservable(const std::optional<InfoFuncion>& f) {
    if (!f)                     return "La funcion no existe";
    if (f->sala_habilitada == 0) return "La sala de esta funcion esta inhabilitada";
    if (f->en_cartelera == 0)    return "Esta pelicula no esta en exhibicion hoy";
    return "";
}

// Cuenta las posiciones con estado DISPONIBLE (0) en la matriz.
int contarDisponibles(const std::vector<std::vector<int>>& matriz) {
    int total = 0;
    for (const auto& fila : matriz)
        for (int estado : fila)
            if (estado == DISPONIBLE) total++;
    return total;
}

// ---- Login del administrador -----------------------------------------------
// Usuario que se crea solo la primera vez (si la tabla administradores esta vacia).
// Cambie la contrasena con:  build\servidor.exe --cambiar-clave admin NUEVA_CLAVE
const std::string ADMIN_INICIAL = "admin";
const std::string CLAVE_INICIAL = "imperium2026";
const size_t      MIN_CLAVE     = 8;

// Saca el token del encabezado "Authorization: Bearer <token>".
std::string tokenDe(const httplib::Request& req) {
    const std::string prefijo = "Bearer ";
    std::string h = req.get_header_value("Authorization");
    return h.rfind(prefijo, 0) == 0 ? h.substr(prefijo.size()) : "";
}

// Guarda (o cambia) la contrasena de un administrador con una sal nueva.
void guardarClave(Database& db, const std::string& usuario, const std::string& clave) {
    std::string sal = textoAleatorio(16);
    db.guardarAdmin(usuario, sal, hashClave(sal, clave));
}

int main(int argc, char* argv[]) {
    // Modo especial: cambiar la contrasena sin arrancar el servidor
    //   servidor.exe --cambiar-clave <usuario> <clave nueva>
    if (argc > 1 && std::string(argv[1]) == "--cambiar-clave") {
        if (argc != 4) {
            std::cerr << "Uso: servidor.exe --cambiar-clave <usuario> <clave nueva>\n";
            return 1;
        }
        if (std::string(argv[3]).size() < MIN_CLAVE) {
            std::cerr << "La clave debe tener al menos " << MIN_CLAVE << " caracteres.\n";
            return 1;
        }
        try {
            Database db("database/cine.db");
            guardarClave(db, argv[2], argv[3]);
            std::cout << "Clave del usuario '" << argv[2] << "' guardada.\n";
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] " << e.what() << "\n";
            return 1;
        }
    }

    int puerto = (argc > 1) ? std::stoi(argv[1]) : 8080;

    // ---------------------------------------------------------------- 1. BD
    try {
        static Database db("database/cine.db");

        // Primer arranque: si no hay ningun administrador se crea el inicial.
        if (db.contarAdmins() == 0) {
            guardarClave(db, ADMIN_INICIAL, CLAVE_INICIAL);
            std::cout << "Se creo el administrador inicial -> usuario: " << ADMIN_INICIAL
                      << "  clave: " << CLAVE_INICIAL << "\n"
                      << "Cambiela con: build\\servidor.exe --cambiar-clave "
                      << ADMIN_INICIAL << " NUEVA_CLAVE\n";
        }

        // Sesiones abiertas del panel admin (viven en memoria)
        static SesionesAdmin sesiones;

        // Revisa que la peticion traiga un token de sesion valido.
        // Si no, responde 401 y devuelve false (la ruta debe terminar ahi).
        auto esAdmin = [&](const httplib::Request& req, httplib::Response& res) {
            if (sesiones.usuario(tokenDe(req)).empty()) {
                responderError(res, 401, "Debe iniciar sesion como administrador");
                return false;
            }
            return true;
        };

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

        // =====================================================================
        //  LOGIN DEL ADMINISTRADOR
        // =====================================================================
        // Entrar (POST /api/login) {"usuario": "...", "clave": "..."}
        svr.Post("/api/login", [&](const httplib::Request& req, httplib::Response& res) {
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");
            if (!j.contains("usuario") || !j["usuario"].is_string() ||
                !j.contains("clave")   || !j["clave"].is_string())
                return responderError(res, 400, "Escriba el usuario y la contrasena");

            std::string usuario = recortar(j["usuario"].get<std::string>());
            std::string clave   = j["clave"].get<std::string>();

            auto admin = db.obtenerAdmin(usuario);
            bool correcto = admin && igualesSeguro(hashClave(admin->sal, clave), admin->clave_hash);
            if (!correcto) {
                // Pequena espera para que adivinar contrasenas sea lento.
                // El mensaje no dice si fallo el usuario o la clave.
                std::this_thread::sleep_for(std::chrono::milliseconds(600));
                return responderError(res, 401, "Usuario o contrasena incorrectos");
            }

            responderJson(res, json{{"ok", true}, {"token", sesiones.crear(admin->usuario)},
                                    {"usuario", admin->usuario}});
        });

        // Revisar si la sesion sigue abierta (GET /api/sesion)
        svr.Get("/api/sesion", [&](const httplib::Request& req, httplib::Response& res) {
            std::string usuario = sesiones.usuario(tokenDe(req));
            if (usuario.empty()) return responderError(res, 401, "Sesion vencida o inexistente");
            responderJson(res, json{{"ok", true}, {"usuario", usuario}});
        });

        // Salir (POST /api/logout)
        svr.Post("/api/logout", [&](const httplib::Request& req, httplib::Response& res) {
            sesiones.cerrar(tokenDe(req));
            responderJson(res, json{{"ok", true}});
        });

        // Salas: lista todas (GET /api/salas)
        svr.Get("/api/salas", [&](const httplib::Request&, httplib::Response& res) {
            responderJson(res, db.listarSalas());
        });

        // Salas: crear (POST /api/salas) -> genera tambien las butacas
        svr.Post("/api/salas", [&](const httplib::Request& req, httplib::Response& res) {
            if (!esAdmin(req, res)) return;   // solo el administrador
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
            if (!esAdmin(req, res)) return;   // solo el administrador
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
            if (!esAdmin(req, res)) return;   // solo el administrador
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
            if (!esAdmin(req, res)) return;   // solo el administrador
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

            // Poster (opcional): se guarda como archivo antes de insertar
            if (j.contains("imagen") && j["imagen"].is_string() && !j["imagen"].get<std::string>().empty()) {
                std::string error_imagen = guardarPoster(j["imagen"].get<std::string>(), p.imagen);
                if (!error_imagen.empty()) return responderError(res, 400, error_imagen);
            }

            int id = 0;
            try {
                id = db.crearPelicula(p, horarios);
            } catch (...) {
                // Si la BD fallo, no dejar el archivo huerfano
                if (!p.imagen.empty()) {
                    std::error_code ec;
                    std::filesystem::remove("frontend/" + p.imagen, ec);
                }
                throw;
            }
            responderJson(res, json{{"ok", true}, {"id", id}, {"imagen", p.imagen}}, 201);
        });

        // =====================================================================
        //  RESERVACION (Modulos 3 y 4)
        // =====================================================================
        // Aqui viven las matrices de las reservaciones que estan en proceso.
        static ReservasEnMemoria reservas;

        // Paso 1: registrar al cliente (POST /api/clientes)
        svr.Post("/api/clientes", [&](const httplib::Request& req, httplib::Response& res) {
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");

            std::string nombre, telefono;
            std::string error = validarCliente(j, nombre, telefono);
            if (!error.empty()) return responderError(res, 400, error);

            int id = db.crearCliente(nombre, telefono);
            responderJson(res, json{{"ok", true}, {"cliente_id", id}}, 201);
        });

        // Paso 3: cuantas butacas quedan libres hoy en una funcion
        // (GET /api/funciones/{id}/disponibles)
        svr.Get(R"(/api/funciones/(\d+)/disponibles)", [&](const httplib::Request& req, httplib::Response& res) {
            auto f = db.obtenerFuncion(std::stoi(req.matches[1]));
            std::string error = validarFuncionReservable(f);
            if (!error.empty()) return responderError(res, f ? 400 : 404, error);

            std::vector<std::vector<int>> matriz, ids;
            db.cargarMatriz(*f, db.fechaHoy(), matriz, ids);
            responderJson(res, json{{"disponibles", contarDisponibles(matriz)}});
        });

        // Pasos 3 y 4: iniciar la reservacion temporal (POST /api/reserva/iniciar)
        // Consulta SQLite, carga la matriz en memoria y devuelve un token.
        svr.Post("/api/reserva/iniciar", [&](const httplib::Request& req, httplib::Response& res) {
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");
            if (!j.contains("cliente_id") || !j["cliente_id"].is_number_integer() ||
                !j.contains("funcion_id") || !j["funcion_id"].is_number_integer() ||
                !j.contains("cantidad")   || !j["cantidad"].is_number_integer())
                return responderError(res, 400, "Faltan cliente_id, funcion_id o cantidad");

            int cliente_id = j["cliente_id"].get<int>();
            int funcion_id = j["funcion_id"].get<int>();
            int cantidad   = j["cantidad"].get<int>();

            if (!db.existeCliente(cliente_id))
                return responderError(res, 404, "Cliente no encontrado. Vuelva al paso 1");

            auto f = db.obtenerFuncion(funcion_id);
            std::string error = validarFuncionReservable(f);
            if (!error.empty()) return responderError(res, f ? 400 : 404, error);

            if (cantidad < 1 || cantidad > MAX_POR_RESERVA)
                return responderError(res, 400, "Puede reservar de 1 a " +
                                      std::to_string(MAX_POR_RESERVA) + " butacas");

            // Cargar la matriz con lo que YA esta guardado en SQLite.
            ReservaTemporal r;
            r.cliente_id = cliente_id;
            r.funcion    = *f;
            r.fecha      = db.fechaHoy();
            r.cantidad   = cantidad;
            db.cargarMatriz(*f, r.fecha, r.matriz, r.ids);

            int disponibles = contarDisponibles(r.matriz);
            if (cantidad > disponibles)
                return responderError(res, 400, "Solo quedan " + std::to_string(disponibles) +
                                      " butacas disponibles en esta funcion");

            json respuesta = {
                {"ok", true},
                {"cantidad", cantidad},
                {"matriz", r.matriz},
                {"agrupaciones", f->agrupaciones},
                {"butacas_por_agrupacion", f->butacas_por_agrupacion}
            };
            respuesta["token"] = reservas.crear(std::move(r));
            responderJson(res, respuesta, 201);
        });

        // Paso 4: marcar o desmarcar una butaca (POST /api/reserva/{token}/marcar)
        // Solo cambia la matriz en memoria: 0 -> 3 o 3 -> 0. No toca SQLite.
        svr.Post(R"(/api/reserva/(\w+)/marcar)", [&](const httplib::Request& req, httplib::Response& res) {
            std::string token = req.matches[1];
            json j;
            if (!leerCuerpo(req, j)) return responderError(res, 400, "JSON invalido");
            if (!j.contains("agrupacion") || !j["agrupacion"].is_number_integer() ||
                !j.contains("numero")     || !j["numero"].is_number_integer())
                return responderError(res, 400, "Faltan agrupacion y numero");

            int a = j["agrupacion"].get<int>();
            int n = j["numero"].get<int>();

            int codigo = 200;
            std::string error;
            json respuesta;

            bool existe = reservas.usar(token, [&](ReservaTemporal& r) {
                // La posicion tiene que estar dentro de la matriz.
                if (a < 0 || a >= (int)r.matriz.size() || n < 0 || n >= (int)r.matriz[a].size()) {
                    codigo = 400; error = "Esa butaca no existe en la sala"; return;
                }
                int& estado = r.matriz[a][n];

                if (estado == OCUPADA || estado == DANADA) {
                    codigo = 409; error = "La butaca " + etiquetaButaca(a, n) + " no esta disponible"; return;
                }
                if (estado == DISPONIBLE) {
                    if (r.seleccionadas >= r.cantidad) {
                        codigo = 400;
                        error = "Ya eligio las " + std::to_string(r.cantidad) +
                                " butacas. Desmarque una para cambiarla";
                        return;
                    }
                    estado = SELECCIONADA;          // nueva seleccion del cliente
                    r.seleccionadas++;
                } else {                            // estaba en 3: se desmarca
                    estado = DISPONIBLE;
                    r.seleccionadas--;
                }
                respuesta = {
                    {"ok", true},
                    {"estado", estado},
                    {"seleccionadas", r.seleccionadas},
                    {"butacas", r.etiquetasNuevas()}
                };
            });

            if (!existe) return responderError(res, 404, "La reservacion expiro. Empiece de nuevo");
            if (!error.empty()) return responderError(res, codigo, error);
            responderJson(res, respuesta);
        });

        // Paso 5: confirmar y simular el pago (POST /api/reserva/{token}/confirmar)
        svr.Post(R"(/api/reserva/(\w+)/confirmar)", [&](const httplib::Request& req, httplib::Response& res) {
            std::string token = req.matches[1];
            auto r = reservas.copia(token);
            if (!r) return responderError(res, 404, "La reservacion expiro. Empiece de nuevo");

            // 1. Tienen que estar elegidas exactamente las butacas pedidas.
            if (r->seleccionadas != r->cantidad)
                return responderError(res, 400, "Debe elegir " + std::to_string(r->cantidad) + " butacas");

            // 2. Recorrer la matriz y tomar SOLO las posiciones con estado 3.
            //    Las de estado 1 ya estaban en SQLite y no se vuelven a insertar.
            std::vector<int> ids_nuevos;
            for (const auto& [a, n] : r->posicionesNuevas()) ids_nuevos.push_back(r->ids[a][n]);

            // 3. Guardar en SQLite (revisa de nuevo la disponibilidad adentro).
            std::vector<int> conflictos = db.confirmarReserva(
                r->funcion.funcion_id, r->cliente_id, r->fecha, ids_nuevos);

            if (!conflictos.empty()) {
                // Otro cliente gano alguna butaca: se recarga la matriz desde
                // SQLite y se conservan las selecciones que siguen libres.
                json respuesta;
                reservas.usar(token, [&](ReservaTemporal& t) {
                    std::vector<std::vector<int>> nueva, ids;
                    db.cargarMatriz(t.funcion, t.fecha, nueva, ids);
                    t.seleccionadas = 0;
                    for (size_t a = 0; a < nueva.size(); a++)
                        for (size_t n = 0; n < nueva[a].size(); n++)
                            if (t.matriz[a][n] == SELECCIONADA && nueva[a][n] == DISPONIBLE) {
                                nueva[a][n] = SELECCIONADA;
                                t.seleccionadas++;
                            }
                    t.matriz = nueva;
                    t.ids    = ids;
                    respuesta = {
                        {"error", "Otra persona acaba de reservar alguna de sus butacas. Elija otra."},
                        {"matriz", t.matriz},
                        {"seleccionadas", t.seleccionadas},
                        {"butacas", t.etiquetasNuevas()}
                    };
                });
                return responderJson(res, respuesta, 409);
            }

            // 4. Listo: se borra la matriz temporal de memoria.
            reservas.borrar(token);
            responderJson(res, json{
                {"ok", true},
                {"total", r->funcion.precio * r->cantidad},
                {"butacas", r->etiquetasNuevas()},
                {"pelicula", r->funcion.pelicula},
                {"sala", r->funcion.sala},
                {"hora", r->funcion.hora}
            }, 201);
        });

        // Cancelar o cambiar la cantidad (POST /api/reserva/{token}/cancelar)
        // Borra la matriz temporal sin guardar nada.
        svr.Post(R"(/api/reserva/(\w+)/cancelar)", [&](const httplib::Request& req, httplib::Response& res) {
            reservas.borrar(req.matches[1]);
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
