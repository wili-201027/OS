$ErrorActionPreference = 'Stop'

$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }
$makeExe = Join-Path $msysRoot 'usr\bin\make.exe'
if (-not (Test-Path $makeExe)) {
    throw "No se encontró make.exe en $makeExe. Abre MSYS2 MinGW y asegúrate de instalar make."
}

$mingwBin = Join-Path $msysRoot 'mingw64\bin'
if (-not (Test-Path $mingwBin)) {
    throw "No se encontró la carpeta de MinGW64 en $mingwBin."
}

$env:Path = "$($msysRoot)\usr\bin;$mingwBin;$env:Path"
$env:MSYS2_MINGW64_ROOT = Join-Path $msysRoot 'mingw64'

Set-Location $PSScriptRoot
& $makeExe -C . -f build/kernel.mk
exit $LASTEXITCODE
