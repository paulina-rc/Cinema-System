-- datos_prueba.sql
-- datos para probar, se corre una sola vez con: sqlite3 cine.db < datos_prueba.sql
-- (si se corre dos veces fallan los unique)

PRAGMA foreign_keys = ON;

-- 5 agrupaciones x 8 butacas = 40 butacas
INSERT INTO salas (nombre, agrupaciones, butacas_por_agrupacion)
VALUES ('Sala 1', 5, 8);


-- las butacas se generan leyendo la sala, asi si se cambia el 5 o el 8
-- de arriba no hay que tocar nada aca
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

-- una butaca danada para probar que no se deja reservar
UPDATE butacas SET habilitada = 0
WHERE sala_id = (SELECT id FROM salas WHERE nombre = 'Sala 1')
  AND agrupacion = 0 AND numero = 0;

-- esta tiene que salir en cartelera, las fechas incluyen hoy
INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id)
VALUES ('Pelicula de prueba', '2026-10-01', '2026-12-31', 3500,
        (SELECT id FROM salas WHERE nombre = 'Sala 1'));

-- esta ya termino, sirve para probar que no sale en cartelera
INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id)
VALUES ('Pelicula vencida', '2026-08-01', '2026-09-30', 3000,
        (SELECT id FROM salas WHERE nombre = 'Sala 1'));

INSERT INTO funciones (pelicula_id, hora) VALUES (1, '15:00');
INSERT INTO funciones (pelicula_id, hora) VALUES (1, '19:00');
INSERT INTO funciones (pelicula_id, hora) VALUES (2, '17:00');

-- las de la cartelera real, los posters estan en frontend/img/
INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id, imagen) VALUES
    ('Los Colegas del Barrio',      '2026-11-07', '2026-12-31', 3500, (SELECT id FROM salas WHERE nombre = 'Sala 1'), 'img/poster1.jpg'),
    ('Donde están las rubias',      '2026-11-07', '2026-12-31', 3500, (SELECT id FROM salas WHERE nombre = 'Sala 1'), 'img/White_Chicks_Poster_Latino.webp'),
    ('Son como niños',              '2026-11-07', '2026-12-31', 3500, (SELECT id FROM salas WHERE nombre = 'Sala 1'), 'img/7b86f2a7e80941d97c265348873a82c73450c0286ed1d1eeb7d86440ec707de5.jpg'),
    ('¿Quién &$%! son los Miller?', '2026-11-07', '2026-12-31', 3500, (SELECT id FROM salas WHERE nombre = 'Sala 1'), 'img/images.jpg');

INSERT INTO funciones (pelicula_id, hora)
SELECT p.id, h.hora
FROM peliculas p
JOIN (SELECT 'Los Colegas del Barrio' AS nombre, '14:00' AS hora UNION ALL
      SELECT 'Los Colegas del Barrio', '19:30' UNION ALL
      SELECT 'Donde están las rubias', '15:15' UNION ALL
      SELECT 'Donde están las rubias', '20:00' UNION ALL
      SELECT 'Son como niños', '13:30' UNION ALL
      SELECT 'Son como niños', '18:00' UNION ALL
      SELECT '¿Quién &$%! son los Miller?', '16:30' UNION ALL
      SELECT '¿Quién &$%! son los Miller?', '21:00') h ON h.nombre = p.nombre;
