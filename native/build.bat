@echo off
REM Build Native Light (~400KB single EXE, no Qt, no installer)
REM Requires: Visual Studio Build Tools OR MinGW

echo === Veyo Updater — Native Win32 Build ===

where cmake >nul 2>&1
if %errorlevel%==0 (
    echo [CMake] Configuring...
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
    if %errorlevel% neq 0 (
        echo Try MinGW...
        cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
    )
    echo [CMake] Building...
    cmake --build build --config Release --parallel
    if exist "build\dist\VeyoUpdater.exe" (
        echo ✅ Built: build\dist\VeyoUpdater.exe
        for %%A in ("build\dist\VeyoUpdater.exe") do echo Size: %%~zA bytes
    ) else if exist "build\VeyoUpdater.exe" (
        echo ✅ Built: build\VeyoUpdater.exe
    )
    goto :eof
)

REM Fallback: direct cl
where cl >nul 2>&1
if %errorlevel%==0 (
    echo [MSVC cl] Building single file...
    call cl /utf-8 /MT /O2 /GL /EHsc /DUNICODE /D_UNICODE src\main.cpp comctl32.lib dwmapi.lib uxtheme.lib user32.lib gdi32.lib advapi32.lib shell32.lib /FeVeyoUpdater.exe /link /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO
    echo ✅ Built: VeyoUpdater.exe
    goto :eof
)

REM MinGW fallback
where g++ >nul 2>&1
if %errorlevel%==0 (
    echo [MinGW g++] Building...
    g++ -O2 -std=c++17 -static src/main.cpp -o VeyoUpdater.exe -municode -mwindows -lcomctl32 -ldwmapi -luxtheme -ladvapi32 -lshell32 -s
    echo ✅ Built: VeyoUpdater.exe
    goto :eof
)

echo ❌ No compiler found. Install Visual Studio Build Tools or MinGW.
echo    winget install Microsoft.VisualStudio.2022.BuildTools
