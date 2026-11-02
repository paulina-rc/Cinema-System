# Manual de ejecución

## Requisitos

- Windows con MSYS2 y el paquete `mingw-w64-ucrt-x86_64-gcc`.
- `C:\msys64\ucrt64\bin` agregado al `PATH` de Windows.

Las librerías cpp-httplib, nlohmann/json y SQLite ya están incluidas en
`backend/third_party/`.

## Compilar y ejecutar

Desde la carpeta raíz del proyecto (por ejemplo en la terminal de VS Code):

```text
.\compilar.bat
.\iniciar.bat
```

`iniciar.bat` abre el navegador en `http://localhost:8080`. El servidor
entrega las páginas de `frontend/` y responde las rutas `/api/...`.

## Cuándo recompilar

| Si cambia... | Qué hacer |
|---|---|
| HTML, CSS o JS de `frontend/` | Recargar el navegador (`Ctrl + F5`) |
| Archivos `.cpp` o `.h` de `backend/src/` | `Ctrl + C` al servidor, `.\compilar.bat`, `.\iniciar.bat` |

Si al compilar sale `Permission denied`, el servidor sigue abierto:
deténgalo con `Ctrl + C` antes de compilar.

## Probar la API sin pantallas

Con el servidor encendido, abrir `http://localhost:8080`, presionar `F12` →
Consola y escribir, por ejemplo:

```js
fetch('/api/salas').then(r => r.json()).then(console.log)
```
