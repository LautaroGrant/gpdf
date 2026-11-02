# GPDF 0.3.3 - Preparación de versión

Fecha: 2 de noviembre de 2026.

## Notas de versión

- Indicador de páginas vacío cuando no hay documentos abiertos, conservando el espacio de la barra.

- Hipervínculos externos HTTP/HTTPS y correo, con apertura en la aplicación predeterminada.
- Navegación a destinos internos del PDF, con coordenadas XYZ cuando existen.
- Cursor de mano y vista previa del destino. El arrastre conserva la selección de texto.
- Reutilización de una página auxiliar para coordenadas, enlaces y detección de texto, evitando cargas repetidas al mover el cursor.
- Interrupción del clic, doble clic y borrador si falla la activación de la página.
- Tooltip de enlaces estable y traducción al inglés corregida.
- Pruebas de regresión para enlaces, rotación, zoom, arrastre y destinos ilegibles. Validación con PDFs reales de 495 y 2 páginas.
- Limpieza de compilaciones antiguas y copias de recuperación del editor.

Los enlaces deben estar incorporados al PDF. No se generan marcadores ni tablas de contenidos. Se conserva el zoom actual para destinos cuyo modo de ajuste no es XYZ. Los resultados del OCR siguen siendo locales y no se incrustan como capa de texto al guardar.

## Release notes

The page indicator stays blank when no PDF is open, preserving toolbar spacing. GPDF 0.3.3 adds clickable PDF links for HTTP/HTTPS, email and internal destinations, with a hand cursor and destination preview. Text selection by dragging remains available. Pointer interactions reuse an auxiliary page instead of repeatedly loading inactive pages. Failed page activation no longer lets selection or eraser operations continue on the previous page. Link tooltips remain stable and use the existing English translation. Regression tests cover links, zoom, rotation, dragging and unreadable destinations.

## Archivos de distribución

- `dist/installers/GPDF-0.3.3-Setup.exe`
- `dist/GPDF-0.3.3-Portable.zip`
- `dist/SHA256SUMS-0.3.3.txt`
- Distribución descomprimida: `dist/GPDF-0.3.3/`.

El portable conserva las preferencias y cachés de usuario habituales de Windows. El instalador mantiene la carpeta predeterminada estable y la ubicación de una instalación anterior. Los manifiestos registran la base Git y los cambios locales sin commit. Preparación local, sin commit, tag, push ni publicación. Resultados en `validation-0.3.3.md`.
