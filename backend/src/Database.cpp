
// todo lo que toca SQLite esta aqui. Las consultas siempre usan ? y bind,
// nunca se pega texto del usuario dentro del SQL
#include "Database.h"

#include <stdexcept>

#include "sqlite3.h"

namespace {

// envuelve un sqlite3_stmt para no tener que acordarse del finalize
// viene del demo que vimos en clase, solo le agregue reiniciar()
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

    // importante: los parametros van desde 1 pero las columnas desde 0
    void bind(int i, const std::string& v) { sqlite3_bind_text(stmt_, i, v.c_str(), -1, SQLITE_TRANSIENT); }
    void bind(int i, double v)             { sqlite3_bind_double(stmt_, i, v); }
    void bind(int i, int v)                { sqlite3_bind_int(stmt_, i, v); }

    bool siguiente() {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW)  return true;
        if (rc == SQLITE_DONE) return false;
        throw std::runtime_error(std::string("Error ejecutando SQL: ") + sqlite3_errmsg(db_));
    }

    // para reutilizar la misma sentencia en un for sin preparar de nuevo
    void reiniciar() {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
    }

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

// leerSala y leerButaca leen las columnas en este mismo orden, si se cambia
// una lista hay que cambiar la funcion
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

Database::Database(const std::string& ruta_archivo) {
    // sin OPEN_CREATE a proposito: si falta cine.db es mejor que falle
    // a que se cree una base vacia sin tablas (se crea con database/schema.sql)
    int flags = SQLITE_OPEN_READWRITE;
    if (sqlite3_open_v2(ruta_archivo.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("No se pudo abrir " + ruta_archivo + ": " + msg);
    }

    // SQLite trae las llaves foraneas apagadas y hay que prenderlas
    // cada vez que se abre la conexion
    ejecutar("PRAGMA foreign_keys = ON;");

    // la columna imagen la agregue despues, asi que las bases viejas no la
    // tienen. Se revisa y se agrega una sola vez
    bool tiene_imagen = false;
    {
        Sentencia s(db_, "PRAGMA table_info(peliculas)");
        while (s.siguiente()) {
            if (s.texto(1) == "imagen") tiene_imagen = true;
        }
    }
    if (!tiene_imagen) {
        ejecutar("ALTER TABLE peliculas ADD COLUMN imagen TEXT NOT NULL DEFAULT '';");
    }

    // lo mismo con la tabla de admins, por eso el IF NOT EXISTS
    ejecutar(
        "CREATE TABLE IF NOT EXISTS administradores ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  usuario    TEXT NOT NULL UNIQUE,"
        "  sal        TEXT NOT NULL,"
        "  clave_hash TEXT NOT NULL"
        ");");
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

std::optional<Administrador> Database::obtenerAdmin(const std::string& usuario) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "SELECT id, usuario, sal, clave_hash FROM administradores WHERE usuario = ?");
    s.bind(1, usuario);
    if (!s.siguiente()) return std::nullopt;

    Administrador a;
    a.id         = s.entero(0);
    a.usuario    = s.texto(1);
    a.sal        = s.texto(2);
    a.clave_hash = s.texto(3);
    return a;
}

int Database::contarAdmins() {
    std::lock_guard<std::mutex> lock(mutex_);
    Sentencia s(db_, "SELECT COUNT(*) FROM administradores");
    s.siguiente();
    return s.entero(0);
}

