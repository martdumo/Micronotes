@echo off
echo Buscando instalacion de Visual Studio...

:: Ruta específica para Visual Studio Community 2022
set VC_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat

if exist "%VC_PATH%" (
    goto :found_vc
)

:: Buscar Visual Studio 2022/2019 en otras versiones
set VC_PATH=
for /f "delims=" %%i in ('dir /s /b "C:\Program Files\Microsoft Visual Studio\*\VC\Auxiliary\Build\vcvarsall.bat" 2^>nul') do (
    set VC_PATH=%%i
    goto :found_vc
)

:: Buscar en Program Files (x86) tambien
for /f "delims=" %%i in ('dir /s /b "C:\Program Files (x86)\Microsoft Visual Studio\*\VC\Auxiliary\Build\vcvarsall.bat" 2^>nul') do (
    set VC_PATH=%%i
    goto :found_vc
)

echo No se encontro Visual Studio instalado.
goto :error

:found_vc
if not defined VC_PATH (
    set VC_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat
)
echo Encontrado: %VC_PATH%
call "%VC_PATH%" x64

if errorlevel 1 (
    echo Error al inicializar el entorno de Visual Studio.
    goto :error
)

echo Entorno de Visual Studio inicializado correctamente.

:: Compilar recursos
echo Compilando recursos...
rc resources.rc
if errorlevel 1 (
    echo Error al compilar recursos.
    goto :error
)

:: Compilar codigo fuente
echo Compilando codigo fuente...
cl main.c resources.res user32.lib gdi32.lib comctl32.lib comdlg32.lib ole32.lib /Fe:Micronotes.exe /W4
if errorlevel 1 (
    echo Error al compilar codigo fuente.
    goto :error
)

echo Compilacion completa exitosamente!
goto :end

:error
echo La compilacion fallo.

:end
pause