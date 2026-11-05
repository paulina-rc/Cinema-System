# API del Sistema Cine

Lista de rutas entre el frontend (JavaScript) y el backend (C++).
Todo se envía y se recibe en **JSON**. Base: `http://localhost:8080`

El mismo servidor C++ entrega también las páginas de `frontend/`, así que el
JavaScript puede llamar con `fetch("/api/...")` sin poner la dirección completa.

## Respuesta de error (en todas las rutas)

Igual que el demo del profe: si algo falla, el backend responde con un código
de error y:

```json
{ "error": "Mensaje para mostrar al usuario" }
```

| Código | Cuándo |
|---|---|
| 200 | Todo bien |
| 201 | Se creó algo (respuesta de un POST que inserta) |
| 400 | Datos inválidos (campo vacío, fecha mal, cantidad incorrecta, JSON mal formado) |
| 404 | No existe (sala, película, función o token) |
| 409 | Conflicto: otra persona reservó la butaca antes de confirmar |
| 500 | Error interno (por ejemplo, de SQLite) |

La función `api()` del `app.js` del demo ya sirve tal cual con esta API.

---

## 1. Salas (administrador)

### `GET /api/salas`
Lista todas las salas, incluidas las inhabilitadas.
```json
[
  { "id": 1, "nombre": "Sala 1", "agrupaciones": 5, "butacas_por_agrupacion": 8, "habilitada": 1 }
]
```

### `POST /api/salas`
Crea la sala **y sus butacas**: el backend recorre la matriz e inserta cada una.
```json
{ "nombre": "Sala 2", "agrupaciones": 6, "butacas_por_agrupacion": 10 }
```
Respuesta: `{ "ok": true, "id": 2 }`

Validaciones del backend: nombre no vacío y sin repetir, números mayores que 0.

### `PUT /api/salas/{id}/habilitada`
Inhabilita o vuelve a habilitar una sala (no la borra).
```json
{ "habilitada": 0 }
```

### `GET /api/salas/{id}/butacas`
Mapa de la sala para que el administrador marque butacas dañadas.
```json
{
  "agrupaciones": 5,
  "butacas_por_agrupacion": 8,
  "butacas": [
    { "id": 1, "agrupacion": 0, "numero": 0, "habilitada": 0 },
    { "id": 2, "agrupacion": 0, "numero": 1, "habilitada": 1 }
  ]
}
```

### `PUT /api/butacas/{id}/habilitada`
```json
{ "habilitada": 0 }
```

---

## 2. Películas (administrador)

### `GET /api/peliculas`
Todas las películas registradas, con su sala y horarios.
```json
[
  {
    "id": 1, "nombre": "Pelicula de prueba",
    "fecha_inicio": "2026-10-01", "fecha_fin": "2026-12-31",
    "precio": 3500, "sala_id": 1, "sala": "Sala 1",
    "horarios": [ { "funcion_id": 1, "hora": "15:00" }, { "funcion_id": 2, "hora": "19:00" } ]
  }
]
```

### `POST /api/peliculas`
La sala se elige de una lista llenada con `GET /api/salas` (solo habilitadas).
```json
{
  "nombre": "Nueva pelicula",
  "fecha_inicio": "2026-11-01",
  "fecha_fin": "2026-11-30",
  "precio": 4000,
  "sala_id": 1,
  "horarios": ["14:00", "18:30"]
}
```
Respuesta: `{ "ok": true, "id": 3 }`

Validaciones del backend: campos completos, `fecha_fin >= fecha_inicio`,
precio mayor que 0, sala existente y habilitada, al menos un horario.

---

## 3. Reservación (cliente)

La página es `frontend/reserva.html` (un solo HTML con los 5 pasos) y su
lógica está en `frontend/js/reserva.js`. La matriz temporal vive en
`backend/src/ReservaTemporal.h`.

### Paso 1 — `POST /api/clientes`
```json
{ "nombre": "Maria Lopez", "telefono": "8888 7777" }
```
Respuesta: `{ "ok": true, "cliente_id": 5 }`

Validaciones del backend: nombre de 3 a 60 caracteres; teléfono de 8 dígitos
(acepta espacios o guiones y lo guarda solo con números).

