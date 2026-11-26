param(
    [string] $DirectXSdk,
    [string] $GameFolder,
    [ValidateSet('Win32', 'ARM64', 'x64')]
    [string] $Arch = 'Win32',
    [string] $FFmpeg,
    [string] $Zstd,
    [string] $CMake = 'cmake',
    [int] $Jobs = 6
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$is64 = $Arch -ne 'Win32'
$outputName = if ($is64) { "PC-$($Arch.ToUpper())-build" } else { 'PC-build' }
$output = Join-Path $repo "build-nx/$outputName"
New-Item -ItemType Directory -Path $output -Force | Out-Null

foreach ($mode in 'sp', 'mp') {
    $build = Join-Path $output $mode
    $singlePlayer = if ($mode -eq 'sp') { 'ON' } else { 'OFF' }
    $options = @('-DKISAK_DIRECT_CONNECT=ON', '-DKISAK_EXPERIMENTAL_SPLITSCREEN=ON', "-DKISAK_SP=$singlePlayer")
    if ($DirectXSdk) { $options += "-DDXSDK_DIR=$DirectXSdk" }
    if ($FFmpeg) { $options += "-DOPENBLOPS_FFMPEG_DIR=$FFmpeg" }
    if ($Zstd) { $options += "-DOPENBLOPS_ZSTD_DIR=$Zstd" }
    & $CMake -S $repo -B $build -G 'Visual Studio 17 2022' -A $Arch @options
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed for $mode." }
    & $CMake --build $build --config Release --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "The $mode build failed." }
}

if ($GameFolder) {
    $game = (Resolve-Path -LiteralPath $GameFolder).Path
    # 64-bit builds load the converted zones in kbz/ (tools/nx/convert-zones.sh).
    $links = if ($is64) { 'main', 'zone', 'kbz' } else { 'main', 'zone' }
    foreach ($name in $links) {
        $source = Join-Path $game $name
        if (-not (Test-Path -LiteralPath $source)) {
            Write-Warning "$source is missing."
            continue
        }
        $target = (Resolve-Path -LiteralPath $source).Path
        $link = Join-Path $output $name
        if (Test-Path -LiteralPath $link) {
            $existing = Get-Item -LiteralPath $link -Force
            if ($existing.LinkType -ne 'Junction' -or $existing.Target -ne $target) {
                throw "Existing game data at $link needs to be moved before linking $target."
            }
        } else {
            New-Item -ItemType Junction -Path $link -Target $target | Out-Null
        }
    }
    $files = if ($is64) { @('localization.txt') } else {
        @('localization.txt', 'binkw32.dll', 'Steam.dll', 'steamclient.dll', 'tier0_s.dll', 'vpx.dll', 'vstdlib_s.dll')
    }
    foreach ($name in $files) {
        $path = Join-Path $game $name
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $output }
    }
    $steamApi = Join-Path $repo 'src/steam/redistributable_bin/steam_api.dll'
    if (-not $is64 -and (Test-Path -LiteralPath $steamApi)) { Copy-Item -LiteralPath $steamApi -Destination $output }
}

Get-Item -LiteralPath (Join-Path $output 'OpenBLOPS.exe'), (Join-Path $output 'OpenBLOPS-MP.exe')
