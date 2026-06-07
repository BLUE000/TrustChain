$ErrorActionPreference = "Stop"
$toolsDir = Join-Path $PSScriptRoot "..\tools\BinMarkManager"
$zipUrl = "https://github.com/BLUE000/BinMarkManager/releases/download/Ver.1.0.0/BinMarkManager_Release_v1.0.0.zip"
$zipFile = Join-Path $PSScriptRoot "..\tools\BinMarkManager.zip"

if (Test-Path (Join-Path $toolsDir "BinMarkManagerGUI.exe")) {
    Write-Host "BinMarkManager is already set up. Skipping download."
    exit 0
}

Write-Host "Creating tools directory..."
if (-not (Test-Path (Join-Path $PSScriptRoot "..\tools"))) {
    New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot "..\tools") | Out-Null
}

Write-Host "Downloading BinMarkManager release..."
Invoke-WebRequest -Uri $zipUrl -OutFile $zipFile

Write-Host "Extracting to tools/BinMarkManager..."
Expand-Archive -Path $zipFile -DestinationPath $toolsDir -Force

Write-Host "Cleaning up zip file..."
Remove-Item -Path $zipFile -Force

Write-Host "BinMarkManager setup complete! You can run tools/BinMarkManager/BinMarkManagerGUI.exe"
