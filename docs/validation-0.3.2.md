# Validación de GPDF 0.3.2

Fecha: 1 de noviembre de 2026.

## Compilación y procedencia

- Release nueva en `build-release-032`, Qt 6.8.3, MinGW 13.1, PDFium chromium/8086, Inno Setup 6.7.3.
- Versión coherente en VERSION, README de ambos idiomas y manifiestos: 0.3.2.
- Base Git: `764c953b99c0f436cc995300ba241d186539faf0`; cambios locales incluidos, `workingTreeDirty: true`.
- Sin commits, tags, push, publicación ni instalación sobre la copia del usuario.
- Instalador sin firma Authenticode; no se utiliza una firma de prueba para distribución.
- Iconos de Subrayado y Resaltar conservados por indicación del usuario. Iconos de aplicación y documentos sin cambios.

## Resultados

- CTest: **15/15 aprobadas**, 23,42 segundos.
- OCR nativo de Windows: aprobado, incluidos caché, cancelación, documento mixto y búsqueda de páginas escaneadas no visitadas.
- La suite de interfaz cubre políticas OCR persistentes, ejecución manual en modo deshabilitado, cancelación y ausencia de inicios automáticos; navegación de búsqueda, pestañas y regresión de ajuste de barras de desplazamiento.
- Instalador en inglés y español: instalación, reinstalación in situ, hashes de todos los archivos, comando e icono PDF, apertura, desinstalación y conservación de un archivo ajeno y de la asociación PDF predeterminada: aprobados.
- Instalación de prueba y registro temporal eliminados por el script de validación. Carpeta de instalación predeterminada estable: `%LOCALAPPDATA%\Programs\GPDF`.
- ZIP: **81 archivos** comprobados por SHA-256 contra la distribución; CRC y listado íntegro verificados.
- Hash del ejecutable distribuido idéntico al de la compilación Release.
- `git diff --check`: aprobado.

## Artefactos

| Archivo | Bytes |
| --- | ---: |
| `dist/installers/GPDF-0.3.2-Setup.exe` | 16486249 |
| `dist/GPDF-0.3.2-Portable.zip` | 20027042 |
| Distribución sin comprimir | 46577881 |

```text
574de991d464023121ddad06faf129b504449e8dde163b2a5a831dcf07dc43c4  GPDF-0.3.2-Setup.exe
ccb8f4a6802b16616d6fb142ccf66b1147aa2c2d13062bd6f1ea4873121ba234  GPDF-0.3.2-Portable.zip
```

Manifiestos: `dist/GPDF-0.3.2/build-info.json`, `dist/installers/GPDF-0.3.2-Setup.json` y `dist/GPDF-0.3.2-Portable.json`.

## Evidencia local

- `tmp/release-032-build.log`
- `build-release-032/Testing/Temporary/LastTest.log`
- `tmp/release-032-native-ocr.log`
- `tmp/release-032-installer.log`
- `tmp/release-032-installer-english.log`
- `tmp/release-032-installer-spanish.log`
- `build-release-032/installer-tests/`

Las pruebas con IPC y herramientas nativas se ejecutaron fuera del aislamiento del entorno. El portable conserva las preferencias y cachés de usuario habituales en Windows; no se ha agregado un modo de configuración autocontenido.
