# ──────────────────────────────────────────────
#  StudyRoom — install script (Windows)
#  Requires: Visual Studio 2022, vcpkg, CMake
# ──────────────────────────────────────────────

$ErrorActionPreference = "Stop"

function Write-Header {
    Clear-Host
    Write-Host ""
    Write-Host "   ╔══════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "   ║        S T U D Y R O O M        ║" -ForegroundColor Cyan
    Write-Host "   ║   synchronized listening space  ║" -ForegroundColor Cyan
    Write-Host "   ╚══════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "   Setting up your client..." -ForegroundColor DarkGray
    Write-Host ""
}

function Write-Step($n, $total, $msg) {
    Write-Host ""
    Write-Host "  [$n/$total] $msg" -ForegroundColor Cyan
}

function Write-Ok($msg)   { Write-Host "    ✓  $msg" -ForegroundColor Green }
function Write-Info($msg) { Write-Host "    →  $msg" -ForegroundColor DarkGray }
function Write-Warn($msg) { Write-Host "    ⚠  $msg" -ForegroundColor Yellow }
function Write-Fail($msg) {
    Write-Host ""
    Write-Host "    ✗  $msg" -ForegroundColor Red
    Write-Host ""
    exit 1
}

Write-Header

# ── Step 1: Check tools ──────────────────────────
Write-Step 1 4 "Checking build tools"

# CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Fail "CMake not found. Download from https://cmake.org/download/ and re-run."
}
$cmakeVer = (cmake --version | Select-Object -First 1) -replace "cmake version ", ""
Write-Ok "cmake $cmakeVer"

# Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    Write-Fail "Visual Studio not found. Install Visual Studio 2022 with 'Desktop development with C++' workload."
}
$vsPath = & $vsWhere -latest -property installationPath
if (-not $vsPath) {
    Write-Fail "Visual Studio 2022 not found."
}
Write-Ok "Visual Studio found at $vsPath"

# vcpkg
$vcpkgCmd = Get-Command vcpkg -ErrorAction SilentlyContinue
if (-not $vcpkgCmd) {
    # Спробуємо знайти vcpkg у типових місцях
    $commonPaths = @(
        "C:\vcpkg\vcpkg.exe",
        "$env:USERPROFILE\vcpkg\vcpkg.exe",
        "C:\dev\vcpkg\vcpkg.exe"
    )
    $vcpkgExe = $commonPaths | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $vcpkgExe) {
        Write-Warn "vcpkg not found in PATH or common locations."
        Write-Info "Installing vcpkg..."
        $vcpkgDir = "$env:USERPROFILE\vcpkg"
        git clone https://github.com/microsoft/vcpkg.git $vcpkgDir
        & "$vcpkgDir\bootstrap-vcpkg.bat" -disableMetrics
        $env:VCPKG_ROOT = $vcpkgDir
        Write-Ok "vcpkg installed at $vcpkgDir"
    } else {
        $env:VCPKG_ROOT = Split-Path $vcpkgExe
        Write-Ok "vcpkg found at $vcpkgExe"
    }
} else {
    $env:VCPKG_ROOT = Split-Path $vcpkgCmd.Source
    Write-Ok "vcpkg $(vcpkg version | Select-Object -First 1)"
}

# ── Step 2: Install dependencies ─────────────────
Write-Step 2 4 "Installing dependencies (via vcpkg)"

$deps = @("boost-asio", "boost-endian", "opus", "portaudio")
foreach ($dep in $deps) {
    Write-Info "Checking $dep..."
    $installed = & "$env:VCPKG_ROOT\vcpkg.exe" list $dep 2>$null
    if ($installed -match $dep) {
        Write-Ok "$dep (already installed)"
    } else {
        Write-Info "Installing $dep..."
        & "$env:VCPKG_ROOT\vcpkg.exe" install "${dep}:x64-windows"
        Write-Ok "$dep"
    }
}

# ── Step 3: Build ────────────────────────────────
Write-Step 3 4 "Building StudyRoom client"

