param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$BuildDirectory='build-gpdf',
    [ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$TestSigningCertificate,
    [ValidateSet('spanish','english')][string]$InstallerLanguage='english'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$version=(Get-Content (Join-Path $root 'VERSION') -Raw).Trim()
$testRoot=Join-Path $root "$BuildDirectory/installer-tests"
$installPath=[IO.Path]::GetFullPath((Join-Path $testRoot 'Instalacion GPDF'))
if(!$installPath.StartsWith([IO.Path]::GetFullPath($testRoot)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Ruta de prueba fuera del proyecto.' }
$registryPath='HKCU:\Software\GPDFInstallerSmoke'
if(Test-Path -LiteralPath $registryPath) { throw 'Existe un registro de prueba previo. Revisarlo antes de repetir.' }
if(Test-Path -LiteralPath $installPath) { throw 'Existe una instalación de prueba previa. Revisarla antes de repetir.' }
$defaultPath='HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\UserChoice'
$before=Get-ItemProperty -LiteralPath $defaultPath -ErrorAction SilentlyContinue | Select-Object ProgId,Hash | ConvertTo-Json -Compress
if($TestSigningCertificate) {
    & (Join-Path $PSScriptRoot 'build-installer.ps1') -BuildDirectory $BuildDirectory -SkipBuild -SmokeTest -TestSigningCertificate $TestSigningCertificate
} else {
    & (Join-Path $PSScriptRoot 'build-installer.ps1') -BuildDirectory $BuildDirectory -SkipBuild -SmokeTest
}
$setup=Join-Path $testRoot 'GPDF-Setup-Test.exe'
for($pass=1;$pass -le 2;$pass++) {
    $languageArguments=if($InstallerLanguage -eq 'spanish') { @('/LANG=spanish') } else { @() }
    $process=Start-Process -FilePath $setup -ArgumentList (@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/TASKS=""',('/DIR="'+$installPath+'"'),('/LOG="'+(Join-Path $testRoot "install-$pass.log")+'"')) + $languageArguments) -WindowStyle Hidden -PassThru -Wait
    if($process.ExitCode -ne 0) { throw "Falló la instalación/actualización de prueba: $($process.ExitCode)" }
}
$package=Join-Path $root "dist/GPDF-$version"
if($TestSigningCertificate) { $package=Join-Path $root "dist/GPDF-$version-TestSigned" }
foreach($file in Get-ChildItem -LiteralPath $package -Recurse -File) {
    $relative=$file.FullName.Substring($package.Length+1)
    $installed=Join-Path $installPath $relative
    if(!(Test-Path -LiteralPath $installed) -or (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) { throw "Archivo instalado incorrecto: $relative" }
}
$command=(Get-Item -LiteralPath "$registryPath\Software\Classes\GPDF.PDF\shell\open\command").GetValue('')
if($command -ne ('"'+$installPath+'\GPDF.exe" "%1"')) { throw 'Comando de apertura incorrecto.' }
$icon=(Get-Item -LiteralPath "$registryPath\Software\Classes\GPDF.PDF\DefaultIcon").GetValue('')
if($icon -ne ('"'+$installPath+'\GPDF.exe",1')) { throw 'Índice de icono de documentos incorrecto.' }
$documentLabel=(Get-Item -LiteralPath "$registryPath\Software\Classes\GPDF.PDF").GetValue('')
$expectedLabel=if($InstallerLanguage -eq 'english') { 'GPDF PDF document' } else { 'Documento PDF de GPDF' }
if($documentLabel -ne $expectedLabel) { throw 'El instalador no aplicó el idioma seleccionado.' }
$env:QT_QPA_PLATFORM='windows'
$sample=Join-Path $root 'tests/data/bookmarks.pdf'
$capture=Join-Path $testRoot 'installed-program.png'
$app=Start-Process -FilePath (Join-Path $installPath 'GPDF.exe') -ArgumentList @(('"'+$sample+'"'),'--snapshot',('"'+$capture+'"')) -WindowStyle Hidden -PassThru -Wait
if($app.ExitCode -ne 0 -or !(Test-Path -LiteralPath $capture)) { throw 'La aplicación instalada no pudo abrir el PDF de prueba.' }
$marker=Join-Path $installPath 'user-file-preserved.txt'
Set-Content -LiteralPath $marker -Value 'Documento ajeno al instalador'
$uninstall=Join-Path $installPath 'unins000.exe'
if($TestSigningCertificate) {
    foreach($artifact in @($setup,(Join-Path $installPath 'GPDF.exe'),$uninstall)) {
        $signature=Get-AuthenticodeSignature -LiteralPath $artifact
        if(!$signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $TestSigningCertificate -or $signature.Status -notin @('Valid','NotTrusted','UnknownError')) { throw "Firma de prueba incorrecta: $artifact" }
    }
}
$process=Start-Process -FilePath $uninstall -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+(Join-Path $testRoot 'uninstall.log')+'"')) -WindowStyle Hidden -PassThru -Wait
if($process.ExitCode -ne 0 -or (Test-Path -LiteralPath (Join-Path $installPath 'GPDF.exe'))) { throw 'Falló la desinstalación.' }
if(!(Test-Path -LiteralPath $marker)) { throw 'La desinstalación borró un archivo ajeno.' }
if(Test-Path -LiteralPath "$registryPath\Software\Classes\GPDF.PDF") { throw 'La desinstalación dejó el ProgID de prueba.' }
$after=Get-ItemProperty -LiteralPath $defaultPath -ErrorAction SilentlyContinue | Select-Object ProgId,Hash | ConvertTo-Json -Compress
if($before -ne $after) { throw 'Cambió la selección predeterminada de PDF.' }
Remove-Item -LiteralPath $marker
if((Get-ChildItem -LiteralPath $installPath -Force).Count -eq 0) { Remove-Item -LiteralPath $installPath }
Remove-Item -LiteralPath $registryPath -Recurse
'PASS: instalación, actualización, hashes, comando e icono PDF, apertura, desinstalación y conservación de archivos ajenos y del predeterminado.'
