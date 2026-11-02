// =============================================================================
//  Database.cpp
//  -----------------------------------------------------------------------------
//  Implementacion de la clase Database con la API en C de SQLite.
//  La clase Sentencia se copio del demo del profe; solo se le agrego reiniciar().
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
#include "Database.h"

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

    // Deja la sentencia lista para ejecutarse otra vez con valores nuevos
    // (sirve para insertar muchas filas con un solo prepare).
    void reiniciar() {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
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

const std::string COLUMNAS_BUTACA = "id, sala_id, agrupacion, numero, habilitada";

Butaca leerButaca(Sentencia& s) {
    Butaca b;
    b.id         = s.entero(0);
    b.sala_id    = s.entero(1);
    b.agrupacion = s.entero(2);
    b.numero     = s.entero(3);
    b.habilitada = s.entero(4);
    return b;
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

std::optional<Sala> Database::obtenerSala(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "SELECT " + COLUMNAS_SALA + " FROM salas WHERE id = ?");
    s.bind(1, id);
    if (s.siguiente()) return leerSala(s);
    return std::nullopt;
}

std::optional<int> Database::crearSala(const std::string& nombre, int agrupaciones,
                                       int butacas_por_agrupacion) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. El nombre no se puede repetir (tambien lo protege el UNIQUE de la tabla).
    {
        Sentencia existe(db_, "SELECT COUNT(*) FROM salas WHERE nombre = ?");
        existe.bind(1, nombre);
        existe.siguiente();
        if (existe.entero(0) > 0) return std::nullopt;
    }

    // 2. Transaccion: la sala y sus butacas se guardan TODAS o NINGUNA.
    //    Si algo falla a la mitad, ROLLBACK deshace lo que se alcanzo a insertar
    //    y no queda una sala con butacas incompletas.
    ejecutar("BEGIN TRANSACTION;");
    try {
        Sentencia sala(db_, "INSERT INTO salas (nombre, agrupaciones, butacas_por_agrupacion) "
                            "VALUES (?, ?, ?)");
        sala.bind(1, nombre);
        sala.bind(2, agrupaciones);
        sala.bind(3, butacas_por_agrupacion);
        sala.siguiente();
        int sala_id = static_cast<int>(sqlite3_last_insert_rowid(db_));

        // 3. Generar la distribucion recorriendo la matriz:
        //    fila = agrupacion, columna = numero de butaca.
        //    Se prepara UNA sentencia y se reutiliza en cada vuelta (reset).
        Sentencia butaca(db_, "INSERT INTO butacas (sala_id, agrupacion, numero) VALUES (?, ?, ?)");
        for (int a = 0; a < agrupaciones; a++) {
            for (int n = 0; n < butacas_por_agrupacion; n++) {
                butaca.reiniciar();
                butaca.bind(1, sala_id);
                butaca.bind(2, a);
                butaca.bind(3, n);
                butaca.siguiente();
            }
        }

        ejecutar("COMMIT;");
        return sala_id;
    } catch (...) {
        ejecutar("ROLLBACK;");
        throw;   // el error sigue hacia main.cpp, que responde 500
    }
}

bool Database::cambiarHabilitadaSala(int id, int habilitada) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "UPDATE salas SET habilitada = ? WHERE id = ?");
    s.bind(1, habilitada);
    s.bind(2, id);
    s.siguiente();
    return sqlite3_changes(db_) > 0;
}

// =============================================================================
//  Butacas
// =============================================================================
std::vector<Butaca> Database::listarButacas(int sala_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Butaca> resultado;
    Sentencia s(db_, "SELECT " + COLUMNAS_BUTACA + " FROM butacas "
                     "WHERE sala_id = ? ORDER BY agrupacion, numero");
    s.bind(1, sala_id);
    while (s.siguiente()) resultado.push_back(leerButaca(s));
    return resultado;
}

bool Database::cambiarHabilitadaButaca(int id, int habilitada) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "UPDATE butacas SET habilitada = ? WHERE id = ?");
    s.bind(1, habilitada);
    s.bind(2, id);
    s.siguiente();
    return sqlite3_changes(db_) > 0;
}
