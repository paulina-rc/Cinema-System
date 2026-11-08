# Cinema System

A web app for a movie theater. Customers can see the movies and book seats.
The admin can manage rooms, seats and movies.

## How to run it (Windows)

You need `g++` from MSYS2, with `C:\msys64\ucrt64\bin` in your `PATH`.
The libraries (cpp-httplib, nlohmann/json and SQLite) are already in
`backend/third_party/`. You don't need to install anything else.

```text
.\compilar.bat     builds the server
.\iniciar.bat      starts the server 
```

To stop the server, press `Ctrl + C` in the server window.

Always run these from the main project folder. The server looks for
`frontend/` and `database/` starting from there.

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

## API

All routes, what they receive and what they send back are in
[`docs/rutas-api.md`](docs/rutas-api.md)

