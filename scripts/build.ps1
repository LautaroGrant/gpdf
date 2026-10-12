param([switch]$Package,[ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_.-]*$')][string]$PackageName='StudyPDF',[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$BuildDirectory='build')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root $BuildDirectory
$qt = Join-Path $root '.tools/Qt/6.8.3/mingw_64'
$mingw = Join-Path $root '.tools/Qt/Tools/mingw1310_64/bin'
$cmake = Join-Path $root '.tools/python/cmake/data/bin/cmake.exe'
$ninja = Join-Path $root '.tools/python/bin/ninja.exe'
foreach ($path in @($cmake,$ninja,"$mingw/g++.exe","$qt/bin/windeployqt.exe","$root/third_party/pdfium/bin/pdfium.dll")) {
    if (!(Test-Path -LiteralPath $path)) { throw "Falta $path. Ejecutá scripts/bootstrap.ps1 primero." }
}
$env:PATH = "$mingw;$qt/bin;$(Split-Path $ninja);$env:PATH"
& $cmake -S $root -B "$build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_PREFIX_PATH=$qt" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE) { throw 'Falló la configuración.' }
& $cmake --build "$build"
if ($LASTEXITCODE) { throw 'Falló la compilación.' }
& "$qt/bin/windeployqt.exe" --release --no-translations --no-opengl-sw "$build/StudyPDF.exe"
if ($LASTEXITCODE) { throw 'Falló el despliegue de Qt.' }
Copy-Item "$qt/plugins/platforms/qoffscreen.dll" "$build/platforms/" -Force
& "$root/.tools/python/cmake/data/bin/ctest.exe" --test-dir "$build" --output-on-failure
if ($LASTEXITCODE) { throw 'Fallaron las pruebas del proyecto.' }
if ($Package) {
    $dist = "$root/dist/$PackageName"
    New-Item -ItemType Directory -Force $dist | Out-Null
    Copy-Item "$build/StudyPDF.exe","$build/pdfium.dll" $dist
    & "$qt/bin/windeployqt.exe" --release --no-translations --no-opengl-sw "$dist/StudyPDF.exe"
    if ($LASTEXITCODE) { throw 'Falló el empaquetado.' }
    Copy-Item "$root/README.md","$root/CHANGELOG.md","$root/VERSION" $dist
    $revision=& git -C $root rev-parse HEAD 2>$null
    $dirty=if($LASTEXITCODE -eq 0) { [bool](& git -C $root status --porcelain) } else { $null }
    [ordered]@{
        version=(Get-Content "$root/VERSION" -Raw).Trim()
        gitCommit=$revision
        workingTreeDirty=$dirty
        builtAtUtc=[DateTime]::UtcNow.ToString('o')
        executableSha256=(Get-FileHash "$dist/StudyPDF.exe" -Algorithm SHA256).Hash
        pdfiumVersion=(Get-Content "$root/third_party/pdfium-version.txt" -Raw).Trim()
        pdfiumSha256=(Get-FileHash "$dist/pdfium.dll" -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content -Encoding utf8 "$dist/build-info.json"
    New-Item -ItemType Directory -Force "$dist/licenses/PDFium" | Out-Null
    Copy-Item "$root/third_party/pdfium/LICENSE*" "$dist/licenses/PDFium" -Recurse -Force
    Copy-Item "$root/third_party/pdfium-version.txt" "$dist/licenses/PDFium"
    New-Item -ItemType Directory -Force "$dist/licenses/Qt" | Out-Null
    Copy-Item "$root/third_party/licenses/Qt/*" "$dist/licenses/Qt" -Recurse -Force
}
