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
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]*$')] [string]$PackageName = 'UltimaV-Native-Windows-x64-dev'
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
    $distRoot = [IO.Path]::GetFullPath($DistDir).TrimEnd('\') + '\'
    $stage = [IO.Path]::GetFullPath((Join-Path $DistDir $PackageName))
    if (-not $stage.StartsWith($distRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package staging must stay inside $distRoot"
    }
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    # The asset-preparation script: one bundled file, built from the project's
    # own TypeScript tooling (native/tools/asset-prep), run by the player's Node.js.
    # It is rebuilt for every package, so it can never be older than this
    # checkout; build.mjs refuses data that is not on its allowlist and writes
    # a manifest (hash, commit, bundled packages and data) read below.
    $bundle = Join-Path $RepoRoot 'native\tools\asset-prep\dist\prepare-assets.mjs'
    $manifestPath = Join-Path $RepoRoot 'native\tools\asset-prep\dist\prepare-assets.manifest.json'
    $builder = Join-Path $RepoRoot 'native\tools\asset-prep\build.mjs'
    if (-not (Get-Command node -ErrorAction SilentlyContinue)) { throw "Packaging needs Node.js 18+ on PATH to build the asset-preparation script." }
    if (-not (Test-Path (Join-Path $RepoRoot 'node_modules'))) { throw "Packaging needs node_modules in the checkout ('npm install') to build the asset-preparation script." }
    Push-Location $RepoRoot
    try { & node $builder; if ($LASTEXITCODE) { throw "asset-prep bundle build failed" } } finally { Pop-Location }
    $manifest = Get-Content -Raw -Path $manifestPath | ConvertFrom-Json
    $bundleHash = (Get-FileHash -Algorithm SHA256 -Path $bundle).Hash.ToLower()
    if ($bundleHash -ne $manifest.sha256) { throw "prepare-assets.mjs does not match its manifest ($bundleHash vs $($manifest.sha256))" }
    $head12 = (git -C $RepoRoot rev-parse --short=12 HEAD).Trim()
    if (($manifest.commit -replace '\+dirty$', '') -ne $head12) { throw "prepare-assets.mjs was built from $($manifest.commit), not HEAD $head12" }

    # SDL3: the licence and version of the SDL this build actually used.
    $cache = Get-Content -Path (Join-Path $BuildDir 'CMakeCache.txt')
    function CacheValue($name) {
        $line = $cache | Where-Object { $_ -match "^$name(:[A-Z]+)?=" } | Select-Object -First 1
        if ($line) { return ($line -split '=', 2)[1] } else { return '' }
    }
    if ((CacheValue 'UV_USE_SYSTEM_SDL3') -match '^(ON|TRUE|1|YES)$') {
        throw "This build uses an installed SDL3 (UV_USE_SYSTEM_SDL3=ON); package from a build of the pinned SDL release."
    }
    $sdlSource = CacheValue 'FETCHCONTENT_SOURCE_DIR_SDL3'
    $sdlOrigin = 'local source folder FETCHCONTENT_SOURCE_DIR_SDL3 (not hash-checked)'
    if (-not $sdlSource) {
        $sdlSource = Join-Path $BuildDir '_deps\sdl3-src'
        $sdlOrigin = 'official source release, SHA-256 pinned in CMakeLists.txt'
    }
    $sdlLicense = Join-Path $sdlSource 'LICENSE.txt'
    $sdlVersionHeader = Join-Path $sdlSource 'include\SDL3\SDL_version.h'
    if (-not (Test-Path $sdlLicense)) { throw "SDL3 LICENSE.txt not found in $sdlSource" }
    if (-not (Test-Path $sdlVersionHeader)) { throw "SDL3 SDL_version.h not found in $sdlSource" }
    $sdlHeader = Get-Content -Raw -Path $sdlVersionHeader
    $sdlVersion = (@('MAJOR', 'MINOR', 'MICRO') | ForEach-Object {
        if ($sdlHeader -match "#define\s+SDL_$($_)_VERSION\s+(\d+)") { $Matches[1] } else { throw "SDL_$($_)_VERSION not found in $sdlVersionHeader" }
    }) -join '.'

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
    # The bundle's npm packages and data come from its manifest, so this list
    # cannot drift from what build.mjs actually bundled.
    $redux = Join-Path $RepoRoot 'game\src\core\data\LICENSE-Ultima5Redux.txt'
    if (-not (Test-Path $redux)) { throw "Missing $redux" }
    $third = @(
        'Third-party software in this package',
        '=====================================',
        '',
        "SDL3 (Simple DirectMedia Layer) $sdlVersion -- SDL3.dll",
        'https://www.libsdl.org/  -- zlib licence, reproduced below.',
        '',
        'prepare-assets.mjs bundles, besides this project''s own GPL-3.0 tooling:'
    )
    foreach ($pkg in $manifest.packages) { $third += "  $pkg (npm package; licence reproduced below)" }
    $third += @(
        "  data from Ultima5Redux by Brad Hannah (MIT licence, reproduced below): $((@($manifest.dataInputs | ForEach-Object { Split-Path -Leaf $_ }) + @($manifest.embeddedData)) -join ', ')",
        '',
        '---------------------------------------------------------------- SDL3',
        ''
    )
    $third += Get-Content -Path $sdlLicense
    foreach ($pkg in $manifest.packages) {
        $lic = Get-ChildItem -Path (Join-Path $RepoRoot "node_modules\$pkg") -Filter 'LICENSE*' -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $lic) { throw "No licence file for the bundled package $pkg in node_modules\$pkg" }
        $third += @('', "---------------------------------------------------------------- $pkg", '')
        $third += Get-Content -Path $lic.FullName
    }
    $third += @('', '---------------------------------------------------------------- Ultima5Redux data', '')
    $third += Get-Content -Path $redux
    Set-Content -Path (Join-Path $stage 'THIRD-PARTY-LICENSES.txt') -Value $third -Encoding UTF8

    # Provenance.
    Push-Location $RepoRoot
    try {
        $commit = (git -c core.autocrlf=false rev-parse HEAD).Trim()
        # The checkout's own line-ending rules: with core.autocrlf=false a CRLF
        # checkout whose timestamp changed (a merge, a rebuilt or touched file)
        # reads as modified although it equals HEAD.
        $dirty = (git status --porcelain --untracked-files=no)
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
        "SDL:          $sdlVersion ($sdlOrigin)",
        "Asset prep:   prepare-assets.mjs from $($manifest.commit), $($manifest.bytes) bytes, SHA-256 $($manifest.sha256)",
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
