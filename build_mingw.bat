@echo off
echo Compilando Micronotes con MinGW...

:: Verificar si windres está disponible
where windres >nul 2>&1
if errorlevel 1 (
    echo Error: MinGW-w64 no encontrado en PATH.
    echo Por favor instale MinGW-w64 y asegurese de que este en PATH.
    pause
    exit /b 1
)

:: Compilar recursos
echo Compilando recursos...
windres resources.rc -O coff -o resources.res
if errorlevel 1 (
    echo Error al compilar recursos.
    pause
    exit /b 1
)

:: Compilar codigo fuente
echo Compilando codigo fuente...
gcc main.c resources.res -o Micronotes.exe -lcomctl32 -lcomdlg32 -lole32 -lmsftedit -mwindows
if errorlevel 1 (
    echo Error al compilar codigo fuente.
    pause
    exit /b 1
)

echo Compilacion completa exitosamente!
pause