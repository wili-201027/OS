$env:Path = 'C:\msys64\usr\bin;' + $env:Path
Set-Location E:\os
& C:\msys64\usr\bin\make.exe -f build/kernel.mk clean
& C:\msys64\usr\bin\make.exe -f build/kernel.mk all > _build.txt 2>&1
Write-Output "EXITCODE=$LASTEXITCODE"
Get-Content _build.txt -TotalCount 40
Write-Output "----TAIL----"
Get-Content _build.txt -Tail 40
