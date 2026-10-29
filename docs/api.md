# API del Cinema System

## Estado

Las rutas descritas son el contrato inicial previsto. El servidor C++ todavia
no esta implementado. Las respuestas definitivas y codigos HTTP deben
confirmarse al desarrollar `backend/main.cpp`.

Base URL local prevista: `http://localhost:8080`.

| Metodo | Ruta | Proposito | Estado |
|---|---|---|---|
| GET | `/api/peliculas` | Consultar peliculas en cartelera | Pendiente |
| POST | `/api/peliculas` | Registrar una pelicula | Pendiente |
| GET | `/api/salas` | Consultar salas y disponibilidad | Pendiente |
| POST | `/api/salas` | Crear sala y sus butacas | Pendiente |
| GET | `/api/funciones?pelicula_id={id}` | Consultar horarios | Pendiente |
| GET | `/api/funciones/{id}/butacas` | Consultar butacas disponibles | Pendiente |
| POST | `/api/reservas` | Confirmar una reserva | Pendiente |

El esquema de datos y sus restricciones estan documentados en
`database/schema.sql`. Todas las rutas que modifican datos deben validar la
entrada y responder con un error HTTP explicito si la operacion falla.
