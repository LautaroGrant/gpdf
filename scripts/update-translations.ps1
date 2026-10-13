$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$qt=Join-Path $root '.tools/Qt/6.8.3/mingw_64/bin'
$mingw=Join-Path $root '.tools/Qt/Tools/mingw1310_64/bin'
$tool=Join-Path $qt 'lrelease.exe'
if(!(Test-Path -LiteralPath $tool)) { throw 'Run scripts/bootstrap.ps1 to install Qt Linguist tools.' }
$env:PATH="$qt;$mingw;$env:PATH"
& $tool (Join-Path $root 'resources/translations/gpdf_en.ts') -qm (Join-Path $root 'resources/translations/gpdf_en.qm')
if($LASTEXITCODE) { throw 'Could not compile the English translation catalog.' }
