# Firma autofirmada de prueba

Por elección explícita del usuario, se creó un certificado de firma de código CN=Lautaro Grant en Cert:\CurrentUser\My. La clave privada no es exportable y no se añadió el certificado a raíces ni editores confiables. Vigencia de un año desde el 13 de octubre de 2026. Sin sello de tiempo.

Thumbprint público: 00BFD1BC60B4EF8C0A2AC7EFB705D82BB06605E7.

Paquete firmado separado: dist/GPDF-0.2.6-TestSigned. Instalador: dist/installers/GPDF-0.2.6-Setup-TestSigned.exe. Certificado público: dist/installers/Lautaro-Grant-Test-Signing.cer. El ejecutable, instalador y desinstalador están firmados; las DLL de terceros conservan sus firmas originales si las tienen. Los paquetes sin firma se conservan.

Get-AuthenticodeSignature muestra CN=Lautaro Grant. En este equipo el estado es UnknownError con mensaje de raíz no confiable: es el resultado esperado sin establecer confianza para el certificado autofirmado. No convierte la distribución en confiable para otros usuarios ni elimina SmartScreen. Una modificación en una copia del ejecutable se detectó como HashMismatch.

Reproducción, desde la cuenta que conserva la clave privada:

```powershell
./scripts/build-installer.ps1 -SkipBuild -TestSigningCertificate 00BFD1BC60B4EF8C0A2AC7EFB705D82BB06605E7
./scripts/validate-installer.ps1 -TestSigningCertificate 00BFD1BC60B4EF8C0A2AC7EFB705D82BB06605E7
```

La validación comprueba las tres firmas y repite instalación/actualización/apertura/desinstalación aisladas. Registro: build-gpdf/validation-testsigned-installer.log. La clave privada no está en el repositorio; .pfx y .p12 se excluyen también mediante .gitignore. No se hizo commit, tag ni push durante este cambio.
