# This would be performed during cmake on other platforms
Write-Host "Generating version.h..."
$gitdesc = (git describe --tags --always)

$versionfile = Get-Item .\src\version.h.in
$outpath = ".\GeneratedFiles"

if (!(Test-Path $outpath))
{
    New-Item -ItemType Directory -Force -Path $outpath | Out-Null
}

(Get-Content $versionfile.PSPath) |
    Foreach-Object { $_ -replace "@QUASAR_GIT_VER@", $gitdesc } |
    Set-Content "$($outpath)\$($versionfile.Basename)" -Force
