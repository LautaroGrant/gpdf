# Validación de GPDF 0.3.3

Fecha: 2 de noviembre de 2026.

## Compilación y procedencia

- Compilación Release independiente en `build-release-033`, Qt 6.8.3, MinGW 13.1 y PDFium chromium/8086.
- Versión actualizada en VERSION y README de ambos idiomas; CMake y el instalador leen VERSION.
- Base Git: `6ff526795b76f5d27005dd4c5f58a14c70ad2042`; cambios locales sin commit incluidos en los manifiestos.
- Preparación local, sin commit, tag, push ni publicación.

## Resultados

- Indicador de página: inicio sin PDF, apertura, navegación, cierre del último documento y reapertura verificados; contenido vacío y deshabilitado sin PDF, numeración restablecida con PDF y espacio conservado.
- CTest: **16/16 aprobadas**, 40.96 segundos, en la compilación Release 0.3.3.
- OCR nativo de Windows: aprobado con `ocr_smoke --native` en la preparación anterior al ajuste visual del indicador.
- PDFs reales (preparación anterior al ajuste visual del indicador): 495 páginas (80 MB) y 2 páginas; apertura, dimensiones y texto de todas las páginas, renderizado de primera/central/última, anotación, deshacer/rehacer, recuperación, guardado y reapertura aprobados. Originales intactos verificados con SHA-256. No equivale a revisión visual de todas las páginas ni a un benchmark aislado.
- ZIP: **81 archivos** comprobados individualmente por SHA-256 contra la distribución; CRC y listado verificados.
- Ejecutable distribuido idéntico al de `build-release-033`.
- Instalador de distribución compilado con Inno Setup 6.7.3, sin firma Authenticode.
- Instalador en inglés: instalación, actualización, hashes de todos los archivos, apertura y desinstalación aprobados; archivo ajeno y asociación PDF predeterminada conservados.
- Instalador en español: instalación, actualización, hashes, apertura y desinstalación aprobados; archivo ajeno y asociación PDF predeterminada conservados.
- Carpetas de instalación y registro de prueba eliminados por el validador.
- `git diff --check`: aprobado. HEAD conservado, sin commits.

## Artefactos

| Archivo | Bytes |
| --- | ---: |
| `dist/installers/GPDF-0.3.3-Setup.exe` | 16487127 |
| `dist/GPDF-0.3.3-Portable.zip` | 20030439 |
| Distribución descomprimida | 46591946 |

```text
98e0b4d6becff7746bfb8f4b94ed09ae9eef1e3396aaafd97a9ee74fd1a49196  GPDF-0.3.3-Setup.exe
802dbebfbdddf574d9b48171587ffd93a270e3d2a5105a5e1a1347afa7cb88df  GPDF-0.3.3-Portable.zip
```

Manifiestos: `dist/GPDF-0.3.3/build-info.json`, `dist/installers/GPDF-0.3.3-Setup.json` y `dist/GPDF-0.3.3-Portable.json`. Hashes en `dist/SHA256SUMS-0.3.3.txt`.

## Evidencia local

- `tmp/release-033-home-build.log`
- `build-release-033/Testing/Temporary/LastTest.log`
- `tmp/release-033-native-ocr.log`
- `tmp/release-033-corpus.log`
- `tmp/release-033-home-installer.log`
- `tmp/release-033-home-installer-english.log`
- `tmp/release-033-home-installer-spanish.log`
- `build-release-033/installer-tests-home-english/`
- `build-release-033/installer-tests/`

Las pruebas de instalación usan una carpeta y claves de registro de prueba, sin reemplazar la instalación del usuario. El portable conserva las preferencias y cachés de usuario de Windows; no es un modo de configuración autocontenido.


Los paquetes anteriores al ajuste del indicador se conservan en `dist/archive/0.3.3-before-empty-page-indicator/`.
