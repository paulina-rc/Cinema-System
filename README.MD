# Cinema System

Sistema web de administración y reservación de butacas para un cine.

- **Frontend:** HTML, CSS y JavaScript (`frontend/`)
- **Backend:** servidor HTTP en C++17 (`backend/`)
- **Base de datos:** SQLite en un solo archivo (`database/cine.db`)

El navegador nunca toca la base de datos: le pide todo al servidor C++ por una
API HTTP, y el servidor es el único que lee y escribe en SQLite.

## Cómo ejecutarlo (Windows)

Requisito: `g++` de MSYS2 con `C:\msys64\ucrt64\bin` en el `PATH`.
Las librerías (cpp-httplib, nlohmann/json y SQLite) ya vienen en
`backend/third_party/`, no hay que instalar nada más.

```text
.\compilar.bat     (la primera vez tarda ~1 min porque compila SQLite)
.\iniciar.bat      (abre http://localhost:8080)
```

Para detener el servidor: `Ctrl + C` en la ventana del servidor.
Se debe ejecutar desde la carpeta raíz, porque el servidor busca
`frontend/` y `database/` de forma relativa.

## Estructura

```text
Cinema-System/
├── frontend/          Páginas, estilos (css/) y JavaScript (js/)
├── backend/
│   ├── src/
│   │   ├── main.cpp       Rutas de la API y validaciones
│   │   ├── Database.h/.cpp Acceso a datos: todo el SQL está aquí
│   │   └── modelos.h      Structs de cada tabla + conversión a JSON
│   └── third_party/   httplib, json y sqlite (no se modifican)
├── database/
│   ├── cine.db            Base de datos
│   ├── schema.sql         Estructura de las tablas
│   └── datos_prueba.sql   Datos de ejemplo
├── docs/              API, guía de estilo, diagramas y capturas
├── compilar.bat
└── iniciar.bat
```

## Base de datos

Tablas: `salas`, `butacas`, `peliculas`, `funciones`, `clientes`,
`reservaciones`. La estructura completa está en `database/schema.sql`.

Para crearla desde cero (terminal MSYS2, en la carpeta `database/`):

```text
sqlite3 cine.db < schema.sql
sqlite3 cine.db < datos_prueba.sql
```

## API

Las rutas, lo que reciben y lo que responden están en
[`docs/rutas-api.md`](docs/rutas-api.md).

## Estado

| Módulo | Backend | Frontend |
|---|---|---|
| Salas y butacas | Listo | En progreso |
| Películas | Pendiente | En progreso |
| Reservación | Pendiente | En progreso |
