param (
    [switch]$debug = $false
)

$qtpath = $env:QTDIR

$rel = "Release"
$rels = "--release"

if ($debug)
{
    $rel = "Debug"
    $rels = "--debug"
}

if ($null -eq $qtpath)
{
    $qtpath = "C:\Qt\5.12.1\msvc2017_64"
}

$windeploy = "$($qtpath)\bin\windeployqt.exe"

$curpath = (Get-Item -Path ".\" -Verbose).FullName
$binpath = "$($curpath)\build\x64\$($rel)\Quasar.exe"

& $windeploy --no-quick-import $rels $binpath
