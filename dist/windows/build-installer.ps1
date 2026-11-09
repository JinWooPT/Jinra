# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory)][string]$PayloadDir,
    [Parameter(Mandatory)][string]$OutputDir,
    [string]$Compiler,
    [switch]$Pgo
)
$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path -LiteralPath $PayloadDir).Path
$sourceRoot = (Get-Item -LiteralPath $PSScriptRoot).Parent.Parent.FullName
$version = (Get-Content -LiteralPath (Join-Path $sourceRoot 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+(?:-preview\.\d+)?$') {
    throw 'The root VERSION file must contain a supported Jinra version.'
}
foreach ($required in @('jinra.exe', 'LICENSE.txt', 'LICENSES', 'GETTING_STARTED.txt')) {
    if (!(Test-Path -LiteralPath (Join-Path $packageRoot $required))) {
        throw "Missing package input: $required"
    }
}
if (!$Compiler) {
    $locations = @(
        (Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 6/ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6/ISCC.exe')
    )
    $Compiler = $locations | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (!$Compiler -or !(Test-Path -LiteralPath $Compiler)) {
    throw 'Install Inno Setup 6 or provide -Compiler with the path to ISCC.exe.'
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $OutputDir).Path
$suffix = if ($Pgo) { '-pgo' } else { '' }
$defines = @("/DPayloadDir=$packageRoot", "/DOutputDir=$outputRoot", "/DAppVersion=$version")
if ($Pgo) { $defines += "/DPackageSuffix=$suffix" }
& $Compiler '/Qp' @defines (Join-Path $PSScriptRoot 'jinra.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
$installer = Join-Path $outputRoot "Jinra-Setup-$version-x64$suffix.exe"
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath (Join-Path $outputRoot 'SHA256SUMS.txt') -Encoding ascii
Get-Item -LiteralPath $installer
