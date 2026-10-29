@echo off
setlocal
cd /d "%~dp0.."
where g++ >nul 2>nul
if errorlevel 1 (
  echo No se encontro g++. Instala un compilador C++17 y agregalo a PATH.
  exit /b 1
)
if not exist build mkdir build
g++ -std=c++17 -Wall -Wextra -pedantic ^
  backend\main.cpp ^
  backend\src\Database.cpp ^
  backend\src\MatrizSala.cpp ^
  backend\src\SalaService.cpp ^
  backend\src\PeliculaService.cpp ^
  backend\src\ReservaService.cpp ^
  backend\src\Validaciones.cpp ^
  -o build\cinema.exe
if errorlevel 1 exit /b 1
echo Compilacion completada. El servidor HTTP sigue pendiente de implementacion.
