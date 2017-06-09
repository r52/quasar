$name = $args[0]

if ($name -eq $null)
{
    $name = "quasar"
}

$pkgname = "$($name)_win_x64.7z"

Write-Host "Packaging $($pkgname)..."

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

& 7z a -t7z $pkgname .\Quasar\

if ($env:APPVEYOR -eq $true)
{
    Get-ChildItem $pkgname | % { Push-AppveyorArtifact $_.FullName -FileName $_.Name }
}
