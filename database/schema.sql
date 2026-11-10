-- schema.sql
-- tablas del sistema de cine en sqlite, una sala tiene butacas y peliculas,
-- una pelicula tiene funciones y cada reserva une funcion + butaca + cliente

-- sqlite trae las llaves foraneas apagadas, esto hay que correrlo en cada
-- conexion (Database.cpp tambien lo hace al abrir)
PRAGMA foreign_keys = ON;

-- con agrupaciones y butacas_por_agrupacion el backend arma la matriz
CREATE TABLE IF NOT EXISTS salas (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre                  TEXT    NOT NULL UNIQUE,
    agrupaciones            INTEGER NOT NULL CHECK (agrupaciones > 0),
    butacas_por_agrupacion  INTEGER NOT NULL CHECK (butacas_por_agrupacion > 0),
    habilitada              INTEGER NOT NULL DEFAULT 1 CHECK (habilitada IN (0, 1))
    -- las salas no se borran, se ponen en habilitada = 0
);

-- las butacas las crea el backend al crear la sala
-- agrupacion y numero empiezan en 0 para que coincidan con la matriz
CREATE TABLE IF NOT EXISTS butacas (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    sala_id     INTEGER NOT NULL,
    agrupacion  INTEGER NOT NULL CHECK (agrupacion >= 0),
    numero      INTEGER NOT NULL CHECK (numero >= 0),
    habilitada  INTEGER NOT NULL DEFAULT 1 CHECK (habilitada IN (0, 1)),
    -- habilitada = 0 es que la butaca esta danada

    FOREIGN KEY (sala_id) REFERENCES salas(id),
    UNIQUE (sala_id, agrupacion, numero)
);

-- las fechas van como 'AAAA-MM-DD' para poder compararlas como texto
CREATE TABLE IF NOT EXISTS peliculas (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre        TEXT    NOT NULL,
    fecha_inicio  TEXT    NOT NULL,
    fecha_fin     TEXT    NOT NULL,
    precio        REAL    NOT NULL CHECK (precio > 0),
    sala_id       INTEGER NOT NULL,
    imagen        TEXT    NOT NULL DEFAULT '',   -- ruta del poster dentro de frontend/, vacio si no tiene

    FOREIGN KEY (sala_id) REFERENCES salas(id),
    CHECK (fecha_fin >= fecha_inicio)
);

-- cada horario de una pelicula es una funcion, las reservas van contra la
-- funcion y no contra la pelicula porque la misma butaca puede estar libre
-- a las 2:00 y ocupada a las 8:00
CREATE TABLE IF NOT EXISTS funciones (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    pelicula_id  INTEGER NOT NULL,
    hora         TEXT    NOT NULL,

    FOREIGN KEY (pelicula_id) REFERENCES peliculas(id),
    UNIQUE (pelicula_id, hora)
);

CREATE TABLE IF NOT EXISTS clientes (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre    TEXT NOT NULL,
    telefono  TEXT NOT NULL
);

-- una fila por butaca, si alguien reserva 3 butacas son 3 filas
CREATE TABLE IF NOT EXISTS reservaciones (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    funcion_id  INTEGER NOT NULL,
    butaca_id   INTEGER NOT NULL,
    cliente_id  INTEGER NOT NULL,
    fecha       TEXT    NOT NULL,   -- dia de la funcion
    fecha_hora  TEXT    NOT NULL DEFAULT (datetime('now', 'localtime')),  -- momento en que se hizo la reserva

    FOREIGN KEY (funcion_id) REFERENCES funciones(id),
    FOREIGN KEY (butaca_id)  REFERENCES butacas(id),
    FOREIGN KEY (cliente_id) REFERENCES clientes(id),

    -- una butaca solo se puede reservar una vez por funcion y por dia,
    -- asi aunque el backend falle la base no deja la doble reserva
    UNIQUE (funcion_id, butaca_id, fecha)
);

-- la contrasena no se guarda, solo el hash con una sal aleatoria
-- (ver backend/src/Seguridad.h), si la tabla esta vacia el servidor
-- crea el admin inicial al arrancar
CREATE TABLE IF NOT EXISTS administradores (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    usuario     TEXT NOT NULL UNIQUE,
    sal         TEXT NOT NULL,
    clave_hash  TEXT NOT NULL
);

-- indices para las consultas que mas se repiten
CREATE INDEX IF NOT EXISTS idx_butacas_sala         ON butacas(sala_id);
CREATE INDEX IF NOT EXISTS idx_peliculas_fechas     ON peliculas(fecha_inicio, fecha_fin);
CREATE INDEX IF NOT EXISTS idx_reservaciones_funcion ON reservaciones(funcion_id, fecha);
