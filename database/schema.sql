-- =============================================================
--  Sistema de cine - Estructura de la base de datos (SQLite)
--  Archivo: database/schema.sql
-- =============================================================
--  Relaciones:
--    salas 1 ── N butacas
--    salas 1 ── N peliculas
--    peliculas 1 ── N funciones (una funcion = pelicula + horario)
--    reservaciones une: funcion + butaca + cliente
-- =============================================================

-- SQLite no revisa las llaves foraneas si no se activa.
-- OJO: en C++ hay que ejecutar esta misma linea cada vez que se abre la conexion.
PRAGMA foreign_keys = ON;

-- -------------------------------------------------------------
-- SALAS
-- La sala guarda su configuracion. Con estos dos numeros el backend
-- arma la matriz:  matriz[agrupacion][numero_de_butaca]
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS salas (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre                  TEXT    NOT NULL UNIQUE,
    agrupaciones            INTEGER NOT NULL CHECK (agrupaciones > 0),
    butacas_por_agrupacion  INTEGER NOT NULL CHECK (butacas_por_agrupacion > 0),
    habilitada              INTEGER NOT NULL DEFAULT 1 CHECK (habilitada IN (0, 1))
    -- habilitada = 0  ->  sala inhabilitada (no se borra)
);

-- -------------------------------------------------------------
-- BUTACAS
-- Una fila por cada butaca fisica de la sala.
-- Se generan automaticamente al crear la sala (lo hace el backend).
-- agrupacion y numero empiezan en 0, igual que los indices de la matriz.
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS butacas (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    sala_id     INTEGER NOT NULL,
    agrupacion  INTEGER NOT NULL CHECK (agrupacion >= 0),
    numero      INTEGER NOT NULL CHECK (numero >= 0),
    habilitada  INTEGER NOT NULL DEFAULT 1 CHECK (habilitada IN (0, 1)),
    -- habilitada = 0  ->  butaca danada o fuera de servicio

    FOREIGN KEY (sala_id) REFERENCES salas(id),
    UNIQUE (sala_id, agrupacion, numero)   -- no puede haber dos butacas en la misma posicion
);

-- -------------------------------------------------------------
-- PELICULAS
-- Fechas en formato 'AAAA-MM-DD' para poder compararlas como texto.
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS peliculas (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre        TEXT    NOT NULL,
    fecha_inicio  TEXT    NOT NULL,
    fecha_fin     TEXT    NOT NULL,
    precio        REAL    NOT NULL CHECK (precio > 0),
    sala_id       INTEGER NOT NULL,

    FOREIGN KEY (sala_id) REFERENCES salas(id),
    CHECK (fecha_fin >= fecha_inicio)      -- la fecha final no puede ser antes del inicio
);

-- -------------------------------------------------------------
-- FUNCIONES (horarios)
-- Cada horario de una pelicula es una funcion.
-- Las reservaciones se asocian a la FUNCION y no a la pelicula,
-- porque la misma butaca puede estar libre a las 2:00 y ocupada a las 8:00.
-- Hora en formato 'HH:MM' (24 horas).
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS funciones (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    pelicula_id  INTEGER NOT NULL,
    hora         TEXT    NOT NULL,

    FOREIGN KEY (pelicula_id) REFERENCES peliculas(id),
    UNIQUE (pelicula_id, hora)             -- no repetir el mismo horario en la misma pelicula
);

-- -------------------------------------------------------------
-- CLIENTES
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS clientes (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre    TEXT NOT NULL,
    telefono  TEXT NOT NULL
);

-- -------------------------------------------------------------
-- RESERVACIONES
-- Una fila por cada butaca reservada.
-- Si un cliente reserva 3 butacas, se insertan 3 filas.
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS reservaciones (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    funcion_id  INTEGER NOT NULL,
    butaca_id   INTEGER NOT NULL,
    cliente_id  INTEGER NOT NULL,
    fecha_hora  TEXT    NOT NULL DEFAULT (datetime('now', 'localtime')),

    FOREIGN KEY (funcion_id) REFERENCES funciones(id),
    FOREIGN KEY (butaca_id)  REFERENCES butacas(id),
    FOREIGN KEY (cliente_id) REFERENCES clientes(id),

    -- La regla mas importante del sistema:
    -- una butaca solo puede reservarse UNA vez por funcion.
    -- Aunque el backend falle, la base de datos rechaza la doble reserva.
    UNIQUE (funcion_id, butaca_id)
);

-- -------------------------------------------------------------
-- INDICES (hacen mas rapidas las consultas que mas se usan)
-- -------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_butacas_sala         ON butacas(sala_id);
CREATE INDEX IF NOT EXISTS idx_peliculas_fechas     ON peliculas(fecha_inicio, fecha_fin);
CREATE INDEX IF NOT EXISTS idx_reservaciones_funcion ON reservaciones(funcion_id);
