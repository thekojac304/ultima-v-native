<# Fresh builds only. Historical caches are preserved evidence and are not reusable. #>
[CmdletBinding()]
param(
    [string]$Checkout='source',
    [ValidateSet('Host','Windows')][string]$Target='Host',
    [string]$BuildDir='',
    [string]$HostToolBin=$env:U5_W64_BIN,
    [switch]$DeveloperTools=$true,
    [switch]$Test
)
$ErrorActionPreference='Stop'
$env:GIT_OPTIONAL_LOCKS='0'
$projectRoot=Split-Path -Parent $PSScriptRoot
$layout=Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'workspace.json') | ConvertFrom-Json
$checkoutPath=if($Checkout -eq 'source'){if(Test-Path -LiteralPath (Join-Path $projectRoot 'source')){Join-Path $projectRoot 'source'}else{$layout.primary}}else{Join-Path (Join-Path $projectRoot 'worktrees') $Checkout}
$checkoutPath=[IO.Path]::GetFullPath($checkoutPath)
if(-not $checkoutPath.StartsWith($projectRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and $checkoutPath -ne $layout.primary){throw 'Checkout must be the recorded primary or inside U5Native'}
if(-not(Test-Path -LiteralPath (Join-Path $checkoutPath '.git'))){throw "Unknown checkout: $checkoutPath"}
if(-not $BuildDir){$BuildDir=Join-Path $projectRoot ("builds\$Checkout\"+(Get-Date -Format 'yyyyMMdd-HHmmss-ffff')+"\$Target")}
$BuildDir=[IO.Path]::GetFullPath($BuildDir)
if(Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt')){throw 'Choose a fresh output directory; historical CMake caches must not be reused'}
if($Target -eq 'Windows'){
    $builder=Join-Path $checkoutPath 'native\targets\windows\tools\build-windows.ps1'
    if(-not(Test-Path -LiteralPath $builder)){throw 'This checkout has no Windows target'}
    & $builder -BuildDir $BuildDir -Test:$Test
    if($LASTEXITCODE){throw 'Windows build failed'}
}else{
    if(-not $HostToolBin){$HostToolBin='C:\Dev\TamaPoke\.build-tools\w64devkit\bin'}
    $env:PATH=$HostToolBin+';'+$env:PATH
    $cmake=Join-Path $HostToolBin 'cmake.exe'
    $ctest=Join-Path $HostToolBin 'ctest.exe'
    $devFlag=if($DeveloperTools){'ON'}else{'OFF'}
    & $cmake -S (Join-Path $checkoutPath 'native\core') -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DOPENU5_ENABLE_DEVELOPER_TOOLS=$devFlag"
    if($LASTEXITCODE){throw 'Host configure failed'}
    & $cmake --build $BuildDir --parallel 6
    if($LASTEXITCODE){throw 'Host build failed'}
    if($Test){ & $ctest --test-dir $BuildDir --output-on-failure --parallel 6; if($LASTEXITCODE){throw 'Host tests failed'} }
}
Write-Host "Build: $BuildDir"
