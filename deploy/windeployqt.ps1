$qtpath = $env:QTDIR

if ($qtpath -eq $null)
{
    $qtpath = "C:\Qt\5.9.3\msvc2017_64"
}

$windeploy = "$($qtpath)\bin\windeployqt.exe"

$curpath = (Get-Item -Path ".\" -Verbose).FullName
$binpath = "$($curpath)\build\x64\Release\Quasar.exe"

& $windeploy --no-quick-import --release $binpath
