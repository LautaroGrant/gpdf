# Downloads only into this project. Requires Python 3.11+ and Internet access.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force .tools,third_party | Out-Null
    python -m pip install --target .tools/python aqtinstall==3.3.0 cmake==4.4.4 ninja==1.13.2
    if ($LASTEXITCODE) { throw 'Falló la descarga de herramientas.' }
    $env:PYTHONPATH = "$root/.tools/python"
    python -m aqt install-qt windows desktop 6.8.3 win64_mingw -O .tools/Qt --archives qtbase
    if ($LASTEXITCODE) { throw 'Falló la descarga de Qt.' }
    python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O .tools/Qt
    if ($LASTEXITCODE) { throw 'Falló la descarga de MinGW.' }
    $url = 'https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F8086/pdfium-win-x64.tgz'
    Invoke-WebRequest $url -OutFile third_party/pdfium.tgz
    $expected = '1FD8AF952832DBB0EB16D9249F68FE09E5F5EBF7C3DD9F6066EA2720CC28487D'
    if ((Get-FileHash third_party/pdfium.tgz -Algorithm SHA256).Hash -ne $expected) { throw 'El hash de PDFium no coincide.' }
    New-Item -ItemType Directory -Force third_party/pdfium | Out-Null
    tar -xzf third_party/pdfium.tgz -C third_party/pdfium
    if ($LASTEXITCODE) { throw 'Falló la extracción de PDFium.' }
    'chromium/8086' | Set-Content third_party/pdfium-version.txt
    New-Item -ItemType Directory -Force third_party/licenses/Qt | Out-Null
    $files = Invoke-RestMethod 'https://api.github.com/repos/qt/qtbase/contents/LICENSES?ref=v6.8.3'
    foreach ($item in $files) {
        if ($item.type -eq 'file') { Invoke-WebRequest $item.download_url -OutFile (Join-Path 'third_party/licenses/Qt' $item.name) }
    }
} finally { Pop-Location }
