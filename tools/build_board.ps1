<#
.SYNOPSIS
    Pico 构建入口 / Build Pico firmware with ESP-IDF v6.1.
#>
param(
    [ValidateSet("pico")][string]$Board = "pico",
    [string]$Output = "build/boards",
    [string]$Python = "python"
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $env:IDF_PATH) { throw "先载入 ESP-IDF / Load ESP-IDF first." }
Push-Location $repo
try {
    $pythonPath = (Get-Command $Python -ErrorAction Stop).Source
    & $pythonPath tools/build_firmware.py --board pico --output $Output --python $pythonPath
    if ($LASTEXITCODE -ne 0) { throw "Pico build failed ($LASTEXITCODE)" }
} finally { Pop-Location }
