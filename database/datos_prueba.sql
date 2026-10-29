-- Datos minimos para desarrollo. Ejecutar despues de schema.sql.
PRAGMA foreign_keys = ON;

INSERT INTO salas (nombre, agrupaciones, butacas_por_agrupacion)
SELECT 'Sala 1', 5, 8
WHERE NOT EXISTS (SELECT 1 FROM salas WHERE nombre = 'Sala 1');

WITH RECURSIVE filas(numero) AS (
    SELECT 0
    UNION ALL
    SELECT numero + 1 FROM filas WHERE numero < 4
),
columnas(numero) AS (
    SELECT 0
    UNION ALL
    SELECT numero + 1 FROM columnas WHERE numero < 7
)
INSERT INTO butacas (sala_id, agrupacion, numero)
SELECT s.id, filas.numero, columnas.numero
FROM salas s
CROSS JOIN filas
CROSS JOIN columnas
WHERE s.nombre = 'Sala 1'
  AND NOT EXISTS (
      SELECT 1 FROM butacas b
      WHERE b.sala_id = s.id
        AND b.agrupacion = filas.numero
        AND b.numero = columnas.numero
  );

INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id)
SELECT 'Pelicula de ejemplo', '2026-01-01', '2027-12-31', 75.00, s.id
FROM salas s
WHERE s.nombre = 'Sala 1'
  AND NOT EXISTS (SELECT 1 FROM peliculas WHERE nombre = 'Pelicula de ejemplo');

INSERT INTO funciones (pelicula_id, hora)
SELECT p.id, '16:00'
FROM peliculas p
WHERE p.nombre = 'Pelicula de ejemplo'
  AND NOT EXISTS (SELECT 1 FROM funciones WHERE pelicula_id = p.id AND hora = '16:00');
