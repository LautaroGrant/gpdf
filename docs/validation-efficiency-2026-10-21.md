# Revisión de eficiencia — 21 de octubre de 2026

## Alcance

Revisión del código propio de `src`, CMake y scripts de compilación, instalación y validación. Se revisaron renderizado, navegación, búsqueda, anotaciones, historial, persistencia, exportación, impresión, traducciones, actualizaciones e integración con Windows. No se modificaron Qt, PDFium ni los PDF originales. Se conservaron los cambios locales que ya existían al comenzar. No se ejecutaron commits, pulls ni staging.

## Cambios

- La localización de páginas en vista continua utiliza búsqueda binaria sobre la geometría ordenada. El repintado recorre sólo las páginas candidatas al área visible o dañada. En vista individual/doble se consultan como máximo dos páginas. También se utiliza este acceso para el anclaje del zoom y la sincronización del desplazamiento.
- Cambiar de página en vista continua conserva la disposición existente cuando ésta no depende de la página activa. Ajustar a página sigue recalculando para respetar documentos con tamaños distintos.
- Un cambio simultáneo de ancho y alto del viewport produce una sola reconstrucción de geometría por llamada.
- Las imágenes de previsualización se reducen en el trabajador de renderizado, fuera del mutex de PDFium, antes de enviarlas a la interfaz. La inserción en las cachés sigue ocurriendo en el hilo de UI.
- El panel conserva sus filas cuando las anotaciones y filtros no cambian. Navegar sigue actualizando selección, botones, página y acciones. Cambios de contenido, filtros y resultados asíncronos reconstruyen la lista.
- El índice de búsqueda procesa ASCII sin crear y convertir un `QString` por carácter. Se conserva la normalización Unicode y el mapeo a los índices originales de PDFium.
- El guardado calcula SHA-256 durante la escritura; elimina la reapertura y lectura completa posterior. Abrir y comprobar cambios externos calculan el hash fuera del mutex global de PDFium.
- Se quitaron una búsqueda de página duplicada, una condición inalcanzable y un override que sólo llamaba a la implementación base.
- Se reparó el target opcional `perf_probe`: implementación de actualizaciones, definición de versión y copia de PDFium. También se garantiza la copia de PDFium al compilar `update_smoke` por separado.

## Verificación

La suite inicial pasó sus 10 pruebas antes de modificar código. Se agregó `navigation_efficiency` con un PDF generado de 257 páginas de tamaños distintos. Compara con recorridos lineales independientes los bordes, espacios entre páginas, áreas visibles y páginas más cercanas; recorre los tres modos, cuatro rotaciones y cuatro ajustes de escala. Compara la navegación con una reconstrucción completa de geometría y comprueba el hash del guardado y la detección de modificaciones externas.

La prueba de interfaz verifica además que navegar desde la lista global conserve un índice persistente del modelo de anotaciones. Las pruebas existentes cubren búsquedas Unicode, edición, deshacer/rehacer, recuperación, cambio de documento y calidad de rasterizado a escalas 1, 1.25, 1.5 y 2.

Medición inicial de 100.000 consultas de página: 5,01 ms con búsqueda binaria frente a 73,50 ms con el recorrido lineal, en un documento de 257 páginas. Es un microbenchmark de esa operación; no representa una aceleración equivalente de toda la aplicación ni una garantía de latencia.

Resultados finales en Release (`build-release-028`):

- CTest: **11/11**, 12,10 segundos, incluidos los cuatro controles de calidad de renderizado. Medición final: 5,01 ms frente a 74,59 ms para las consultas de página.
- Corpus local: **55/55 PDF**, con apertura, lectura de texto, renderizado, edición en copias, recuperación, guardado y reapertura. Todos informaron `original_unchanged=yes`. Log: `build-release-028/efficiency-corpus.log`.
- Estrés: documentos generados de 3 y 130 páginas y variante ampliada en 64 MiB; todos pasaron y conservaron los originales.
- `GPDF_BUILD_PERF_PROBE=ON`: compiló y ejecutó sobre `tests/data/bookmarks.pdf`, con panel oculto/visible, ocho resaltados por recorrido, deshacer/rehacer y reapertura del archivo guardado. Log: `build-release-028/efficiency-perf.log`. Esta ejecución valida el instrumento y el comportamiento; no se utiliza como comparación de rendimiento antes/después.
- `git diff --check`: sin errores de whitespace. `.gitignore` permite mostrar el nuevo test como archivo no rastreado para que no quede oculto por la regla general de `tests`; no se agregó ningún archivo al índice.

Ejecutable actualizado: `build-release-028/GPDF.exe`. El paquete y los instaladores de `dist` conservan su versión anterior.

## Límites

Abrir, serializar, guardar recuperaciones e imprimir aún incluyen operaciones síncronas. PDFium continúa protegido por su mutex global. Las mejoras reducen trabajo redundante y contención; no garantizan ausencia de pausas con cualquier PDF. No se cambió ese modelo de concurrencia ni el orden de persistencia, que requieren una migración específica para conservar correctamente documentos, revisiones y recuperación.

La validación de interfaz se ejecuta con la plataforma offscreen. No equivale a una comprobación manual de todos los controles nativos de Windows. No se modificaron ni regeneraron los instaladores publicados de `dist`.
