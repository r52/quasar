param (
    [switch]$portable = $false,
    [switch]$installer = $false,
    [string]$name = "quasar",
    [switch]$delete = $false
 )

Write-Host "Packing artifacts ($($name))..."

Copy-Item .\build\x64\Release\ .\Quasar\ -recurse

Remove-Item .\Quasar\*.ilk
Remove-Item .\Quasar\*.exp
Remove-Item .\Quasar\*.iobj
Remove-Item .\Quasar\*.ipdb
Remove-Item .\Quasar\plugin-api.lib

Remove-Item .\Quasar\plugins\*.exp
Remove-Item .\Quasar\plugins\*.iobj
Remove-Item .\Quasar\plugins\*.lib
Remove-Item .\Quasar\plugins\*.ipdb

Copy-Item .\widgets\ .\Quasar\ -recurse
Copy-Item README.md .\Quasar\
Copy-Item LICENSE.txt .\Quasar\

if ($portable)
{
    $pkgname = "$($name)_win_x64.7z"

    Write-Host "Packaging $($pkgname)..."

    & 7z a -t7z $pkgname .\Quasar\

    if ($env:APPVEYOR -eq $true)
    {
        Get-ChildItem $pkgname | % { Push-AppveyorArtifact $_.FullName -FileName $_.Name }
    }
}

if ($installer)
{
    $pkgname = "$($name)_win_x64.msi"

    Write-Host "Packaging $($pkgname)..."

    if ($env:WIX)
    {
        & $env:WIX\candle.exe -arch x64 .\deploy\quasar.wxs
        & $env:WIX\light.exe -out $pkgname quasar.wixobj

        if ($env:APPVEYOR -eq $true)
        {
            Get-ChildItem $pkgname | % { Push-AppveyorArtifact $_.FullName -FileName $_.Name }
        }
    }
}

if ($delete)
{
    Remove-Item .\Quasar -Force -Recurse
}
