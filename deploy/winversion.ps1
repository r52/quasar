$gitdesc = (git describe --tags --always)

$versionfile = Get-Item .\src\version.h.in

(Get-Content $versionfile.PSPath) |
    Foreach-Object { $_ -replace "@QUASAR_GIT_VER@", $gitdesc } |
    Set-Content $versionfile.Basename
