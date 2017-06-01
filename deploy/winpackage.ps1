$name = $args[0]

if ($name -eq $null)
{
    $name = "quasar"
}

$pkgname = "$($name)_win_x64.7z"

Write-Host "Packaging $($pkgname)..."

Copy-Item .\build\x64\Release\ .\quasar\ -recurse

Remove-Item .\quasar\*.ilk
Remove-Item .\quasar\*.exp
Remove-Item .\quasar\*.iobj
Remove-Item .\quasar\*.ipdb
Remove-Item .\quasar\plugin-api.lib

Remove-Item .\quasar\plugins\*.exp
Remove-Item .\quasar\plugins\*.iobj
Remove-Item .\quasar\plugins\*.lib
Remove-Item .\quasar\plugins\*.ipdb

Copy-Item .\widgets\ .\quasar\ -recurse
Copy-Item README.md .\quasar\
Copy-Item LICENSE.txt .\quasar\

& 7z a -t7z $pkgname .\quasar\

Remove-Item .\quasar -Force -Recurse

if ($env:APPVEYOR -eq $true)
{
    Get-ChildItem $pkgname | % { Push-AppveyorArtifact $_.FullName -FileName $_.Name }
}
