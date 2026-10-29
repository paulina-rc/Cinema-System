# Cinema System

Sistema de cine con frontend web, backend en C++ y base de datos SQLite.

## Estado

Este repositorio contiene el esquema de base de datos y el esqueleto inicial de
la aplicacion. Las pantallas, rutas HTTP y servicios de C++ necesitan completar
su implementacion antes de que el sistema pueda usarse como una aplicacion
funcional.

## Estructura

- `frontend/`: paginas, estilos y JavaScript del navegador.
- `backend/`: servidor y servicios en C++.
- `database/schema.sql`: definicion de las tablas SQLite.
- `database/datos_prueba.sql`: datos de ejemplo para desarrollo.
- `docs/`: documentacion de API y ejecucion.

## Requisitos

- Compilador C++17 disponible en `PATH`.
- SQLite.
- cpp-httplib y nlohmann/json; coloca sus cabeceras en `backend/include/`.

Las dependencias externas no se incluyen en este repositorio.

## Base de datos

Desde la raiz del proyecto, ejecuta el esquema y los datos de ejemplo con una
herramienta SQLite:

```text
sqlite3 database/cine.db ".read database/schema.sql"
sqlite3 database/cine.db ".read database/datos_prueba.sql"
```

## Compilacion y ejecucion

El backend es un esqueleto y aun no expone las rutas de la API. Cuando se
implemente, desde Windows se podra iniciar la compilacion con
`backend\compilar.bat`. Consulta `docs/manual-ejecucion.md` para los pasos y el
estado de los componentes.