param(
    [Parameter(Mandatory=$true)][string]$ArtifactPath,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$CertificateThumbprint,
    [switch]$AllowSelfSigned
)
$ErrorActionPreference='Stop'
$certificate=Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint"
if(!$certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)) { throw 'El certificado no tiene clave privada o está fuera de su vigencia.' }
if($certificate.Subject -ne 'CN=Lautaro Grant') { throw 'El certificado no corresponde a Lautaro Grant.' }
if(!($certificate.EnhancedKeyUsageList | Where-Object ObjectId -EQ '1.3.6.1.5.5.7.3.3')) { throw 'El certificado no permite firmar código.' }
$selfSigned=$certificate.Subject -eq $certificate.Issuer
if($selfSigned -and !$AllowSelfSigned) { throw 'La firma autofirmada requiere AllowSelfSigned y solo sirve para pruebas.' }
$signature=Set-AuthenticodeSignature -LiteralPath $ArtifactPath -Certificate $certificate -HashAlgorithm SHA256
$verified=Get-AuthenticodeSignature -LiteralPath $ArtifactPath
if(!$verified.SignerCertificate -or $verified.SignerCertificate.Thumbprint -ne $CertificateThumbprint -or $verified.Status -notin @('Valid','NotTrusted','UnknownError')) { throw "No se pudo verificar la firma: $($verified.Status) $($verified.StatusMessage)" }
if(!$AllowSelfSigned -and $verified.Status -ne 'Valid') { throw "La firma no es confiable: $($verified.Status)" }
Write-Output "Firmado: $ArtifactPath; autor: $($verified.SignerCertificate.Subject); estado: $($verified.Status)"
