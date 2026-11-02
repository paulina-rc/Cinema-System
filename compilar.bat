@echo off
REM ===========================================================================
REM  compilar.bat - Compila el servidor del Sistema Cine.
REM  Basado en el compilar.bat del demo, con las carpetas de este proyecto:
REM    backend\src\          nuestro codigo C++
REM    backend\third_party\  httplib, json y sqlite (copiadas del demo)
REM  Requiere g++ y gcc en el PATH (MSYS2 UCRT64: C:\msys64\ucrt64\bin)
REM ===========================================================================
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul || (
  echo [ERROR] No se encontro g++ en el PATH.
  echo         Agregue C:\msys64\ucrt64\bin al PATH.
  pause & exit /b 1
)

if not exist build mkdir build

REM --- 1. SQLite: solo la primera vez -----------------------------------------
if not exist build\sqlite3.o (
  echo [1/3] Compilando SQLite ^(solo la primera vez, puede tardar^)...
  gcc -O2 -DSQLITE_THREADSAFE=1 -c backend\third_party\sqlite\sqlite3.c -o build\sqlite3.o || goto :error
) else (
  echo [1/3] SQLite ya compilado, se omite.
)

REM --- 2. Nuestro codigo C++ --------------------------------------------------
echo [2/3] Compilando codigo C++...
set FLAGS=-std=c++17 -O2 -Wall -D_WIN32_WINNT=0x0A00 -Ibackend\src -Ibackend\third_party\httplib -Ibackend\third_party\json -Ibackend\third_party\sqlite
g++ %FLAGS% -c backend\src\main.cpp     -o build\main.o     || goto :error
g++ %FLAGS% -c backend\src\database.cpp -o build\database.o || goto :error

REM --- 3. Enlazar -------------------------------------------------------------
echo [3/3] Enlazando...
g++ build\main.o build\database.o build\sqlite3.o -o build\servidor.exe -lws2_32 -static || goto :error

echo.
echo Listo: build\servidor.exe
echo Ejecute  iniciar.bat  para arrancar el servidor.
pause
exit /b 0

:error
echo.
echo [ERROR] La compilacion fallo.
pause
exit /b 1
