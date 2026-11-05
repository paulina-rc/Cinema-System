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

// =============================================================================
//  Peliculas
// =============================================================================
std::vector<Pelicula> Database::listarPeliculas(bool solo_cartelera) {
    std::lock_guard<std::mutex> lock(mutex_);

    // JOIN: trae tambien el nombre de la sala desde la tabla salas.
    std::string sql =
        "SELECT p.id, p.nombre, p.fecha_inicio, p.fecha_fin, p.precio, p.sala_id, s.nombre "
        "FROM peliculas p JOIN salas s ON s.id = p.sala_id ";

    // Cartelera: la fecha de HOY tiene que estar dentro del periodo.
    // Las fechas se guardan como 'AAAA-MM-DD', asi que se pueden comparar
    // como texto y el orden coincide con el orden real de las fechas.
    if (solo_cartelera) {
        sql += "WHERE date('now', 'localtime') BETWEEN p.fecha_inicio AND p.fecha_fin "
               "AND s.habilitada = 1 ";
    }
    sql += "ORDER BY p.nombre";

    std::vector<Pelicula> resultado;
    {
        Sentencia s(db_, sql);
        while (s.siguiente()) {
            Pelicula p;
            p.id           = s.entero(0);
            p.nombre       = s.texto(1);
            p.fecha_inicio = s.texto(2);
            p.fecha_fin    = s.texto(3);
            p.precio       = s.real(4);
            p.sala_id      = s.entero(5);
            p.sala         = s.texto(6);
            resultado.push_back(p);
        }
    }

    // Horarios de cada pelicula (una consulta preparada, reutilizada).
    Sentencia h(db_, "SELECT id, hora FROM funciones WHERE pelicula_id = ? ORDER BY hora");
    for (Pelicula& p : resultado) {
        h.reiniciar();
        h.bind(1, p.id);
        while (h.siguiente()) {
            Funcion f;
            f.id   = h.entero(0);
            f.hora = h.texto(1);
            p.horarios.push_back(f);
        }
    }
    return resultado;
}

int Database::crearPelicula(const Pelicula& p, const std::vector<std::string>& horarios) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Transaccion: la pelicula y sus horarios se guardan todos o ninguno.
    ejecutar("BEGIN TRANSACTION;");
    try {
        Sentencia ins(db_, "INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id) "
                           "VALUES (?, ?, ?, ?, ?)");
        ins.bind(1, p.nombre);
        ins.bind(2, p.fecha_inicio);
        ins.bind(3, p.fecha_fin);
        ins.bind(4, p.precio);
        ins.bind(5, p.sala_id);
        ins.siguiente();
        int pelicula_id = static_cast<int>(sqlite3_last_insert_rowid(db_));

        Sentencia fun(db_, "INSERT INTO funciones (pelicula_id, hora) VALUES (?, ?)");
        for (const std::string& hora : horarios) {
            fun.reiniciar();
            fun.bind(1, pelicula_id);
            fun.bind(2, hora);
            fun.siguiente();
        }

        ejecutar("COMMIT;");
        return pelicula_id;
    } catch (...) {
        ejecutar("ROLLBACK;");
        throw;
    }
}

// =============================================================================
//  Clientes
// =============================================================================
int Database::crearCliente(const std::string& nombre, const std::string& telefono) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "INSERT INTO clientes (nombre, telefono) VALUES (?, ?)");
    s.bind(1, nombre);
    s.bind(2, telefono);
    s.siguiente();
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

bool Database::existeCliente(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "SELECT COUNT(*) FROM clientes WHERE id = ?");
    s.bind(1, id);
    s.siguiente();
    return s.entero(0) > 0;
}

// =============================================================================
//  Reservaciones
// =============================================================================
std::string Database::fechaHoy() {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "SELECT date('now', 'localtime')");
    s.siguiente();
    return s.texto(0);
}

