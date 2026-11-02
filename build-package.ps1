param(
    [string]$Destination = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Noble Launcher')
)
$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$build = Join-Path $repo 'build'
$cmake = $null
foreach ($cand in @(
    'C:\Program Files\CMake\bin\cmake.exe',
    'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
    "$env:LOCALAPPDATA\Microsoft\WinGet\Links\cmake.exe"
)) {
    if (Test-Path $cand) { $cmake = $cand; break }
}
if (-not $cmake) {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { $cmake = $cmd.Source }
}
if (-not $cmake) { throw 'cmake.exe not found' }

$useNinjaMsvc = $false
if ((Test-Path "$env:LOCALAPPDATA\Programs\MSVC\VC\Tools\MSVC") -and -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $msvcVer = (Get-ChildItem "$env:LOCALAPPDATA\Programs\MSVC\VC\Tools\MSVC" -Directory | Select-Object -Last 1).Name
    $msvcDir = "$env:LOCALAPPDATA\Programs\MSVC\VC\Tools\MSVC\$msvcVer"
    $sdkDir  = 'C:\Program Files\Microsoft Visual Studio\2022\Community\SDK\ScopeCppSDK\vc15\SDK'
    if (-not (Test-Path "$sdkDir\include\um\Windows.h")) {
        $altSdk = 'C:\Program Files\Microsoft Visual Studio\18\Community\SDK\ScopeCppSDK\vc15\SDK'
        if (Test-Path "$altSdk\include\um\Windows.h") { $sdkDir = $altSdk }
    }
    $ninjaDir = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
    if (-not (Test-Path "$ninjaDir\ninja.exe")) { $ninjaDir = "$env:LOCALAPPDATA\Microsoft\WinGet\Links" }
    if (Test-Path "$msvcDir\bin\Hostx64\x64\cl.exe") {
        $env:PATH    = "$msvcDir\bin\Hostx64\x64;$sdkDir\bin;$ninjaDir;$env:PATH"
        $env:INCLUDE = "$msvcDir\include;$sdkDir\include\ucrt;$sdkDir\include\um;$sdkDir\include\shared"
        $env:LIB     = "$msvcDir\lib\x64;$sdkDir\lib"
        $useNinjaMsvc = $false
    }
}

if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    Write-Host '== Configuring CMake project =='
    if ($useNinjaMsvc) {
        & $cmake -S $repo -B $build -G 'Ninja' -DCMAKE_BUILD_TYPE=Release '-DCMAKE_EXE_LINKER_FLAGS=/machine:x64 /MANIFEST:NO' '-DCMAKE_SHARED_LINKER_FLAGS=/machine:x64 /MANIFEST:NO'
    } else {
        & $cmake -S $repo -B $build -A x64
    }
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
}

Write-Host '== Building clean release client and dev binaries =='
Stop-Process -Name 'Noble','noble-client','noble-dev' -Force -ErrorAction SilentlyContinue
& $cmake --build $build --config Release --target noble-client
if ($LASTEXITCODE -ne 0) { throw 'noble-client build failed' }
& $cmake --build $build --config Release --target noble-dev
if ($LASTEXITCODE -ne 0) { throw 'noble-dev build failed' }

$release = Join-Path $build 'bin\Release'
$clientExe = Join-Path $release 'noble-client.exe'
if (-not (Test-Path $clientExe)) { throw 'missing noble-client.exe' }

