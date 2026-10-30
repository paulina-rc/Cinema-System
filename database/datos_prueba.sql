-- =========================================================
--  Datos de prueba - Sistema Cine
--  Archivo: database/datos_prueba.sql
--  Ejecutar UNA sola vez:  sqlite3 cine.db < datos_prueba.sql
-- =========================================================

PRAGMA foreign_keys = ON;

-- Sala de 5 agrupaciones x 8 butacas = 40 butacas
INSERT INTO salas (nombre, agrupaciones, butacas_por_agrupacion)
VALUES ('Sala 1', 5, 8);


-- Lee la configuracion de la sala, asi que si cambias 5 u 8
-- arriba, las butacas se generan solas con el nuevo tamano.
WITH RECURSIVE
  filas(a) AS (
    SELECT 0
    UNION ALL
    SELECT a + 1 FROM filas
    WHERE a + 1 < (SELECT agrupaciones FROM salas WHERE nombre = 'Sala 1')
  ),
  columnas(n) AS (
    SELECT 0
    UNION ALL
    SELECT n + 1 FROM columnas
    WHERE n + 1 < (SELECT butacas_por_agrupacion FROM salas WHERE nombre = 'Sala 1')
  )
INSERT INTO butacas (sala_id, agrupacion, numero)
SELECT (SELECT id FROM salas WHERE nombre = 'Sala 1'), a, n
FROM filas, columnas;

-- Una butaca danada para probar que no se puede reservar
UPDATE butacas SET habilitada = 0
WHERE sala_id = (SELECT id FROM salas WHERE nombre = 'Sala 1')
  AND agrupacion = 0 AND numero = 0;

-- Pelicula en cartelera (incluye la fecha de hoy)
INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id)
VALUES ('Pelicula de prueba', '2026-10-01', '2026-12-31', 3500,
        (SELECT id FROM salas WHERE nombre = 'Sala 1'));

-- Pelicula que YA termino (no debe salir en la reservacion)
INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id)
VALUES ('Pelicula vencida', '2026-08-01', '2026-09-30', 3000,
        (SELECT id FROM salas WHERE nombre = 'Sala 1'));

INSERT INTO funciones (pelicula_id, hora) VALUES (1, '15:00');
INSERT INTO funciones (pelicula_id, hora) VALUES (1, '19:00');
INSERT INTO funciones (pelicula_id, hora) VALUES (2, '17:00');