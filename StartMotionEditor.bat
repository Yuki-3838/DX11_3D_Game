@echo off
setlocal
set "PROJECT_ROOT=%~dp0"
cd /d "%PROJECT_ROOT%"

set "EDITOR_EXE=tools\MotionEditor.Wpf\bin\Debug\net8.0-windows\DX11MotionEditor.exe"
set "EDITOR_PROJECT=tools\MotionEditor.Wpf\MotionEditor.Wpf.csproj"

if exist "%EDITOR_EXE%" (
    "%PROJECT_ROOT%%EDITOR_EXE%"
    if errorlevel 1 (
        echo.
        echo Motion Editor exited with an error.
        echo Check %%TEMP%%\DX11MotionEditor-error.log for details.
        pause
    )
    exit /b %errorlevel%
)

echo Motion Editor is not built. Building now...
dotnet build "%EDITOR_PROJECT%" --configuration Debug
if errorlevel 1 (
    echo.
    echo Build failed. Keep this window open to read the error above.
    pause
    exit /b 1
)

"%PROJECT_ROOT%%EDITOR_EXE%"
if errorlevel 1 (
    echo.
    echo Motion Editor exited with an error.
    echo Check %%TEMP%%\DX11MotionEditor-error.log for details.
    pause
)
endlocal