# Preserve existing user settings, keybinds, identity, and appearance into %APPDATA%\ProjectNoble\data
$appDataNoble = Join-Path $env:APPDATA 'ProjectNoble\data'
$appDataUser = Join-Path $appDataNoble 'userdata'
New-Item -ItemType Directory -Force -Path $appDataUser | Out-Null
foreach ($srcData in @((Join-Path $Destination 'data'), (Join-Path $release 'data'), (Join-Path $repo 'data'))) {
    if (-not (Test-Path $srcData)) { continue }
    foreach ($rel in @('userdata\settings.json', 'userdata\identity.json', 'appearance.dat')) {
        $srcFile = Join-Path $srcData $rel
        $dstFile = Join-Path $appDataNoble $rel
        if (Test-Path $srcFile) {
            if (-not (Test-Path $dstFile) -or ((Get-Item $srcFile).LastWriteTimeUtc -gt (Get-Item $dstFile).LastWriteTimeUtc)) {
                $dstDir = Split-Path -Parent $dstFile
                if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory -Force -Path $dstDir | Out-Null }
                Copy-Item $srcFile $dstFile -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

# Clean build directory of any stale nested duplicates, user data, armor, blocks, or master server scripts
foreach ($bad in @(
    (Join-Path $release 'data\data'),
    (Join-Path $release 'data\mods\mods'),
    (Join-Path $release 'data\maps\maps'),
    (Join-Path $release 'data\ui\custom\custom'),
    (Join-Path $release 'data\autosave'),
    (Join-Path $release 'data\sandbox'),
    (Join-Path $release 'data\userdata'),
    (Join-Path $release 'data\armor'),
    (Join-Path $release 'data\blocks'),
    (Join-Path $release 'data\launch_block_1.bin'),
    (Join-Path $release 'data\lb_m_c_1.bin'),
    (Join-Path $release 'data\profile_1.bin'),
    (Join-Path $release 'autosave'),
    (Join-Path $release 'userdata'),
    (Join-Path $release 'dedicated_data'),
    (Join-Path $release 'tools\master-server')
)) {
    if (Test-Path $bad) { Remove-Item $bad -Recurse -Force -ErrorAction SilentlyContinue }
}

# Sync clean repo data into build\bin\Release\data (excluding loose blocks and templates now compiled in)
$repoData = Join-Path $repo 'data'
if (Test-Path $repoData) {
    & robocopy $repoData (Join-Path $release 'data') /E /XO /XD userdata autosave cache sandbox armor blocks /XF launch_block_1.bin lb_m_c_1.bin profile_1.bin *.log *.dmp *.tmp /NFL /NDL /NJH /NJS /NP | Out-Null
}
$repoAssets = Join-Path $repo 'assets'
if (Test-Path $repoAssets) {
    & robocopy $repoAssets (Join-Path $release 'data') /E /XO /NFL /NDL /NJH /NJS /NP | Out-Null
}
$repoVgm = Join-Path $repo 'tools\vgmstream'
if (Test-Path $repoVgm) {
    & robocopy $repoVgm (Join-Path $release 'tools\vgmstream') /E /XO /NFL /NDL /NJH /NJS /NP | Out-Null
}

Write-Host "== Packaging fresh release to $Destination =="
# Completely remove old packaging directory so no stale files, developer IDs, or duplicate folders remain
if (Test-Path $Destination) {
    Write-Host "[*] Purging previous package directory..."
    Remove-Item $Destination -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $Destination | Out-Null

# 1. Copy client executable as Noble.exe ONLY (noble-dev.exe is NEVER copied!)
Copy-Item $clientExe (Join-Path $Destination 'Noble.exe') -Force

# 2. Copy data folder (excluding userdata, autosave, cache, sandbox, armor, blocks, and prebuilt bins)
$destData = Join-Path $Destination 'data'
New-Item -ItemType Directory -Force -Path $destData | Out-Null
& robocopy (Join-Path $release 'data') $destData /E /XD userdata autosave cache sandbox armor blocks /XF launch_block_1.bin lb_m_c_1.bin profile_1.bin *.log *.dmp *.tmp /NFL /NDL /NJH /NJS /NP | Out-Null

# 3. Copy tools\vgmstream
$destTools = Join-Path $Destination 'tools\vgmstream'
if (Test-Path (Join-Path $release 'tools\vgmstream')) {
    New-Item -ItemType Directory -Force -Path $destTools | Out-Null
    & robocopy (Join-Path $release 'tools\vgmstream') $destTools /E /NFL /NDL /NJH /NJS /NP | Out-Null
}

# 4. Extra cleanup pass to guarantee ZERO dev builds, master server scripts, or duplicate folders in package
foreach ($devFile in @('noble-dev.exe', 'noble-dev.map', 'noble-dedicated.exe', 'noble-client.exe', 'dedicated_server.json', 'master_server.js', 'test_master_server.js', 'launch_block_1.bin', 'lb_m_c_1.bin', 'profile_1.bin')) {
    $f = Join-Path $Destination $devFile
    if (Test-Path $f) { Remove-Item $f -Force -ErrorAction SilentlyContinue }
    $fData = Join-Path $destData $devFile
    if (Test-Path $fData) { Remove-Item $fData -Force -ErrorAction SilentlyContinue }
}
foreach ($badFolder in @('dedicated_data', 'tools\master-server', 'master-server', 'data\userdata', 'data\armor', 'data\blocks', 'userdata')) {
    $f = Join-Path $Destination $badFolder
    if (Test-Path $f) { Remove-Item $f -Recurse -Force -ErrorAction SilentlyContinue }
}
Get-ChildItem -Path $Destination -Recurse -Filter "*master*server*.js" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

# 5. Launcher script and version file
Set-Content (Join-Path $Destination 'play.bat') "@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" `"Noble.exe`"`r`n"
$versionMatch = Select-String -Path (Join-Path $repo 'src\common\Version.hpp') -Pattern 'VERSION_STRING = "([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+) \(([^)]*)\)"'
if (-not $versionMatch) {
    $versionMatch = Select-String -Path (Join-Path $repo 'src\common\Version.hpp') -Pattern 'VERSION_STRING = "([0-9]+\.[0-9]+\.[0-9]+) \(([^)]*)\)"'
}
$version = if ($versionMatch) { "v" + $versionMatch.Matches[0].Groups[1].Value + " (" + $versionMatch.Matches[0].Groups[2].Value + ")" } else { "dev" }
Set-Content (Join-Path $Destination 'version.txt') ("Noble " + $version + " release build " + (Get-Date -Format 'yyyy-MM-dd HH:mm'))

Write-Host '== Verification =='
Write-Host "Executable: $(Join-Path $Destination 'Noble.exe')"
Write-Host "Dev build in package? $(Test-Path (Join-Path $Destination 'noble-dev.exe'))"
Write-Host "User data in package? $(Test-Path (Join-Path $Destination 'data\userdata'))"
Write-Host "Armor directory in package? $(Test-Path (Join-Path $Destination 'data\armor'))"
Write-Host "Loose blocks directory in package? $(Test-Path (Join-Path $Destination 'data\blocks'))"
Write-Host "Loose .bin files in package? $((Get-ChildItem -Path $Destination -Recurse -Filter '*.bin' -ErrorAction SilentlyContinue | Measure-Object).Count -gt 0)"
Write-Host "Nested mods in package? $(Test-Path (Join-Path $Destination 'data\mods\mods'))"
Write-Host "Master server script in package? $((Get-ChildItem -Path $Destination -Recurse -Filter '*master*server*.js' -ErrorAction SilentlyContinue | Measure-Object).Count -gt 0)"
Write-Host '== Packaging Complete =='
$global:LASTEXITCODE = 0
exit 0
