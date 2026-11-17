# Revisión de apertura y rendimiento

Cambios del 13 de noviembre de 2026:

- `PdfDocument::open` y `setPage` conservan la validación de la página, pero no
  extraen texto hasta que se necesita para selección, hit testing o consulta.
  La caché de texto se conserva con el documento al intercambiar pestañas y se
  libera al cambiar de página o cerrar. Las llamadas siguen protegidas por el
  mutex de PDFium.
- Con el panel de anotaciones cerrado se consultan sólo las anotaciones de la
  página activa. Antes, los documentos de hasta 50 páginas se recorrían enteros.
- La ventana se muestra antes de restaurar la sesión y abrir los archivos de
  inicio. El registro de asociaciones se difiere; los procesos que sólo reenvían
  archivos a la instancia existente ya no realizan ese trabajo.
- Los rangos de selección se limitan al texto disponible, incluido OCR. Se evitan
  desbordamientos aritméticos y asignaciones enormes con índices fuera de rango.

## Medición reproducible

`open_performance` genera un PDF temporal de tres páginas con texto denso, descarta
la primera iteración y comunica la media de cinco aperturas con cachés calientes.
Mide por separado `open()` y la primera extracción de texto. También verifica
selección, navegación, hit testing, intercambio, cierre y apertura fallida.

La extracción diferida traslada trabajo a la primera interacción con texto; no
acelera el motor de extracción. Esta microprueba no mide el inicio completo del
proceso ni el tiempo hasta el primer render, y no representa todos los PDFs.
La lectura completa del archivo y la carga de páginas siguen siendo síncronas.

Ejecutar con `QT_QPA_PLATFORM=offscreen` o mediante CTest (`pdf_open_performance`).
Los tiempos son informativos, sin umbrales frágiles dependientes del hardware.

Resultados locales, compilación Release, media de cinco iteraciones:

| Operación | Antes | Después |
| --- | ---: | ---: |
| `PdfDocument::open()` | 55,45 ms | 19,80 ms |
| Primera consulta de texto | <0,01 ms | 26,15 ms |

La apertura del núcleo bajó aproximadamente un 64 % en este documento sintético.
No es una estimación de mejora del arranque completo ni de PDFs escaneados.

## Validación

Compilación Release y las 23 pruebas de CTest verificadas, incluidas calidad de
render en cuatro escalas, OCR, anotaciones, deshacer/rehacer, recuperación,
idiomas, Workbench, precarga y transferencias de ventanas.

El directorio temporal virtual del sandbox provocó errores de guardado atómico;
se repitieron las pruebas afectadas con `TEMP` y `TMP` en
`tmp/performance-test-temp`. Las pruebas `annotation_ui` y
`window_transfer_native` se ejecutaron fuera del sandbox para permitir IPC y
arrastre nativo. No se modificó el guardado del producto para sortear estas
limitaciones del entorno.
