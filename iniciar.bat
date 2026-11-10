@echo off
REM iniciar.bat
REM arranca el servidor y abre el navegador, el cd es para que el servidor
REM encuentre frontend\ y database\ aunque se abra desde otra carpeta
cd /d "%~dp0"

if not exist build\servidor.exe (
  echo No existe build\servidor.exe. Ejecute primero compilar.bat
  pause & exit /b 1
)

start "" http://localhost:8080/cartelera.html
build\servidor.exe 8080
pause
