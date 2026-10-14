<#
.SYNOPSIS
  Builds, tests and packages Ultima V Native for Windows (x64, MSVC, SDL3).

.DESCRIPTION
  Finds Visual Studio (vswhere), loads its x64 build environment, configures
  native/targets/windows with CMake + Ninja, builds, optionally runs the tests
  and makes the player ZIP:

    dist\UltimaV-Native-Windows-x64-dev.zip

  The ZIP holds ONLY whitelisted files: the executable, SDL3.dll, the player
  documents, the licences, the asset-preparation script and BUILD-INFO.txt.
  No Ultima V data is ever copied into it.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File native\targets\windows\tools\build-windows.ps1 -Config Release -Test -Package
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')] [string]$Config = 'Release',
    [string]$BuildDir = '',
    [switch]$Test,
    [switch]$Package,
    [string]$PackageName = 'UltimaV-Native-Windows-x64-dev'
)
$ErrorActionPreference = 'Stop'

$WinDir = Split-Path -Parent $PSScriptRoot                       # native/targets/windows
$RepoRoot = (Resolve-Path (Join-Path $WinDir '..\..\..')).Path
if (-not $BuildDir) { $BuildDir = Join-Path $WinDir ("build-" + $Config.ToLower()) }
$DistDir = Join-Path $WinDir 'dist'

function Step($text) { Write-Host "==> $text" -ForegroundColor Cyan }

# --- Visual Studio environment ------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: install Visual Studio 2022/2026 with 'Desktop development with C++'." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "No Visual Studio with the C++ x64 tools was found." }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
Step "Visual Studio: $vs"
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $env:PATH = (Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja') + ';' + $env:PATH
}

# --- Configure and build ---------------------------------------------------------
Step "Configure ($Config) -> $BuildDir"
& $cmake -S $WinDir -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
if ($LASTEXITCODE) { throw "CMake configure failed" }
Step "Build"
& $cmake --build $BuildDir
if ($LASTEXITCODE) { throw "Build failed" }
$exe = Join-Path $BuildDir 'UltimaVNative.exe'
$sdl = Join-Path $BuildDir 'SDL3.dll'
foreach ($f in @($exe, $sdl)) { if (-not (Test-Path $f)) { throw "Missing build output: $f" } }

# --- Tests -----------------------------------------------------------------------
if ($Test) {
    Step "Tests"
    Push-Location $BuildDir
    try { & $ctest --output-on-failure; if ($LASTEXITCODE) { throw "Tests failed" } } finally { Pop-Location }
}

