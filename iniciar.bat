@echo off
REM ===========================================================================
REM  iniciar.bat - Arranca el servidor y abre el navegador.
REM  Se ejecuta desde la carpeta raiz para que encuentre frontend\ y database\
REM ===========================================================================
cd /d "%~dp0"

if not exist build\servidor.exe (
  echo No existe build\servidor.exe. Ejecute primero compilar.bat
  pause & exit /b 1
)

start "" http://localhost:8080/cartelera.html
build\servidor.exe 8080
pause