std::optional<InfoFuncion> Database::obtenerFuncion(int funcion_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // JOIN de tres tablas: la funcion, su pelicula y la sala de la pelicula.
    Sentencia s(db_,
        "SELECT f.id, f.hora, p.id, p.nombre, p.precio, "
        "       s.id, s.nombre, s.agrupaciones, s.butacas_por_agrupacion, s.habilitada, "
        "       (date('now', 'localtime') BETWEEN p.fecha_inicio AND p.fecha_fin) "
        "FROM funciones f "
        "JOIN peliculas p ON p.id = f.pelicula_id "
        "JOIN salas s     ON s.id = p.sala_id "
        "WHERE f.id = ?");
    s.bind(1, funcion_id);
    if (!s.siguiente()) return std::nullopt;

    InfoFuncion f;
    f.funcion_id             = s.entero(0);
    f.hora                   = s.texto(1);
    f.pelicula_id            = s.entero(2);
    f.pelicula               = s.texto(3);
    f.precio                 = s.real(4);
    f.sala_id                = s.entero(5);
    f.sala                   = s.texto(6);
    f.agrupaciones           = s.entero(7);
    f.butacas_por_agrupacion = s.entero(8);
    f.sala_habilitada        = s.entero(9);
    f.en_cartelera           = s.entero(10);
    return f;
}

void Database::cargarMatriz(const InfoFuncion& f, const std::string& fecha,
                            std::vector<std::vector<int>>& matriz,
                            std::vector<std::vector<int>>& ids) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Crear la matriz del tamano de la sala, todo en 0 (disponible).
    matriz.assign(f.agrupaciones, std::vector<int>(f.butacas_por_agrupacion, 0));
    ids.assign(f.agrupaciones, std::vector<int>(f.butacas_por_agrupacion, 0));

    // 2. LEFT JOIN: trae TODAS las butacas de la sala y, si la butaca ya
    //    tiene reserva para esta funcion y este dia, r.id no es NULL.
    Sentencia s(db_,
        "SELECT b.id, b.agrupacion, b.numero, b.habilitada, "
        "       (r.id IS NOT NULL) AS ocupada "
        "FROM butacas b "
        "LEFT JOIN reservaciones r "
        "       ON r.butaca_id = b.id AND r.funcion_id = ? AND r.fecha = ? "
        "WHERE b.sala_id = ?");
    s.bind(1, f.funcion_id);
    s.bind(2, fecha);
    s.bind(3, f.sala_id);

    // 3. Pasar el estado de cada butaca a su posicion en la matriz.
    while (s.siguiente()) {
        int id = s.entero(0), a = s.entero(1), n = s.entero(2);
        if (a < 0 || a >= f.agrupaciones || n < 0 || n >= f.butacas_por_agrupacion) continue;

        ids[a][n] = id;
        if (s.entero(3) == 0)      matriz[a][n] = 2;   // danada / fuera de servicio
        else if (s.entero(4) == 1) matriz[a][n] = 1;   // ocupada por otro cliente
    }
}

std::vector<int> Database::confirmarReserva(int funcion_id, int cliente_id,
                                            const std::string& fecha,
                                            const std::vector<int>& butaca_ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<int> conflictos;

    // Transaccion: o se guardan TODAS las butacas del cliente o NINGUNA.
    ejecutar("BEGIN IMMEDIATE TRANSACTION;");
    try {
        // 1. Revisar otra vez en SQLite (otro cliente pudo reservar mientras
        //    este elegia, o el administrador pudo inhabilitar la butaca).
        Sentencia revisar(db_,
            "SELECT b.habilitada, "
            "       (SELECT COUNT(*) FROM reservaciones r "
            "        WHERE r.butaca_id = b.id AND r.funcion_id = ? AND r.fecha = ?) "
            "FROM butacas b WHERE b.id = ?");
        for (int id : butaca_ids) {
            revisar.reiniciar();
            revisar.bind(1, funcion_id);
            revisar.bind(2, fecha);
            revisar.bind(3, id);
            if (!revisar.siguiente() || revisar.entero(0) == 0 || revisar.entero(1) > 0)
                conflictos.push_back(id);
        }

        if (!conflictos.empty()) {
            ejecutar("ROLLBACK;");
            return conflictos;
        }

        // 2. Insertar una fila por butaca nueva.
        Sentencia ins(db_,
            "INSERT INTO reservaciones (funcion_id, butaca_id, cliente_id, fecha) "
            "VALUES (?, ?, ?, ?)");
        for (int id : butaca_ids) {
            ins.reiniciar();
            ins.bind(1, funcion_id);
            ins.bind(2, id);
            ins.bind(3, cliente_id);
            ins.bind(4, fecha);
            ins.siguiente();
        }

        ejecutar("COMMIT;");
        return conflictos;   // vacio = todo bien
    } catch (...) {
        ejecutar("ROLLBACK;");
        throw;
    }
}
