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
.\iniciar.bat      (abre la cartelera en el navegador)
```

Para detener el servidor: `Ctrl + C` en la ventana del servidor.
Se debe ejecutar desde la carpeta raíz, porque el servidor busca
`frontend/` y `database/` de forma relativa.

## Páginas

Con el servidor encendido:

| Página | Para qué |
|---|---|
| http://localhost:8080/ | Página principal: redirige a `cartelera.html` |
| http://localhost:8080/cartelera.html | Cartelera pública: películas en exhibición hoy |
| http://localhost:8080/reserva.html | Reservación de butacas para el cliente (5 pasos) |
| http://localhost:8080/admin-login.html | Inicio de sesión del administrador |
| http://localhost:8080/admin-salas.html | Admin: crear salas, habilitar/inhabilitar salas y butacas |
| http://localhost:8080/admin-peliculas.html | Admin: registrar películas con póster, sala, precio, fechas y horarios |

Las dos páginas `admin-*` piden iniciar sesión; si no hay sesión, mandan a
`admin-login.html`. Desde la cartelera se llega con el enlace **Admin** del menú.

### Usuario del administrador

La primera vez que arranca el servidor, si la tabla `administradores` está
vacía, se crea este usuario (también se muestra en la consola):

| Usuario | Contraseña inicial |
|---|---|
| `admin` | `imperium2026` |

Para cambiar la contraseña (desde la carpeta raíz, mínimo 8 caracteres):

```text
build\servidor.exe --cambiar-clave admin NUEVA_CLAVE
```

Ese comando solo guarda la clave nueva y termina, no arranca el servidor.
La sesión dura 8 horas y se pierde si se reinicia el servidor.


## Estructura

```text
Cinema-System/
├── frontend/
│   ├── cartelera.html        Página principal pública (carrusel + tarjetas + detalle)
│   ├── reserva.html          Reservación de butacas del cliente
│   ├── admin-login.html      Login del administrador
│   ├── admin-salas.html      Admin de salas y butacas
│   ├── admin-peliculas.html  Admin de películas
│   ├── css/                  Un archivo de estilos por página
│   ├── js/                   Un archivo por página + admin-sesion.js (protege el panel)
│   └── img/
│       ├── logo.png          Logo del cine (encabezado y favicon)
│       └── peliculas/        Pósters subidos desde el admin (los crea el servidor)
├── backend/
│   ├── src/
│   │   ├── main.cpp          Rutas de la API, validaciones y login
│   │   ├── Database.h/.cpp   Acceso a datos: todo el SQL está aquí
│   │   ├── modelos.h         Structs de cada tabla + conversión a JSON
│   │   ├── Seguridad.h       Hash SHA-256 con sal de las contraseñas y sesiones del admin (token de 8 h)
│   │   └── ReservaTemporal.h Matriz en memoria de la reserva en curso; solo se guarda en SQLite al confirmar
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
`reservaciones` y `administradores`. La estructura completa está en
`database/schema.sql`.

- `peliculas.imagen`: ruta del póster dentro de `frontend/`
  (por ejemplo `img/peliculas/pelicula-123.jpg`); vacía si no tiene póster.
- `administradores`: usuarios del panel admin (`usuario`, `sal`,
  `clave_hash`). La contraseña no se guarda, solo su hash con sal.

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
| Salas y butacas | Listo | Listo |
| Películas (con póster y horarios) | Listo | Listo |
| Reservación (matriz en memoria) | Listo | Listo |
| Login del administrador | Listo | Listo |
| Cartelera (carrusel y detalle de película) | Listo | Listo |
