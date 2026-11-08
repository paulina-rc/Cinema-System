# Cinema System

A web app for a movie theater. Customers can see the movies and book seats.
The admin can manage rooms, seats and movies.

- **Frontend:** HTML, CSS and JavaScript (`frontend/`)
- **Backend:** an HTTP server written in C++17 (`backend/`)
- **Database:** SQLite, saved in one file (`database/cine.db`)

The browser never talks to the database directly. It asks the C++ server
for everything through an HTTP API. Only the server reads and writes SQLite.

## How to run it (Windows)

You need `g++` from MSYS2, with `C:\msys64\ucrt64\bin` in your `PATH`.
The libraries (cpp-httplib, nlohmann/json and SQLite) are already in
`backend/third_party/`. You don't need to install anything else.

```text
.\compilar.bat     (builds the server; the first time takes about 1 minute)
.\iniciar.bat      (starts the server and opens the movie listings page)
```

To stop the server, press `Ctrl + C` in the server window.

Always run these from the main project folder. The server looks for
`frontend/` and `database/` starting from there.

## Pages

When the server is running:

| Page | What it is for |
|---|---|
| http://localhost:8080/ | Home page. It sends you to `cartelera.html` |
| http://localhost:8080/cartelera.html | Public movie listings: movies showing today |
| http://localhost:8080/reserva.html | Seat booking for customers (5 steps) |
| http://localhost:8080/admin-login.html | Admin login |
| http://localhost:8080/admin-salas.html | Admin: add rooms, turn rooms and seats on or off |
| http://localhost:8080/admin-peliculas.html | Admin: add movies with poster, room, price, dates and showtimes |

The two `admin-*` pages need a login. If you are not logged in, they send you
to `admin-login.html`. From the movie listings page, click **Admin** in the
top menu.

### Admin user

The first time the server starts, if there are no admins yet, it creates
this user. It also prints it in the server window.

| User | First password |
|---|---|
| `admin` | `imperium2026` |

To change the password, run this from the main project folder.
The new password needs at least 8 characters.

```text
build\servidor.exe --cambiar-clave admin NEW_PASSWORD
```

This command only saves the new password and then stops. It does not start
the server.

A login lasts 8 hours. If the server restarts, you need to log in again.

## Folders and files

```text
Cinema-System/
├── frontend/
│   ├── cartelera.html        Home page: movie carousel, movie cards and movie details
│   ├── reserva.html          Seat booking for customers
│   ├── admin-login.html      Admin login
│   ├── admin-salas.html      Admin page for rooms and seats
│   ├── admin-peliculas.html  Admin page for movies
│   ├── css/                  One style file per page
│   ├── js/                   One script per page, plus admin-sesion.js (keeps admin pages private)
│   └── img/
│       ├── logo.png          Cinema logo (page header and browser tab icon)
│       └── peliculas/        Posters uploaded by the admin (the server creates this folder)
├── backend/
│   ├── src/
│   │   ├── main.cpp          API routes, data checks and login
│   │   ├── Database.h/.cpp   Database access: all the SQL is here
│   │   ├── modelos.h         One struct per table, and how to turn it into JSON
│   │   ├── Seguridad.h       Keeps passwords safe (salted SHA-256 hash) and tracks admin logins (8 hours)
│   │   └── ReservaTemporal.h Holds the seats a customer is picking in memory; saves them to SQLite only when they confirm
│   └── third_party/          httplib, json and sqlite (do not change these)
├── database/
│   ├── cine.db               The database
│   ├── schema.sql            Table structure
│   └── datos_prueba.sql      Sample data
├── docs/                     API, style guide, diagrams and screenshots
├── compilar.bat              Builds the server
└── iniciar.bat               Starts the server
```

## Database

Tables: `salas` (rooms), `butacas` (seats), `peliculas` (movies),
`funciones` (showtimes), `clientes` (customers), `reservaciones` (bookings)
and `administradores` (admins). The full structure is in `database/schema.sql`.

- `peliculas.imagen`: where the poster is, inside `frontend/`
  (for example `img/peliculas/pelicula-123.jpg`). It is empty if the movie
  has no poster.
- `administradores`: admin users (`usuario`, `sal`, `clave_hash`).
  The password itself is never saved, only a salted hash of it.

To create the database from zero (MSYS2 terminal, inside `database/`):

```text
sqlite3 cine.db < schema.sql
sqlite3 cine.db < datos_prueba.sql
```

## API

All routes, what they receive and what they send back are in
[`docs/rutas-api.md`](docs/rutas-api.md).

## Status

| Part | Backend | Frontend |
|---|---|---|
| Rooms and seats | Done | Done |
| Movies (with poster and showtimes) | Done | Done |
| Seat booking (seats kept in memory until confirmed) | Done | Done |
| Admin login | Done | Done |
| Movie listings (carousel and movie details) | Done | Done |
