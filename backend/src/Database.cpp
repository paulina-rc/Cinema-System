// =============================================================================
//  database.cpp
//  -----------------------------------------------------------------------------
//  Implementacion de la clase Database con la API en C de SQLite.
//  La clase Sentencia se copio del demo del profe sin cambios.
//
//  Flujo de cada consulta:
//     1. sqlite3_prepare_v2()  -> "compila" el texto SQL
//     2. sqlite3_bind_*()      -> rellena los parametros  ?
//     3. sqlite3_step()        -> ejecuta; una fila cada vez
//     4. sqlite3_column_*()    -> lee las columnas de la fila actual
//     5. sqlite3_finalize()    -> libera la sentencia
//
//  Seguridad: nunca se concatena texto del usuario en el SQL; siempre ? + bind.
// =============================================================================
#include "database.h"

#include <stdexcept>

#include "sqlite3.h"

namespace {

// -----------------------------------------------------------------------------
//  Sentencia: envoltorio RAII sobre sqlite3_stmt (copiado del demo).
//  El constructor hace el paso 1 y el destructor el paso 5.
// -----------------------------------------------------------------------------
class Sentencia {
public:
    Sentencia(sqlite3* db, const std::string& sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("Error preparando SQL: ") + sqlite3_errmsg(db));
        }
    }
    ~Sentencia() { sqlite3_finalize(stmt_); }

    Sentencia(const Sentencia&)            = delete;
    Sentencia& operator=(const Sentencia&) = delete;

    // Los parametros se numeran desde 1.
    void bind(int i, const std::string& v) { sqlite3_bind_text(stmt_, i, v.c_str(), -1, SQLITE_TRANSIENT); }
    void bind(int i, double v)             { sqlite3_bind_double(stmt_, i, v); }
    void bind(int i, int v)                { sqlite3_bind_int(stmt_, i, v); }

    // Avanza a la siguiente fila. true = hay fila; false = termino.
    bool siguiente() {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW)  return true;
        if (rc == SQLITE_DONE) return false;
        throw std::runtime_error(std::string("Error ejecutando SQL: ") + sqlite3_errmsg(db_));
    }

    // Las columnas se numeran desde 0.
    int         entero(int col) { return sqlite3_column_int(stmt_, col); }
    double      real(int col)   { return sqlite3_column_double(stmt_, col); }
    std::string texto(int col) {
        auto p = sqlite3_column_text(stmt_, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    }

private:
    sqlite3*      db_   = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

// Convierte la fila actual de un SELECT en una Sala.
// El orden debe coincidir con COLUMNAS_SALA.
const std::string COLUMNAS_SALA = "id, nombre, agrupaciones, butacas_por_agrupacion, habilitada";

Sala leerSala(Sentencia& s) {
    Sala sala;
    sala.id                     = s.entero(0);
    sala.nombre                 = s.texto(1);
    sala.agrupaciones           = s.entero(2);
    sala.butacas_por_agrupacion = s.entero(3);
    sala.habilitada             = s.entero(4);
    return sala;
}

} // namespace

// =============================================================================
//  Constructor / destructor
// =============================================================================
Database::Database(const std::string& ruta_archivo) {
    // A diferencia del demo, NO creamos el archivo si no existe:
    // las tablas se crean con database/schema.sql. Si falta cine.db,
    // preferimos un error claro a una base de datos vacia.
    int flags = SQLITE_OPEN_READWRITE;
    if (sqlite3_open_v2(ruta_archivo.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("No se pudo abrir " + ruta_archivo + ": " + msg);
    }

    // SQLite trae las llaves foraneas apagadas: hay que activarlas
    // cada vez que se abre la conexion.
    ejecutar("PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    sqlite3_close(db_);
}

void Database::ejecutar(const std::string& sql) {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        std::string msg = error ? error : "desconocido";
        sqlite3_free(error);
        throw std::runtime_error("Error SQL: " + msg);
    }
}

// =============================================================================
//  Salas
// =============================================================================
std::vector<Sala> Database::listarSalas() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Sala> resultado;
    Sentencia s(db_, "SELECT " + COLUMNAS_SALA + " FROM salas ORDER BY nombre");
    while (s.siguiente()) resultado.push_back(leerSala(s));
    return resultado;
}