Write-Info "Configuring with CMake..."
$toolchain = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake -B build\windows-msvc `
      -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
      -DBUILD_SERVER=OFF `
      -DBUILD_TESTING=OFF `
      -A x64 | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Fail "CMake configuration failed."
}

Write-Info "Compiling..."
cmake --build build\windows-msvc --target client --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Fail "Build failed. Run the cmake command manually for details."
}

$binaryPath = "build\windows-msvc\client\Release\client.exe"
if (-not (Test-Path $binaryPath)) {
    Write-Fail "Build succeeded but binary not found at $binaryPath"
}
Write-Ok "Build complete"

# ── Step 4: Create launcher ──────────────────────
Write-Step 4 4 "Creating launcher"

$launcherPath = Join-Path (Get-Location) "studyroom.ps1"
$fullBinaryPath = Join-Path (Get-Location) $binaryPath

@"
# StudyRoom launcher
& "$fullBinaryPath" @args
"@ | Set-Content $launcherPath -Encoding UTF8

Write-Ok "Created studyroom.ps1"

# Додаємо функцію в PowerShell профіль
$profileDir = Split-Path $PROFILE
if (-not (Test-Path $profileDir)) {
    New-Item -ItemType Directory -Path $profileDir -Force | Out-Null
}
if (-not (Test-Path $PROFILE)) {
    New-Item -ItemType File -Path $PROFILE -Force | Out-Null
}

$profileContent = Get-Content $PROFILE -Raw -ErrorAction SilentlyContinue
$funcEntry = "function studyroom { & '$fullBinaryPath' @args }"

if ($profileContent -notmatch "function studyroom") {
    Add-Content $PROFILE "`n# StudyRoom`n$funcEntry"
    Write-Ok "Added 'studyroom' function to PowerShell profile"
    Write-Info "Restart PowerShell or run '. `$PROFILE' to use it"
} else {
    Write-Ok "studyroom function already in profile"
}

# ── Done ─────────────────────────────────────────
Write-Host ""
Write-Host "   ╔══════════════════════════════════╗" -ForegroundColor Green
Write-Host "   ║     ✓  Ready to listen!         ║" -ForegroundColor Green
Write-Host "   ╚══════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "  Start the app:  " -NoNewline
Write-Host ".\studyroom.ps1" -ForegroundColor Cyan
Write-Host "  After restart:  " -NoNewline
Write-Host "studyroom" -ForegroundColor Cyan -NoNewline
Write-Host "  (function added to your profile)" -ForegroundColor DarkGray
Write-Host ""

$desktopPath = [Environment]::GetFolderPath("Desktop")
$shortcutPath = Join-Path $desktopPath "StudyRoom.lnk"

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = "powershell.exe"
$shortcut.Arguments = "-NoExit -Command `"& '$fullBinaryPath'`""
$shortcut.WorkingDirectory = (Get-Location).Path
$shortcut.Description = "StudyRoom - synchronized listening"
$shortcut.Save()

Write-Ok "Created StudyRoom shortcut on Desktop"

$iconPng = Join-Path (Get-Location) "assets\icon.png"
if (Test-Path $iconPng) {
    Add-Type -AssemblyName System.Drawing

    $png = [System.Drawing.Image]::FromFile($iconPng)
    $icoPath = Join-Path (Get-Location) "assets\icon.ico"

    # Створюємо ico з кількох розмірів
    $sizes = @(16, 32, 48, 64, 128, 256)
    $memStream = New-Object System.IO.MemoryStream

    # ICO header
    $writer = New-Object System.IO.BinaryWriter($memStream)
    $writer.Write([uint16]0)      # reserved
    $writer.Write([uint16]1)      # type: ico
    $writer.Write([uint16]$sizes.Count)  # image count

    $imageStreams = @()
    $offset = 6 + $sizes.Count * 16  # header + directory size

    foreach ($size in $sizes) {
        $bmp = New-Object System.Drawing.Bitmap($png, $size, $size)
        $imgStream = New-Object System.IO.MemoryStream
        $bmp.Save($imgStream, [System.Drawing.Imaging.ImageFormat]::Png)
        $imageStreams += $imgStream
        $bmp.Dispose()

        $bytes = $imgStream.ToArray()
        $writer.Write([byte]($size -eq 256 ? 0 : $size))
        $writer.Write([byte]($size -eq 256 ? 0 : $size))
        $writer.Write([byte]0)   # color count
        $writer.Write([byte]0)   # reserved
        $writer.Write([uint16]1) # planes
        $writer.Write([uint16]32) # bit count
        $writer.Write([uint32]$bytes.Length)
        $writer.Write([uint32]$offset)
        $offset += $bytes.Length
    }

    foreach ($imgStream in $imageStreams) {
        $writer.Write($imgStream.ToArray())
        $imgStream.Dispose()
    }

    [System.IO.File]::WriteAllBytes($icoPath, $memStream.ToArray())
    $png.Dispose()
    $writer.Dispose()

    $shortcut.IconLocation = $icoPath
    Write-Ok "Applied custom icon to shortcut"
} else {

    $shortcut.IconLocation = "C:\Windows\System32\imageres.dll,105"
}
