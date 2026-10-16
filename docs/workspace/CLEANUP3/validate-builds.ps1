param([ValidateSet('host','windows','tooling','firmware','primary')][string]$Suite)
$ErrorActionPreference='Stop'
$env:GIT_OPTIONAL_LOCKS='0'
$env:PYTHONDONTWRITEBYTECODE='1'
$projectRoot='C:\Dev\U5Native'
$rcRoot=Join-Path $projectRoot 'worktrees\ultima-v-native-rc1'
$layout=Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'workspace.json') | ConvertFrom-Json
$primaryRoot=if(Test-Path -LiteralPath (Join-Path $projectRoot 'source')){Join-Path $projectRoot 'source'}else{$layout.primary}
$logRoot=Join-Path $projectRoot 'archive\CLEANUP3\tests'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
function Invoke-Logged([string]$Name,[string]$Exe,[string[]]$Arguments) {
    $logPath=Join-Path $logRoot ($Name+'.log')
    if(Test-Path -LiteralPath $logPath){ throw "Refuse overwrite: $logPath" }
    Write-Host "Running $Name"
    & $Exe @Arguments 2>&1 | Tee-Object -FilePath $logPath | Out-Null
    $exitCode=$LASTEXITCODE
    @{name=$Name;exe=$Exe;arguments=$Arguments;exit_code=$exitCode;log=$logPath} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $logRoot ($Name+'.json'))
    Get-Content -LiteralPath $logPath -Tail 6
    if($exitCode){throw "$Name failed: $exitCode"}
}
if($Suite -eq 'host') {
    $toolBin='C:\Dev\TamaPoke\.build-tools\w64devkit\bin'
    $env:PATH=$toolBin+';'+$env:PATH
    $cmake=Join-Path $toolBin 'cmake.exe'
    $ctest=Join-Path $toolBin 'ctest.exe'
    $output=Join-Path $projectRoot 'builds\cleanup3-validation\rc1-host'
    Invoke-Logged 'host-configure' $cmake @('-S',"$rcRoot\native\core",'-B',$output,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DOPENU5_ENABLE_DEVELOPER_TOOLS=ON','-DOPENU5_BUILD_RC1_DIFFERENTIAL=ON')
    Invoke-Logged 'host-build' $cmake @('--build',$output,'--parallel','6')
    Invoke-Logged 'host-tests' $ctest @('--test-dir',$output,'--output-on-failure','--parallel','6')
}
if($Suite -eq 'windows') {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(-not $vs){throw 'Visual Studio not available'}
    $vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object { if($_ -match '^([^=]+)=(.*)$'){Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]} }
    $cmake=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
    $env:PATH=(Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja')+';'+$env:PATH
    $output=Join-Path $projectRoot 'builds\cleanup3-validation\rc1-windows'
    $sdlSource=Join-Path $rcRoot 'native\targets\windows\build-rc1\_deps\SDL3-3.4.16'
    Invoke-Logged 'windows-configure' $cmake @('-S',"$rcRoot\native\targets\windows",'-B',$output,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',"-DFETCHCONTENT_SOURCE_DIR_SDL3=$sdlSource",'-DUV_VERSION_SUFFIX=rc1',"-DUV_SCENARIO_PACKS=$rcRoot\native\assets","-DUV_SCENARIO_PC_SAVES=$rcRoot\original\u5\ultima5")
    Invoke-Logged 'windows-build' $cmake @('--build',$output,'--parallel','6')
    Invoke-Logged 'windows-tests' $ctest @('--test-dir',$output,'--output-on-failure','--parallel','2')
}
if($Suite -eq 'tooling') {
    Invoke-Logged 'rc1-tooling' 'python' @('-B','-m','unittest','discover','-s',"$rcRoot\native\tools\rc1",'-p','test_*.py','-v')
    Invoke-Logged 'cleanup-safety' 'python' @('-B',"$primaryRoot\native\tools\test_cleanup_builds.py")
}


if($Suite -eq 'firmware') {
    . C:\esp\v6.1\esp-idf\export.ps1 *> (Join-Path $logRoot 'firmware-export.log')
    $output=Join-Path $projectRoot 'builds\cleanup3-validation\rc1-tdeck'
    if(Test-Path -LiteralPath $output){throw 'Refuse existing firmware output'}
    New-Item -ItemType Directory -Path $output | Out-Null
    Copy-Item -LiteralPath "$rcRoot\native\targets\tdeck\sdkconfig" -Destination (Join-Path $output 'sdkconfig')
    Push-Location "$rcRoot\native\targets\tdeck"
    try {
        $idf=Join-Path $env:IDF_PATH 'tools\idf.py'
        Invoke-Logged 'firmware-configure' 'python' @($idf,'--no-ccache','-B',$output,'-D',"SDKCONFIG=$output\sdkconfig",'reconfigure')
        Invoke-Logged 'firmware-build' 'ninja' @('-C',$output,'-j','6','all')
    } finally {Pop-Location}
}

if($Suite -eq 'primary') {
    $entry=Join-Path $projectRoot 'tools\Build-U5Native.ps1'
    $output=Join-Path $projectRoot 'builds\cleanup3-validation\primary-host-dev'
    Invoke-Logged 'primary-host-dev' 'pwsh' @('-NoProfile','-File',$entry,'-Checkout','source','-Target','Host','-BuildDir',$output,'-DeveloperTools','-Test')
}
