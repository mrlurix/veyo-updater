param(
    [string]$QtDir = "",
    [string]$BuildType = "Release",
    [switch]$NoSingleExe,       # فقط ZIP پرتابل بساز، تک‌فایل نساز
    [switch]$UseUPX,            # فشرده‌سازی با UPX اگر نصب باشد
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Write-Host "=== VeyoUpdater — Portable Packager ===" -ForegroundColor Cyan
Write-Host "BuildType: $BuildType | UPX: $UseUPX | SingleExe: $(-not $NoSingleExe)" -ForegroundColor Gray

# 1) بیلد
$buildArgs = @{}
if ($QtDir) { $buildArgs.QtDir = $QtDir }
if ($BuildType -eq "Debug") { $buildArgs.Release = $false }

Write-Host "`n[1/5] Building..." -ForegroundColor Yellow
& "$PSScriptRoot\build.ps1" @buildArgs
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed!" -ForegroundColor Red; exit $LASTEXITCODE }

# پیدا کردن exe
$exeCandidates = @(
    "build\dist\VeyoUpdater.exe",
    "build\$BuildType\VeyoUpdater.exe",
    "build\VeyoUpdater.exe"
)
$exe = $null
foreach ($c in $exeCandidates) { if (Test-Path $c) { $exe = (Resolve-Path $c).Path; break } }
if (-not $exe) { Write-Host "exe پیدا نشد! کاندیدها: $($exeCandidates -join ', ')" -ForegroundColor Red; exit 1 }
Write-Host "exe: $exe" -ForegroundColor Green

# 2) windeployqt (اگر build.ps1 قبلاً انجام نداده باشد)
if (-not $QtDir) {
    $candidates = @("C:\Qt\6.8.0\msvc2022_64","C:\Qt\6.7.3\msvc2022_64","C:\Qt\6.7.0\msvc2019_64","C:\Qt\6.5.3\msvc2019_64")
    foreach ($c in $candidates) { if (Test-Path "$c\bin\qmake.exe") { $QtDir = $c; break } }
    if (-not $QtDir) {
        $found = Get-ChildItem "C:\Qt" -Recurse -Filter "windeployqt.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $QtDir = Split-Path (Split-Path $found.FullName -Parent) -Parent }
    }
}
$windeployqt = $null
if ($QtDir) { $windeployqt = Join-Path $QtDir "bin\windeployqt.exe" }
if (-not $windeployqt -or -not (Test-Path $windeployqt)) {
    $windeployqt = Get-Command windeployqt -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}
if ($windeployqt -and (Test-Path $windeployqt)) {
    Write-Host "`n[2/5] windeployqt --no-translations --no-system-d3d-compiler --no-opengl-sw" -ForegroundColor Yellow
    & $windeployqt --no-translations --no-system-d3d-compiler --no-opengl-sw --no-serialport --no-positioning --no-qmltooling "$exe"
    if ($LASTEXITCODE -ne 0) { Write-Host "windeployqt هشدار: $LASTEXITCODE" -ForegroundColor Yellow }
} else {
    Write-Host "windeployqt پیدا نشد — از DLLهای کنار exe استفاده می‌شود" -ForegroundColor Yellow
}

# 3) تمیزکاری برای حجم کم
$exeDir = Split-Path $exe -Parent
Write-Host "`n[3/5] Trimming for minimal size..." -ForegroundColor Yellow

# حذف فایل‌های غیرضروری که windeployqt گاهی می‌آورد
$removePatterns = @("*.pdb","*d.dll","opengl32sw.dll","d3dcompiler_*.dll")
foreach ($pat in $removePatterns) {
    Get-ChildItem $exeDir -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  حذف $($_.Name)" -ForegroundColor DarkGray
        Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
    }
}

# حذف پلتفرم‌های اضافی
if (Test-Path "$exeDir\translations") { Remove-Item "$exeDir\translations" -Recurse -Force -ErrorAction SilentlyContinue }

# ساخت Qt.conf برای پرتابل بودن
$qtConf = @"
[Paths]
Prefix = .
Plugins = plugins
Libraries = .
"@
Set-Content -Path "$exeDir\Qt.conf" -Value $qtConf -Encoding ASCII
Write-Host "  Qt.conf ساخته شد" -ForegroundColor Green

