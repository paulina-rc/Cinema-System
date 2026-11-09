-- Datos iniciales (los mismos de database/cine.db): una sala de 5 x 8,
-- la butaca A1 fuera de servicio y las cuatro peliculas con poster.
INSERT INTO salas (nombre, agrupaciones, butacas_por_agrupacion) VALUES ('Sala 1', 5, 8);
--> statement-breakpoint
INSERT INTO butacas (sala_id, agrupacion, numero, habilitada)
SELECT s.id, a, n, CASE WHEN a = 0 AND n = 0 THEN 0 ELSE 1 END
FROM salas s, generate_series(0, 4) AS a, generate_series(0, 7) AS n
WHERE s.nombre = 'Sala 1';
--> statement-breakpoint
INSERT INTO peliculas (nombre, fecha_inicio, fecha_fin, precio, sala_id, imagen)
SELECT v.nombre, '2026-11-07', '2026-12-31', 3500, s.id, v.imagen
FROM salas s, (VALUES
  ('Los Colegas del Barrio',      'img/poster1.jpg'),
  ('Donde están las rubias',      'img/White_Chicks_Poster_Latino.webp'),
  ('Son como niños',              'img/7b86f2a7e80941d97c265348873a82c73450c0286ed1d1eeb7d86440ec707de5.jpg'),
  ('¿Quién &$%! son los Miller?', 'img/images.jpg')
) AS v(nombre, imagen)
WHERE s.nombre = 'Sala 1';
--> statement-breakpoint
INSERT INTO funciones (pelicula_id, hora)
SELECT p.id, h.hora
FROM peliculas p
JOIN (VALUES
  ('Los Colegas del Barrio', '14:00'), ('Los Colegas del Barrio', '19:30'),
  ('Donde están las rubias', '15:15'), ('Donde están las rubias', '20:00'),
  ('Son como niños', '13:30'), ('Son como niños', '18:00'),
  ('¿Quién &$%! son los Miller?', '16:30'), ('¿Quién &$%! son los Miller?', '21:00')
) AS h(nombre, hora) ON h.nombre = p.nombre;
