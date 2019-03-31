param (
    [string]$name = (git describe --tags --always),
    [switch]$portable = $false,
    [switch]$clean = $false
)

$ErrorActionPreference = "Stop"

$qtpath = $env:QTDIR
if ($null -eq $qtpath) {
    $qtpath = "C:\Qt\5.12.2\msvc2017_64"
}

$windeployqt = "$($qtpath)\bin\windeployqt.exe"

$package_paths = (".\deploy\packages\quasar.main\data\"),
(".\deploy\packages\quasar.api\data\"),
(".\deploy\packages\quasar.debug\data\"),
(".\deploy\packages\quasar.widgets\data\")

$data_path, $api_path, $debug_path, $widget_path = 0, 1, 2, 3

foreach ($path in $package_paths) {
    Write-Host "Cleaning path ($($path))..."
    Remove-Item $path -Force -Recurse -ErrorAction SilentlyContinue
}

if ($clean) {
    exit
}

$release_path = ".\build\x64\Release\"

$release_files = ("Quasar.exe"),
("extension-api.dll"),
("ssleay32.dll"),
("libeay32.dll")
("extensions\win_simple_perf.dll"),
("extensions\win_audio_viz.dll")

$api_files = (".\extension-api\extension_types.h"),
(".\extension-api\extension_support.h"),
(".\extension-api\extension_api.h"),
(".\build\x64\Release\extension-api.lib"),
(".\build\x64\Release\extension-api.exp")

$debug_files = ("Quasar.pdb"),
("extension-api.pdb"),
("extensions\win_simple_perf.pdb"),
("extensions\win_audio_viz.pdb")

Write-Host "Packing artifacts ($($name))..."

# Copy main app files
Write-Host "Copying main application files..."
New-Item $package_paths[$data_path] -Type Directory -Force > $null
New-Item ($package_paths[$data_path] + "extensions\") -Type Directory -Force > $null
foreach ($file in $release_files) {
    Write-Host "Copying $($release_path + $file) to $($package_paths[$data_path])"
    Copy-Item ($release_path + $file) -Destination ($package_paths[$data_path] + $file) -Force -ErrorAction SilentlyContinue
}

# Copy README
Copy-Item README.md -Destination $package_paths[$data_path] -Force

# Copy api files
Write-Host "Copying API files..."
New-Item $package_paths[$api_path] -Type Directory -Force > $null
New-Item ($package_paths[$api_path] + "api\") -Type Directory -Force > $null
foreach ($file in $api_files) {
    Write-Host "Copying $file to $($package_paths[$api_path] + "api\")"
    Copy-Item $file -Destination ($package_paths[$api_path] + "api\") -Force
}

#Copy debug files
Write-Host "Copying debug files..."
New-Item $package_paths[$debug_path] -Type Directory -Force > $null
New-Item ($package_paths[$debug_path] + "extensions\") -Type Directory -Force > $null
foreach ($file in $debug_files) {
    Write-Host "Copying $($release_path + $file) to $($package_paths[$debug_path])"
    Copy-Item ($release_path + $file) -Destination ($package_paths[$debug_path] + $file) -Force -ErrorAction SilentlyContinue
}

# Copy widgets
Write-Host "Copying widgets..."
Copy-Item .\widgets\ -Destination ($package_paths[$widget_path] + "widgets\") -recurse -Force

# Copy SSL lib
if (($env:OPENSSL) -and (Test-Path $env:OPENSSL -pathType container)) {
    Copy-Item $env:OPENSSL\libeay32.dll -Destination $package_paths[$data_path] -Force
    Copy-Item $env:OPENSSL\ssleay32.dll -Destination $package_paths[$data_path] -Force
}

# Deploy Qt
Write-Host "Deploying Qt..."
& $windeployqt --no-quick-import --release ($package_paths[$data_path] + "\Quasar.exe")

# Create installer
$pkgname = "quasar_$($name)_x64_installer.exe"
Write-Host "Creating installer $($pkgname)..."

& C:\Qt\Tools\QtInstallerFramework\3.0\bin\binarycreator.exe --offline-only -c deploy\config\config.xml -p deploy\packages $pkgname

if ($env:APPVEYOR -eq $true) {
    Get-ChildItem $pkgname | % { Push-AppveyorArtifact $_.FullName -FileName $_.Name }
}

# Create portable version too if specified
if ($portable) {
    $pkgname = "quasar_$($name)_x64_portable.7z"
    Write-Host "Packaging $($pkgname)..."

    New-Item .\Quasar_portable -Type Directory -Force > $null
    foreach ($path in $package_paths) {
        Copy-Item ($path + "*") -Destination .\Quasar_portable -Recurse -Force
    }

    & 7z a -t7z $pkgname .\Quasar_portable

    if ($env:APPVEYOR -eq $true) {
        Get-ChildItem $pkgname | % { Push-AppveyorArtifact $_.FullName -FileName $_.Name }
    }
}
