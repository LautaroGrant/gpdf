# Preparación de GPDF 0.2.8 — 17 de octubre de 2026

Versión centralizada en `VERSION`: 0.2.8. Changelog consolidado y README en español e inglés actualizados para incluir el renderizado a resolución física. Los cambios se preparan para commit; no se crea commit, tag ni push.

## Compilación y pruebas

Compilación Release con Qt 6.8.3, MinGW 13.1 y PDFium chromium/8086 mediante `./scripts/build-installer.ps1 -BuildDirectory build-release-028`.

Se utiliza una carpeta nueva porque `build-gpdf/CMakeCache.txt` conserva rutas de la antigua unidad E:. El despliegue de Qt requiere ejecutar el script fuera del entorno restringido de la herramienta: dentro de ese entorno, el proceso auxiliar `qtpaths` finaliza con 0xC0000142.

CTest: 9/9 pruebas aprobadas, en 13,47 segundos. Núcleo y persistencia, interfaz de anotaciones, corpus, español, inglés y comparación de raster a escalas 1, 1.25, 1.5 y 2. La comparación cubre dos páginas y giros de 0°, 90°, 180° y 270°, con un documento de referencia independiente. Registro: `build-release-028/Testing/Temporary/LastTest.log`.

## Distribución

- Portable: `dist/GPDF-0.2.8/`, con 81 archivos y 46.418.574 bytes, incluidas dependencias, documentación y licencias.
- ZIP: `dist/GPDF-0.2.8-Portable.zip`.
- Instalador: `dist/installers/GPDF-0.2.8-Setup.exe`, generado con Inno Setup 6.7.3.
- Manifiestos: `dist/GPDF-0.2.8/build-info.json` y `dist/installers/GPDF-0.2.8-Setup.json`.
- Hashes de los artefactos distribuibles: `dist/SHA256SUMS-0.2.8.txt`.

Los manifiestos identifican el commit base `ec6562b8c374a60894e7f173dc54fffdf56d657a` y declaran cambios sin commit. El instalador se genera sin firma digital; no se regeneró una variante TestSigned. Los binarios y registros locales están excluidos de Git por las reglas existentes de `dist/` y `build*/`.

## Instalador

Validación aislada mediante `scripts/validate-installer.ps1 -BuildDirectory build-release-028`: instalación y actualización, hashes de todos los archivos, comando e icono registrados, apertura de PDF con el ejecutable instalado, desinstalación y conservación de archivos ajenos y de la elección del lector PDF predeterminado. Inglés predeterminado aprobado. Registro: `build-release-028/validation-installer-english.log`.

La misma validación con `-InstallerLanguage spanish` también aprobó. Registro: `build-release-028/validation-installer-spanish.log`.

La prueba utiliza una rama de registro temporal y una carpeta del proyecto. No ejercita los accesos directos ni la entrada real de Aplicaciones instaladas. La impresión física y la edición de anotaciones en lectores externos siguen requiriendo comprobación manual.

## Actualizador — 19 de octubre de 2026

Se integra GitHub Releases para `LautaroGrant/gpdf`, comprobación automática al iniciar, control manual desde Ayuda, preferencia persistente, omisión de versión, descarga cancelable y validación de tamaño y SHA-256 antes del cierre normal e inicio del instalador. La edición portable abre la página oficial. Catálogo inglés regenerado: 275 mensajes. Detalles y requisitos de publicación en `docs/updates.md`.

Compilación Release y CTest: 10/10 pruebas aprobadas en 11,56 segundos. Se añaden casos de orden numérico, versiones iguales y anteriores, prereleases, borradores, URLs ajenas o sin HTTPS, hashes inválidos y límites de tamaño; la suite UI verifica las acciones y la persistencia de la preferencia. Registro: `build-release-028/update-build.log`. Se regeneraron paquete, instalador, ZIP y hashes. El portable final contiene 81 archivos y 46.486.491 bytes.

La validación aislada del instalador final en inglés aprobó instalación, actualización, hashes, apertura, desinstalación y conservación de archivos ajenos y del lector predeterminado. Registro: `build-release-028/validation-update-installer-english.log`.

La misma validación del instalador final en español también aprobó. Registro: `build-release-028/validation-update-installer-spanish.log`.

La comprobación pública confirmó que el repositorio es público y que `v0.2.7` existe con `prerelease: true` y `draft: false`. Por eso `/releases/latest` devuelve HTTP 404: no hay una publicación estable disponible. No se modificó ni publicó una release ni se ejecutó una actualización real sobre la instalación del usuario. La descarga y ejecución de una futura versión completa quedan pendientes de una publicación estable pública compatible; las pruebas locales de selección de versiones utilizan datos controlados.

## Borrador independiente

La acción existente `editTool4` se mueve del desplegable de Lápiz a la barra de herramientas, entre Lápiz y Resaltar, conservando el grupo de selección y el borrado existente de resaltados, subrayados y trazos. El nuevo icono utiliza geometría vectorial en `GpdfToolIcon`; el cursor se genera con la misma silueta, ajustado a la densidad de pantalla y con borde blanco para distinguirlo sobre fondos claros y oscuros.

F5 selecciona el borrador y aparece en el tooltip y en la configuración de atajos. Se migra el antiguo atajo vacío al nuevo valor predeterminado y se conservan combinaciones personalizadas. El cursor de goma se muestra sobre las páginas, también durante el arrastre y al soltar el botón; fuera de la página se utiliza la flecha.

Las regresiones de interfaz comprueban posición en la barra, ausencia en el desplegable de Lápiz, icono en distintos tamaños, teclas F1 a F5, selección exclusiva, cursor sobre y fuera de la página, borrado de trazos y deshacer. Registro de compilación y CTest: `build-release-028/eraser-build.log`.

Resultado: 10/10 pruebas aprobadas en 11,80 segundos. La ejecución adicional de interfaz con capturas también aprobó; se revisaron `build-release-028/eraser-validation/eraser-icon.png`, `eraser-cursor.png` y `eraser-selected.png`, además de la barra en modo oscuro en `build-release-028/eraser-toolbar.png`. Registro: `build-release-028/eraser-visual-validation.log`.

Paquete portable, ZIP, instalador y SHA256SUMS regenerados con el borrador. El portable contiene 81 archivos y 46.492.753 bytes.

La validación aislada del instalador también aprobó instalación, actualización, hashes, apertura, desinstalación y conservación de archivos ajenos y del lector predeterminado. Registro: `build-release-028/validation-eraser-installer.log`.