# UPX اگر درخواست شده
if ($UseUPX) {
    $upx = Get-Command upx -ErrorAction SilentlyContinue
    if ($upx) {
        Write-Host "  UPX compress..." -ForegroundColor Yellow
        Get-ChildItem $exeDir -Filter "*.dll" | ForEach-Object { & upx --best --lzma $_.FullName 2>$null | Out-Null }
        & upx --best --lzma "$exe" 2>$null | Out-Null
    } else {
        Write-Host "  UPX نصب نیست — رد شد (نصب: winget install upx)" -ForegroundColor DarkYellow
    }
}

# آمار حجم
$total = (Get-ChildItem $exeDir -Recurse | Measure-Object -Property Length -Sum).Sum
$exeSize = (Get-Item $exe).Length
Write-Host "  exe: $([math]::Round($exeSize/1MB,2)) MB | کل پوشه: $([math]::Round($total/1MB,2)) MB" -ForegroundColor Cyan

# 4) ساخت ZIP پرتابل
Write-Host "`n[4/5] Creating Portable ZIP..." -ForegroundColor Yellow
$portableRoot = Join-Path $PSScriptRoot $OutDir
if (-not (Test-Path $portableRoot)) { New-Item -ItemType Directory -Path $portableRoot | Out-Null }

$version = "1.0.0"
try { $v = (Get-Item $exe).VersionInfo.ProductVersion; if ($v) { $version = $v } } catch {}

$zipName = "VeyoUpdater_v${version}_Portable.zip"
$zipPath = Join-Path $portableRoot $zipName

# پوشه‌ی پرتابل تمیز
$stage = Join-Path $env:TEMP "VeyoUpdater_Portable_Stage"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item "$exeDir\*" -Destination $stage -Recurse -Force

# افزودن README پرتابل
@"
VeyoUpdater v$version — Portable
===============================
• نیازی به نصب ندارد — پوشه را هرجا کپی کن و VeyoUpdater.exe را اجرا کن.
• تنظیمات در کنار exe ذخیره نمی‌شود (پرتابل واقعی).
• نیاز: Windows 10/11 + winget (پیش‌فرض)
• برای آپدیت همه: دکمه «⚡ آپدیت همه»

ساخت: $(Get-Date -Format "yyyy-MM-dd HH:mm")
Qt: $QtDir
"@ | Set-Content -Path "$stage\README-Portable.txt" -Encoding UTF8

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path "$stage\*" -DestinationPath $zipPath -CompressionLevel Optimal -Force
Write-Host "  ✅ ZIP: $zipPath" -ForegroundColor Green
Write-Host "  حجم ZIP: $([math]::Round((Get-Item $zipPath).Length/1MB,2)) MB" -ForegroundColor Cyan