void Database::guardarAdmin(const std::string& usuario, const std::string& sal,
                            const std::string& clave_hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    // si el usuario ya existe solo se le cambia la clave
    Sentencia s(db_,
        "INSERT INTO administradores (usuario, sal, clave_hash) VALUES (?, ?, ?) "
        "ON CONFLICT(usuario) DO UPDATE SET sal = excluded.sal, clave_hash = excluded.clave_hash");
    s.bind(1, usuario);
    s.bind(2, sal);
    s.bind(3, clave_hash);
    s.siguiente();
}

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

    // se revisa antes para devolver nullopt en vez de que explote el UNIQUE
    {
        Sentencia existe(db_, "SELECT COUNT(*) FROM salas WHERE nombre = ?");
        existe.bind(1, nombre);
        existe.siguiente();
        if (existe.entero(0) > 0) return std::nullopt;
    }

    // va en transaccion para que no quede una sala con la mitad de las butacas
    // si algo falla a medio camino
    ejecutar("BEGIN TRANSACTION;");
    try {
        Sentencia sala(db_, "INSERT INTO salas (nombre, agrupaciones, butacas_por_agrupacion) "
                            "VALUES (?, ?, ?)");
        sala.bind(1, nombre);
        sala.bind(2, agrupaciones);
        sala.bind(3, butacas_por_agrupacion);
        sala.siguiente();
        int sala_id = static_cast<int>(sqlite3_last_insert_rowid(db_));

        // fila = agrupacion, columna = numero
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
        throw;   // main.cpp lo atrapa y responde 500
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

std::vector<Pelicula> Database::listarPeliculas(bool solo_cartelera) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string sql =
        "SELECT p.id, p.nombre, p.fecha_inicio, p.fecha_fin, p.precio, p.sala_id, s.nombre, p.imagen "
        "FROM peliculas p JOIN salas s ON s.id = p.sala_id ";

    // Las fechas van como 'AAAA-MM-DD', asi que compararlas como texto
    // da el mismo orden que las fechas reales.
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
            p.imagen       = s.texto(7);
            resultado.push_back(p);
        }
    }

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

    // Igual que en crearSala, todo o nada.
    ejecutar("BEGIN TRANSACTION;");
    try {
        Sentencia ins(db_, "INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id, imagen) "
                           "VALUES (?, ?, ?, ?, ?, ?)");
        ins.bind(1, p.nombre);
        ins.bind(2, p.fecha_inicio);
        ins.bind(3, p.fecha_fin);
        ins.bind(4, p.precio);
        ins.bind(5, p.sala_id);
        ins.bind(6, p.imagen);
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

std::string Database::fechaHoy() {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "SELECT date('now', 'localtime')");
    s.siguiente();
    return s.texto(0);
}

std::optional<InfoFuncion> Database::obtenerFuncion(int funcion_id) {
    std::lock_guard<std::mutex> lock(mutex_);

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

    // Arranca todo en 0 (disponible) y despues se marcan las que no lo estan.
    matriz.assign(f.agrupaciones, std::vector<int>(f.butacas_por_agrupacion, 0));
    ids.assign(f.agrupaciones, std::vector<int>(f.butacas_por_agrupacion, 0));

    // LEFT JOIN porque se necesitan todas las butacas, tengan reserva o no.
    // Si hay reserva para esta funcion y este dia, r.id no viene NULL.
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

    while (s.siguiente()) {
        int id = s.entero(0), a = s.entero(1), n = s.entero(2);
        if (a < 0 || a >= f.agrupaciones || n < 0 || n >= f.butacas_por_agrupacion) continue;

        ids[a][n] = id;
        if (s.entero(3) == 0)      matriz[a][n] = 2;   // danada
        else if (s.entero(4) == 1) matriz[a][n] = 1;   // ocupada
    }
}

std::vector<int> Database::confirmarReserva(int funcion_id, int cliente_id,
                                            const std::string& fecha,
                                            const std::vector<int>& butaca_ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<int> conflictos;

    // BEGIN IMMEDIATE toma el lock de escritura desde el inicio, asi dos
    // confirmaciones al mismo tiempo no pueden pasar la revision las dos.
    ejecutar("BEGIN IMMEDIATE TRANSACTION;");
    try {
        // Se revisa otra vez aunque la matriz ya se reviso, porque mientras
        // este cliente elegia otro pudo reservar o el admin pudo inhabilitar
        // alguna butaca
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
        return conflictos;
    } catch (...) {
        ejecutar("ROLLBACK;");
        throw;
    }
}
