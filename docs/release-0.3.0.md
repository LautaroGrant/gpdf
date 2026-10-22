# GPDF 0.3.0 — Release preparation

Release date: October 22, 2026. Intended tag: `v0.3.0`.

## Release notes (Spanish)

GPDF 0.3.0 incorpora OCR automático para documentos escaneados. El reconocimiento
se ejecuta localmente con el motor de Windows y permite seleccionar, copiar,
buscar, resaltar y subrayar texto de páginas que antes eran solo imágenes.

Se revisa el documento completo, incluidas las páginas que todavía no se abrieron.
En documentos mixtos se conserva el texto digital existente. Las búsquedas esperan
a que termine la preparación y reutilizan el texto reconocido. No hay opciones de
OCR en el menú Ver; la barra inferior solo aparece durante el escaneo y se oculta
al finalizar. El texto reconocido se conserva al cambiar de pestaña y al deshacer
o rehacer anotaciones, incluso en documentos con más de 64 páginas reconocidas.

Requisitos OCR: Windows PowerShell 5.1 y un idioma OCR instalado en Windows.
Las posiciones de caracteres dentro de las palabras son aproximadas. Al guardar
no se incrusta una capa OCR en el PDF. Las políticas del equipo pueden bloquear
el motor; en ese caso GPDF indica que la preparación OCR está incompleta.

## Release notes (English)

GPDF 0.3.0 adds automatic OCR for scanned documents. Recognition runs locally
using Windows OCR and supports selection, copying, searching, highlighting and
underlining text in image-only pages.

The entire document is checked, including pages you have not visited. Existing
digital text is preserved in mixed documents. Search waits until preparation is
finished and reuses the recognized text. There are no OCR menu options; the bottom
information bar only appears during scanning and hides when scanning finishes.
Recognized text survives tab switches and annotation undo/redo, including
documents containing more than 64 recognized pages.

OCR requires Windows PowerShell 5.1 and an installed Windows OCR language.
Character positions within words are approximate. Saving does not embed a
searchable OCR layer in the PDF. Device policies may block recognition; GPDF
then reports incomplete OCR preparation.

## Assets to attach when publishing

- `dist/installers/GPDF-0.3.0-Setup.exe`
- `dist/GPDF-0.3.0-Portable.zip`
- `dist/SHA256SUMS-0.3.0.txt`

Use a stable release tagged `v0.3.0` for the current updater. Keep the installer
filename exactly `GPDF-0.3.0-Setup.exe`. Existing installations only receive an
update notification after this release is published on GitHub.

The installer keeps the existing application identity and installation path.
Build provenance and artifact hashes are recorded in `build-info.json`, the
installer JSON manifest, and the SHA-256 checksum file. The current artifacts
are built from an uncommitted working tree; their manifest records this fact.

No commit, tag, push, or GitHub release is part of this preparation.

## Validation and sizes

See [validation-0.3.0.md](validation-0.3.0.md) for test results, build provenance
and artifact hashes. Installer: 16.48 MB; portable archive: 20.67 MB;
uncompressed distribution: 46.54 MB (decimal units).