# 5) ساخت تک‌فایل EXE (SFX) — بدون نیاز به Enigma
if (-not $NoSingleExe) {
    Write-Host "`n[5/5] Creating Single-File Portable EXE (SFX)..." -ForegroundColor Yellow

    # روش 1: 7-Zip SFX (اگر 7z نصب باشد) — بهترین و رایگان
    $sevenZip = @(
        "C:\Program Files\7-Zip\7z.exe",
        "C:\Program Files (x86)\7-Zip\7z.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $sevenZip) {
        $sevenZipCmd = Get-Command 7z -ErrorAction SilentlyContinue
        if ($sevenZipCmd) { $sevenZip = $sevenZipCmd.Source }
    }

    $sfxExe = Join-Path $portableRoot "VeyoUpdater_v${version}_Single.exe"

    if ($sevenZip) {
        Write-Host "  7-Zip یافت شد: $sevenZip" -ForegroundColor Green
        $tmp7z = Join-Path $env:TEMP "VeyoUpdater.7z"
        $sfxModule = Join-Path (Split-Path $sevenZip -Parent) "7z.sfx"
        # fallback: اگر sfx پیدا نشد، از 7zCon.sfx استفاده کن
        if (-not (Test-Path $sfxModule)) {
            $sfxModule = Get-ChildItem (Split-Path $sevenZip -Parent) -Filter "*.sfx" | Select-Object -First 1 | ForEach-Object { $_.FullName }
        }

        if ($sfxModule -and (Test-Path $sfxModule)) {
            # ساخت 7z
            & $sevenZip a -t7z -mx=9 -m0=lzma2 -ms=on $tmp7z "$stage\*" -y | Out-Null
            # config برای SFX
            $config = @"
;!@Install@!UTF-8!
Title="Veyo Updater Portable"
BeginPrompt="Extract and run VeyoUpdater?"
RunProgram="VeyoUpdater.exe"
;!@InstallEnd@!
"@
            $configPath = Join-Path $env:TEMP "sfx_config.txt"
            Set-Content -Path $configPath -Value $config -Encoding UTF8

            # ترکیب: sfx + config + 7z = exe
            $sfxBytes = [IO.File]::ReadAllBytes($sfxModule)
            $cfgBytes = [IO.File]::ReadAllBytes($configPath)
            $arcBytes = [IO.File]::ReadAllBytes($tmp7z)
            $outStream = [IO.File]::Create($sfxExe)
            $outStream.Write($sfxBytes, 0, $sfxBytes.Length)
            $outStream.Write($cfgBytes, 0, $cfgBytes.Length)
            $outStream.Write($arcBytes, 0, $arcBytes.Length)
            $outStream.Close()

            Write-Host "  ✅ Single EXE (7z SFX): $sfxExe" -ForegroundColor Green
            Write-Host "  حجم: $([math]::Round((Get-Item $sfxExe).Length/1MB,2)) MB" -ForegroundColor Cyan
            Remove-Item $tmp7z -Force -ErrorAction SilentlyContinue
        } else {
            Write-Host "  sfx module پیدا نشد — فقط ZIP ساخته شد" -ForegroundColor Yellow
            Write-Host "  نصب پیشنهادی: winget install 7zip.7zip" -ForegroundColor Gray
        }
    } else {
        Write-Host "  7-Zip نصب نیست — Single EXE ساخته نشد" -ForegroundColor Yellow
        Write-Host "  برای تک‌فایل:" -ForegroundColor Gray
        Write-Host "    winget install 7zip.7zip  -> سپس دوباره .\pack-portable.ps1 را بزن" -ForegroundColor Gray
        Write-Host "  یا از Enigma Virtual Box (رایگان) استفاده کن:" -ForegroundColor Gray
        Write-Host "    https://enigmaprotector.com/en/about/enigma_virtual_box.html" -ForegroundColor Gray
        Write-Host "    فایل‌های $stage را به EVB بده تا یک exe بسازد." -ForegroundColor Gray

        # fallback: ساخت SFX با IExpress (پیش‌فرض ویندوز) — بدون نیاز به نصب
        Write-Host "  تلاش با IExpress (پیش‌فرض ویندوز)..." -ForegroundColor DarkCyan
        try {
            $iexpressSfx = Join-Path $portableRoot "VeyoUpdater_v${version}_Portable-SFX.exe"
            # IExpress نیاز به SED دارد — ساده‌ترین: فقط هشدار بده
            Write-Host "  IExpress به صورت دستی: iexpress.exe -> Create new SED -> کپی فایل‌ها" -ForegroundColor Gray
        } catch {}
    }

    Write-Host "`n💡 نکته: نسخه ZIP خودش «پرتابل» است — پوشه را هرجا کپی کن، بدون نصب اجرا می‌شود." -ForegroundColor Cyan
    Write-Host "   نسخه Single EXE فقط برای راحتی حمل به عنوان یک فایل است (خودکار extract و اجرا)." -ForegroundColor Cyan
}

Write-Host "`n=== Done ===" -ForegroundColor Green
Write-Host "ZIP Portable: $zipPath" -ForegroundColor White
if (Test-Path $sfxExe) { Write-Host "Single EXE : $sfxExe" -ForegroundColor White }
Write-Host "پوشه‌ی پرتابل آماده: $stage" -ForegroundColor DarkGray

# نمایش محتوا
Get-ChildItem $stage | Format-Table Name, Length -AutoSize | Out-String | Write-Host