### Paso 2 — `GET /api/cartelera`
Solo películas con la fecha de hoy dentro del período y sala habilitada.
Misma forma que `GET /api/peliculas`.

### Paso 3 — `GET /api/funciones/{id}/disponibles`
Cuántas butacas libres quedan HOY en esa función (para el texto
"Quedan N butacas disponibles" y el máximo del contador).
```json
{ "disponibles": 66 }
```

### Pasos 3 y 4 — `POST /api/reserva/iniciar`
El cliente ya eligió la función y la cantidad. El backend consulta SQLite, carga
la **matriz en memoria** con las reservas existentes de hoy y devuelve un `token`
que identifica esta reservación temporal.
```json
{ "cliente_id": 5, "funcion_id": 1, "cantidad": 3 }
```
Validaciones del backend: el cliente existe, la película está en exhibición
hoy, la sala está habilitada, cantidad de 1 a 10 y no mayor que las disponibles.

Respuesta (201):
```json
{
  "ok": true,
  "token": "r17",
  "cantidad": 3,
  "agrupaciones": 2,
  "butacas_por_agrupacion": 8,
  "matriz": [
    [2, 0, 0, 1, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0]
  ]
}
```

**Estados de la matriz** (mismos números en C++ y en JavaScript):

| Valor | Estado | Color |
|---|---|---|
| 0 | Disponible | Verde |
| 1 | Ocupada (reserva anterior en SQLite) | Rojo |
| 2 | Inhabilitada / dañada | Sin color (borde punteado) |
| 3 | Seleccionada por el cliente actual (nueva) | Amarillo |

Ninguna de las dos (1 y 2) se puede tocar. Se mantienen como números distintos
para que el backend sepa por qué está bloqueada y no intente guardar una butaca
dañada como si fuera una reserva; en pantalla la dañada sale como "Fuera de
servicio", igual que en el boceto.

La fila es la agrupación y la columna es el número: `matriz[agrupacion][numero]`.
Nombre de la butaca: letra de la agrupación + número desde 1 → `(0, 4)` = `A5`.
En pantalla cada agrupación se dibuja como un bloque (el número de columnas del
bloque es solo diseño, lo decide el JS).

### Paso 4 — `POST /api/reserva/{token}/marcar`
Cada clic en una butaca. El backend cambia 0 → 3 o 3 → 0 en la matriz en memoria.
Si es 1 o 2, lo rechaza. Tampoco deja pasar de la cantidad pedida.
```json
{ "agrupacion": 0, "numero": 4 }
```
Respuesta: `{ "ok": true, "estado": 3, "seleccionadas": 1, "butacas": ["A5"] }`

Errores: 409 si la butaca es 1 o 2; 400 si ya eligió todas; 404 si el token expiró
(la matriz temporal se borra sola a los 20 minutos).

### Paso 5 — `POST /api/reserva/{token}/confirmar`
Simula el pago. El backend:
1. Revisa que las seleccionadas (estado 3) sean igual a la cantidad.
2. Vuelve a consultar SQLite por si otro cliente reservó esas butacas mientras tanto.
3. Recorre la matriz e inserta **solo** las posiciones con estado 3.
4. Borra la reservación temporal de memoria.

Respuesta:
```json
{ "ok": true, "total": 10500, "butacas": ["A5", "A6", "A7"] }
```
Respuesta completa (201):
```json
{ "ok": true, "total": 10500, "butacas": ["A5", "B8", "B18"],
  "pelicula": "Dune", "sala": "Sala 2", "hora": "19:30" }
```

Si otra persona ganó una butaca: código 409. El backend recarga la matriz desde
SQLite, conserva las selecciones que siguen libres y responde:
```json
{ "error": "Otra persona acaba de reservar...", "matriz": [[...]],
  "seleccionadas": 1, "butacas": ["A3"] }
```
El navegador vuelve al paso 4 con el mapa actualizado.

### `POST /api/reserva/{token}/cancelar`
Borra la matriz temporal sin guardar nada. Se usa en "Cancelar y volver",
"Cambiar cantidad" y al cerrar la pestaña. Respuesta: `{ "ok": true }`
