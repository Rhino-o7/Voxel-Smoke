@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"
set "EMSDK_DIR=%SCRIPT_DIR%emsdk"
set "CONFIG=%~1"

if "%CONFIG%"=="" set "CONFIG=Release"

set "BUILD_DIR=%SCRIPT_DIR%..\build\bin\%CONFIG%\Client-Web"
set "RESP_FILE=%BUILD_DIR%\sources.rsp"

if exist "%EMSDK_DIR%\emsdk_env.bat" (
    call "%EMSDK_DIR%\emsdk_env.bat" >nul
)

where em++ >nul 2>&1
if errorlevel 1 (
    echo [ERROR] em++ not found in PATH.
    echo         Run emsdk_env.bat first, or keep emsdk in Client-Web\emsdk.
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

del "%RESP_FILE%" 2>nul

powershell -NoProfile -ExecutionPolicy Bypass -Command "$client = Get-ChildItem -Path '%SCRIPT_DIR%src' -Recurse -Filter *.cpp | ForEach-Object { ($_.FullName -replace '\\','/') }; $core = Get-ChildItem -Path '%REPO_ROOT%\CoreLib\src' -Recurse -Filter *.cpp | Where-Object { $_.FullName -notmatch '\\third_party\\glfw\\' -and $_.FullName -notmatch '\\third_party\\asio-1.12.1\\' -and $_.FullName -notmatch '\\third_party\\websocketpp-0.8.2\\' -and $_.FullName -notmatch '\\third_party\\glm\\test\\' -and $_.FullName -notmatch '\\third_party\\imgui\\backends\\' -and $_.FullName -notmatch '\\third_party\\imgui\\examples\\' -and $_.FullName -notmatch '\\third_party\\imgui\\misc\\' -and $_.FullName -notmatch '\\gl\\shader.cpp$' -and $_.FullName -notmatch '\\graphic\\display.cpp$' -and $_.FullName -notmatch '\\world\\world.cpp$' } | ForEach-Object { ($_.FullName -replace '\\','/') }; ($client + $core) | Set-Content -Path '%RESP_FILE%'"

if not exist "%RESP_FILE%" (
    echo [ERROR] Failed to generate source list.
    exit /b 1
)

set "COMMON_FLAGS=-std=c++20 -include thread -include mutex -include condition_variable -include functional -sUSE_GLFW=3 -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 -lwebsocket.js"
set "INCLUDE_FLAGS=-I%SCRIPT_DIR%src -I%REPO_ROOT%\CoreLib\src -I%REPO_ROOT%\CoreLib\src\third_party -I%REPO_ROOT%\CoreLib\src\third_party\glm -I%REPO_ROOT%\CoreLib\src\third_party\imgui -I%REPO_ROOT%\CoreLib\src\third_party\lodepng -I%REPO_ROOT%\CoreLib\src\third_party\websocketpp-0.8.2 -I%REPO_ROOT%\CoreLib\src\third_party\FastNoiseLite -I%REPO_ROOT%\CoreLib\src\third_party\FrustumCulling"
set "PRELOAD_FLAGS=--preload-file %REPO_ROOT%\CoreLib\resources@../CoreLib/resources --preload-file %SCRIPT_DIR%resources@../Client-Web/resources"
set "OUT_FILE=%BUILD_DIR%\index.html"

if /I "%CONFIG%"=="Debug" (
    set "OPT_FLAGS=-O0 -gsource-map -sASSERTIONS=2"
) else (
    set "OPT_FLAGS=-O2 -sASSERTIONS=1"
)

echo [INFO] Building Client-Web (%CONFIG%)...
em++ @"%RESP_FILE%" %COMMON_FLAGS% %OPT_FLAGS% %INCLUDE_FLAGS% %PRELOAD_FLAGS% -o "%OUT_FILE%"
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [OK] Build complete: "%OUT_FILE%"
echo [TIP] Run in browser with: emrun "%OUT_FILE%"

endlocal
