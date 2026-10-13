param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$BuildDirectory='build-gpdf',
    [switch]$SkipBuild,
    [switch]$SmokeTest,
    [ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$TestSigningCertificate
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$version=(Get-Content (Join-Path $root 'VERSION') -Raw).Trim()
$package=Join-Path $root "dist/GPDF-$version"
$compilerDirectory=Join-Path $root '.tools/InnoSetup/compiler'
$compiler=Join-Path $compilerDirectory 'ISCC.exe'
if(!(Test-Path -LiteralPath $compiler)) {
    $download=Join-Path $root '.tools/InnoSetup/innosetup-6.7.3.exe'
    New-Item -ItemType Directory -Force (Split-Path $download) | Out-Null
    if(!(Test-Path -LiteralPath $download)) {
        Invoke-WebRequest -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe' -OutFile $download
    }
    if((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne '9C73C3BAE7ED48D44112A0F48E66742C00090BDB5BEF71D9D3C056C66E97B732') { throw 'El hash del compilador no coincide.' }
    $signature=Get-AuthenticodeSignature -LiteralPath $download
    if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Pyrsys B.V.') { throw 'La firma del compilador no es válida.' }
    $process=Start-Process -FilePath $download -ArgumentList @('/PORTABLE=1','/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="'+$compilerDirectory+'"')) -WindowStyle Hidden -PassThru -Wait
    if($process.ExitCode -ne 0) { throw 'Falló la extracción portable de Inno Setup.' }
}
if(!$SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -BuildDirectory $BuildDirectory -Package -PackageName "GPDF-$version"
    if($LASTEXITCODE) { throw 'Falló la preparación del paquete.' }
}
$manifest=Get-Content (Join-Path $package 'build-info.json') -Raw | ConvertFrom-Json
if($manifest.version -ne $version -or (Get-FileHash (Join-Path $package 'GPDF.exe') -Algorithm SHA256).Hash -ne $manifest.executableSha256) { throw 'El paquete no corresponde a VERSION o su ejecutable cambió.' }
if($TestSigningCertificate) {
    $signedPackage=Join-Path $root "dist/GPDF-$version-TestSigned"
    New-Item -ItemType Directory -Force $signedPackage | Out-Null
    Copy-Item -Path (Join-Path $package '*') -Destination $signedPackage -Recurse -Force
    $package=$signedPackage
    & (Join-Path $PSScriptRoot 'sign-artifact.ps1') -ArtifactPath (Join-Path $package 'GPDF.exe') -CertificateThumbprint $TestSigningCertificate -AllowSelfSigned
    $manifest.executableSha256=(Get-FileHash (Join-Path $package 'GPDF.exe') -Algorithm SHA256).Hash
    $manifest | Add-Member -NotePropertyName testSigningCertificate -NotePropertyValue $TestSigningCertificate -Force
    $manifest | Add-Member -NotePropertyName signingTrust -NotePropertyValue 'self-signed-test-only' -Force
    $manifest | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $package 'build-info.json')
}
$output=if($SmokeTest) { Join-Path $root "$BuildDirectory/installer-tests" } else { Join-Path $root 'dist/installers' }
New-Item -ItemType Directory -Force $output | Out-Null
$arguments=@("/DPackageDir=$package","/DAppVersion=$version","/DOutputDir=$output")
if($SmokeTest) { $arguments+='/DSmokeTest=1' }
if($TestSigningCertificate) {
    $arguments+='/DTestSigning=1'
    $signScript=Join-Path $PSScriptRoot 'sign-artifact.ps1'
    $signShell=(Get-Process -Id $PID).Path
    $arguments+=('/Sgpdf=$q'+$signShell+'$q -NoProfile -ExecutionPolicy Bypass -File $q'+$signScript+'$q -ArtifactPath $f -CertificateThumbprint '+$TestSigningCertificate+' -AllowSelfSigned')
}
& $compiler @arguments (Join-Path $root 'installer/GPDF.iss')
if($LASTEXITCODE) { throw 'Falló la compilación del instalador.' }
if(!$SmokeTest) {
    $baseName=if($TestSigningCertificate) { "GPDF-$version-Setup-TestSigned" } else { "GPDF-$version-Setup" }
    $installer=Join-Path $output "$baseName.exe"
    [ordered]@{version=$version;gitCommit=$manifest.gitCommit;installerSourcesDirty=[bool](& git -C $root status --porcelain);installerSha256=(Get-FileHash $installer -Algorithm SHA256).Hash;installerBytes=(Get-Item $installer).Length;compilerVersion='Inno Setup 6.7.3';testSigningCertificate=$TestSigningCertificate} | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $output "$baseName.json")
    Write-Output "Instalador: $installer"
}