# --- Package ---------------------------------------------------------------------
if ($Package) {
    Step "Package $PackageName.zip"
    $stage = Join-Path $DistDir $PackageName
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    # The asset-preparation script: one bundled file, built from the project's
    # own TypeScript tooling (native/tools/asset-prep), run by the player's Node.js.
    $bundle = Join-Path $RepoRoot 'native\tools\asset-prep\dist\prepare-assets.mjs'
    $builder = Join-Path $RepoRoot 'native\tools\asset-prep\build.mjs'
    if ((Test-Path $builder) -and (Get-Command node -ErrorAction SilentlyContinue) -and (Test-Path (Join-Path $RepoRoot 'node_modules'))) {
        Push-Location $RepoRoot
        try { & node $builder; if ($LASTEXITCODE) { throw "asset-prep bundle build failed" } } finally { Pop-Location }
    }
    if (-not (Test-Path $bundle)) { throw "The asset-preparation bundle is missing ($bundle). Run 'node native/tools/asset-prep/build.mjs' from a checkout with node_modules." }

    $sdlLicense = Get-ChildItem -Path (Join-Path $BuildDir '_deps') -Filter 'LICENSE.txt' -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match 'sdl3-src' } | Select-Object -First 1
    $files = [ordered]@{
        'UltimaVNative.exe'      = $exe
        'SDL3.dll'               = $sdl
        'README-Windows.txt'     = Join-Path $WinDir 'packaging\README-Windows.txt'
        'CONTROLS.txt'           = Join-Path $WinDir 'packaging\CONTROLS.txt'
        'Prepare-Game-Data.cmd'  = Join-Path $WinDir 'packaging\Prepare-Game-Data.cmd'
        'prepare-assets.mjs'     = $bundle
        'LICENSE.txt'            = Join-Path $RepoRoot 'LICENSE'
        'NOTICE.txt'             = Join-Path $RepoRoot 'NOTICE'
    }
    foreach ($name in $files.Keys) {
        if (-not (Test-Path $files[$name])) { throw "Missing package input: $($files[$name])" }
        Copy-Item -Path $files[$name] -Destination (Join-Path $stage $name)
    }
    $third = @(
        'Third-party software in this package',
        '=====================================',
        '',
        'SDL3 (Simple DirectMedia Layer) 3.4.16 -- SDL3.dll',
        'https://www.libsdl.org/  -- zlib licence, reproduced below.',
        '',
        'prepare-assets.mjs bundles, besides this project''s own GPL-3.0 tooling:',
        '  pngjs (MIT licence) https://github.com/lukeapage/pngjs',
        '  fflate (MIT licence) https://github.com/101arrowz/fflate',
        '',
        '---------------------------------------------------------------- SDL3',
        ''
    )
    if ($sdlLicense) { $third += Get-Content -Path $sdlLicense.FullName } else { $third += '(SDL3 LICENSE.txt not found in the build tree)' }
    foreach ($pkg in @('pngjs', 'fflate')) {
        $lic = Get-ChildItem -Path (Join-Path $RepoRoot "node_modules\$pkg") -Filter 'LICENSE*' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($lic) { $third += @('', "---------------------------------------------------------------- $pkg", ''); $third += Get-Content -Path $lic.FullName }
    }
    Set-Content -Path (Join-Path $stage 'THIRD-PARTY-LICENSES.txt') -Value $third -Encoding UTF8

    # Provenance.
    Push-Location $RepoRoot
    try {
        $commit = (git -c core.autocrlf=false rev-parse HEAD).Trim()
        $dirty = (git -c core.autocrlf=false status --porcelain --untracked-files=no)
        $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    } finally { Pop-Location }
    $version = (Get-Item $exe).VersionInfo.ProductVersion
    $info = @(
        'Ultima V Native for Windows -- build information',
        '',
        "Version:      $version",
        "Source:       $commit ($branch)$(if ($dirty) { ' + UNCOMMITTED CHANGES' } else { '' })",
        "Built:        $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))",
        "Toolchain:    MSVC $($env:VCToolsVersion), Windows SDK $($env:WindowsSDKVersion.TrimEnd('\')), CMake $((& $cmake --version | Select-Object -First 1) -replace 'cmake version ','')",
        "Config:       $Config, x64, static C/C++ runtime",
        'SDL:          3.4.16 (official source release, SHA-256 pinned in CMakeLists.txt)',
        '',
        'This package contains no Ultima V game data.',
        '',
        'SHA-256 of the files:'
    )
    foreach ($f in Get-ChildItem -Path $stage -File | Sort-Object Name) {
        $info += ('  {0}  {1}' -f (Get-FileHash -Algorithm SHA256 -Path $f.FullName).Hash.ToLower(), $f.Name)
    }
    Set-Content -Path (Join-Path $stage 'BUILD-INFO.txt') -Value $info -Encoding UTF8

    # Whitelist check: nothing but the expected files may be in the package.
    $allowed = @($files.Keys) + @('THIRD-PARTY-LICENSES.txt', 'BUILD-INFO.txt')
    $extra = Get-ChildItem -Path $stage -Recurse -File | Where-Object { $allowed -notcontains $_.Name }
    if ($extra) { throw "Unexpected files in the package: $($extra.Name -join ', ')" }
    foreach ($bad in @('*.bin', '*.GAM', '*.OOL', '*.16', '*.DAT', '*.OVL', '*.TLK')) {
        if (Get-ChildItem -Path $stage -Recurse -Filter $bad) { throw "Game data pattern $bad found in the package" }
    }

    $zip = Join-Path $DistDir "$PackageName.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
    $hash = (Get-FileHash -Algorithm SHA256 -Path $zip).Hash.ToLower()
    Step "Package: $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 2)) MB, SHA-256 $hash)"
}
Step "Done"
