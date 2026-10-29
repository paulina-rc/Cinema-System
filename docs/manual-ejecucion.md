# Manual de ejecucion

## Requisitos

- Windows y un compilador C++17 en `PATH`.
- SQLite.
- Las cabeceras de cpp-httplib y nlohmann/json en `backend/include/`.

Las dependencias externas no se descargan ni se incluyen automaticamente.

## Preparar la base de datos

Desde la raiz del repositorio:

```text
sqlite3 database/cine.db ".read database/schema.sql"
sqlite3 database/cine.db ".read database/datos_prueba.sql"
```

## Backend

El backend todavia es un esqueleto: las rutas HTTP y servicios de negocio no
estan implementados. `backend\compilar.bat` lo indica y termina con error
intencionalmente para no dar la impresion de que el servidor esta listo.

## Frontend

Abre `frontend\index.html` en un navegador cuando el backend este disponible.
La API del frontend apunta por defecto a `http://localhost:8080`.

## Diagramas

Los diagramas PNG y las capturas se agregaran cuando se definan y validen los
flujos de la aplicacion.
