param(
    [switch]$Release = $true,
    [string]$QtDir = ""
)

$ErrorActionPreference = "Stop"

Write-Host "=== VeyoUpdater Build (Qt6 + CMake) ===" -ForegroundColor Cyan

# پیدا کردن Qt
if (-not $QtDir) {
    $candidates = @(
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.7.3\msvc2022_64",
        "C:\Qt\6.7.0\msvc2019_64",
        "C:\Qt\6.6.3\msvc2019_64",
        "C:\Qt\6.5.3\msvc2019_64"
    )
    foreach ($c in $candidates) { if (Test-Path "$c\bin\qmake.exe") { $QtDir = $c; break } }
    if (-not $QtDir) {
        # جستجوی عمومی
        $found = Get-ChildItem "C:\Qt" -Recurse -Filter "qmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $QtDir = Split-Path (Split-Path $found.FullName -Parent) -Parent }
    }
}

if (-not $QtDir -or -not (Test-Path "$QtDir\bin\qmake.exe")) {
    Write-Host "Qt6 پیدا نشد!" -ForegroundColor Red
    Write-Host @"
راهنما:
  1) Qt را از https://www.qt.io/download-qt-installer نصب کن (Qt 6.5+ با MSVC)
     یا با aqtinstall:
       pip install aqtinstall
       aqt install-qt windows desktop 6.8.0 win64_msvc2022_64 -m qtbase

  2) سپس:
       .\build.ps1 -QtDir "C:\Qt\6.8.0\msvc2022_64"

  3) پیش‌نیازها:
       - Visual Studio 2022 (Build Tools) + CMake + Ninja
       - winget نصب باشد (پیش‌فرض ویندوز 11)

"@ -ForegroundColor Yellow
    exit 1
}

Write-Host "Qt: $QtDir" -ForegroundColor Green
$env:CMAKE_PREFIX_PATH = $QtDir

# پیدا کردن CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $cmakeCandidates = @("C:\Program Files\CMake\bin\cmake.exe", "C:\Program Files (x86)\CMake\bin\cmake.exe")
    foreach ($c in $cmakeCandidates) { if (Test-Path $c) { $cmake = Get-Item $c; break } }
}
if (-not $cmake) { Write-Host "CMake پیدا نشد! از https://cmake.org/download نصب کن" -ForegroundColor Red; exit 1 }

$buildType = if ($Release) { "Release" } else { "Debug" }
$buildDir = "build"

if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }

# پیدا کردن MSVC
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) { Write-Host "VS: $vsPath" -ForegroundColor Green }
}

Write-Host "Configuring ($buildType)..." -ForegroundColor Cyan
& $cmake.Source configure -S . -B $buildDir -DCMAKE_BUILD_TYPE=$buildType -DCMAKE_PREFIX_PATH="$QtDir" -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "سعی با Ninja..." -ForegroundColor Yellow
    & $cmake.Source -S . -B $buildDir -DCMAKE_BUILD_TYPE=$buildType -DCMAKE_PREFIX_PATH="$QtDir" -G Ninja
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Building..." -ForegroundColor Cyan
& $cmake.Source --build $buildDir --config $buildType --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $buildDir "$buildType\VeyoUpdater.exe"
if (-not (Test-Path $exe)) { $exe = Join-Path $buildDir "VeyoUpdater.exe" }
Write-Host "✅ Build done: $exe" -ForegroundColor Green
Write-Host "حجم:" -NoNewline; (Get-Item $exe).Length / 1MB | ForEach-Object { Write-Host (" {0:N2} MB" -f $_) -ForegroundColor Cyan }

# windeployqt
$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (Test-Path $windeployqt) {
    Write-Host "Running windeployqt..." -ForegroundColor Cyan
    & $windeployqt --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
}

Write-Host "اجرا: & `"$exe`"" -ForegroundColor Yellow
