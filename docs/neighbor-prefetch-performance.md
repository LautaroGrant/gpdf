# Precarga de páginas vecinas

## Implementación

Después de completar los renders visibles, un temporizador de 80 ms solicita una vista previa de la página siguiente y luego de la anterior. Cada imagen mide como máximo 512 × 512 píxeles (menos de 1 MiB para una página rectangular). Se conserva en la caché de vistas previas existente de 8 MiB; la caché de resolución completa mantiene su límite de 64 MiB. Estos límites no representan la RAM total del proceso.

Se procesa un solo render especulativo a la vez, con prioridad baja en el pool. Se intenta adquirir el mutex de PDFium sin esperar y se descarta trabajo que ya no corresponde al documento, revisión, rotación o vecindad actual. No se solicitan precargas mientras haya renders visibles pendientes. Los errores de render especulativo se recuerdan hasta invalidar la caché, para evitar reintentos continuos de páginas dañadas.

Una vez iniciado, el render de PDFium no se interrumpe: una navegación que coincida con ese trabajo puede esperar su finalización. La resolución pequeña reduce el costo del raster, pero un PDF complejo todavía puede requerir mucho procesamiento. La primera página, los saltos lejanos y la navegación más rápida que la precarga pueden seguir mostrando blanco. Si hay vista previa, la página aparece primero con menor nitidez y se reemplaza por el raster final.

## Comparación reproducible

Build Release, Qt 6.8.3 / MinGW 13.1, PDFium incluido en el proyecto. El probe usa la ventana real `Window`, sus paneles y su `PageView`; configura un directorio temporal de preferencias. Se comparan dos procesos independientes con el mismo harness, librerías y opciones, cambiando únicamente los headers `Window.h` / `PageView.h` por los del commit previo en el baseline.

Tres ejecuciones por documento y variante, intercaladas. Modo de página única, ajuste de página, ventana de 1100 × 950, plataforma Qt offscreen. Se recorren las primeras 30 páginas de `many-pages.pdf` y las tres páginas de `scan.pdf` del corpus sintético local, con 200 ms de pausa después de completar cada raster visible. No se editan los documentos. Esta prueba mide el pipeline y la memoria del proceso; no mide la latencia de presentación del compositor de Windows ni garantiza los mismos resultados en todos los PDFs.

`preview_hits` indica que la vista previa de la página destino estaba disponible antes de solicitar su primer repintado. `full_mean_ms` mide desde ese repintado hasta que la caché contiene el raster final. CPU es tiempo acumulado del proceso (kernel + usuario), no porcentaje de ocupación. Se usa `GetProcessMemoryInfo`: working set, memoria privada y pico de working set. La granularidad de `GetProcessTimes` vuelve especialmente ruidosas las mediciones del documento de sólo tres páginas.

Para repetir:

1. Guardar los headers originales en un directorio separado antes de modificar el código (o extraerlos con `git show <commit>:src/PageView.h` y `git show <commit>:src/Window.h`).
2. Configurar CMake con `-DGPDF_BUILD_PREFETCH_PROBE=ON -DGPDF_PREFETCH_BASELINE_DIR=<directorio de headers originales>`.
3. Compilar `prefetch_baseline`, `prefetch_probe` y `GPDF` con las mismas opciones Release.
4. Ejecutar ambos probes con la misma ruta de PDF; usar `QT_QPA_PLATFORM=offscreen` y un `TEMP` / `TMP` escribible. Cada ejecución abre un proceso nuevo.

## Validación

`neighbor_prefetch` verifica la precarga de ambos vecinos, que no ocupen la caché de resolución completa, las tres rotaciones, la navegación rápida, los límites de caché y el reinicio de documento. Las cuatro pruebas `render_quality_*` comparan los píxeles finales con un render PDFium directo. También se ejecutaron `navigation_efficiency` y `annotation_ui`; esta última requiere comunicación local de instancia única y pasó fuera del sandbox.

## Resultados con la ventana completa

Promedios de tres procesos por variante. Baseline: commit `9a91e87ea2d214ef45ca1c340854e199353b80e7`.

| Documento / métrica | Sin precarga | Con precarga |
|---|---:|---:|
| Texto, RAM residente (MiB) | 163,865 | 164,185 |
| Texto, memoria privada (MiB) | 206,356 | 206,482 |
| Texto, pico RAM residente (MiB) | 164,495 | 164,878 |
| Texto, CPU acumulada del recorrido (ms) | 8609,4 | 8864,6 |
| Texto, tiempo medio hasta raster final (ms) | 168,0 | 167,9 |
| Texto, transiciones con preview disponible | 0/87 | 51/87 |
| Escaneado, RAM residente (MiB) | 41,232 | 41,427 |
| Escaneado, memoria privada (MiB) | 19,549 | 19,354 |
| Escaneado, CPU acumulada del recorrido (ms) | 458,3 | 432,3 |
| Escaneado, tiempo medio hasta raster final (ms) | 162,1 | 116,4 |
| Escaneado, transiciones con preview disponible | 0/6 | 5/6 |

En texto: +0,32 MiB de RAM residente, +0,13 MiB de memoria privada, aproximadamente +3 % de CPU acumulada y tiempo de calidad final prácticamente igual. Hubo preview inmediato en el 59 % de las transiciones rápidas; el resto aún puede mostrar blanco. En el escaneado de sólo tres páginas, el costo de arranque y la variabilidad dominan: no debe interpretarse la disminución de CPU o del tiempo medio como una aceleración demostrada del renderer.

Una medición preliminar del `PageView` aislado, sin los paneles de `Window`, obtuvo previews en 29/29 transiciones de texto. No se usa ese resultado como rendimiento de la aplicación completa: la prueba con la ventana muestra la competencia con sus demás tareas.

Los archivos fuente de medición son `tests/prefetch_probe.cpp` y `tests/prefetch_smoke.cpp`. El ejecutable nuevo con sus dependencias está en `build-prefetch/GPDF.exe`.

### Mediciones individuales

```text
run=1 document=many-pages variant=baseline pages=30 preview_hits=0 full_mean_ms=148.806 full_max_ms=609.233 cpu_ms=8781.25 working_MiB=163.848 private_MiB=206.355 peak_MiB=164.508
run=1 document=many-pages variant=probe pages=30 preview_hits=17 full_mean_ms=175.412 full_max_ms=529.025 cpu_ms=9000 working_MiB=164.156 private_MiB=206.367 peak_MiB=164.879
run=1 document=scan variant=baseline pages=3 preview_hits=0 full_mean_ms=159.497 full_max_ms=431.728 cpu_ms=468.75 working_MiB=41.25 private_MiB=19.5508 peak_MiB=55.9062
run=1 document=scan variant=probe pages=3 preview_hits=1 full_mean_ms=22.7169 full_max_ms=33.1188 cpu_ms=390.625 working_MiB=41.6016 private_MiB=19.3984 peak_MiB=55.9297
run=2 document=many-pages variant=baseline pages=30 preview_hits=0 full_mean_ms=197.126 full_max_ms=332.939 cpu_ms=8578.12 working_MiB=163.926 private_MiB=206.398 peak_MiB=164.461
run=2 document=many-pages variant=probe pages=30 preview_hits=15 full_mean_ms=153.183 full_max_ms=616.046 cpu_ms=8765.62 working_MiB=164.148 private_MiB=206.504 peak_MiB=164.926
run=2 document=scan variant=baseline pages=3 preview_hits=0 full_mean_ms=153.184 full_max_ms=418.281 cpu_ms=390.625 working_MiB=41.2109 private_MiB=19.5234 peak_MiB=55.8867
run=2 document=scan variant=probe pages=3 preview_hits=2 full_mean_ms=160.281 full_max_ms=417.918 cpu_ms=421.875 working_MiB=41.3242 private_MiB=19.3242 peak_MiB=55.8867
run=3 document=many-pages variant=baseline pages=30 preview_hits=0 full_mean_ms=158.038 full_max_ms=542.815 cpu_ms=8468.75 working_MiB=163.82 private_MiB=206.316 peak_MiB=164.516
run=3 document=many-pages variant=probe pages=30 preview_hits=19 full_mean_ms=175.123 full_max_ms=610.903 cpu_ms=8828.12 working_MiB=164.25 private_MiB=206.574 peak_MiB=164.828
run=3 document=scan variant=baseline pages=3 preview_hits=0 full_mean_ms=173.656 full_max_ms=457.118 cpu_ms=515.625 working_MiB=41.2344 private_MiB=19.5742 peak_MiB=55.9023
run=3 document=scan variant=probe pages=3 preview_hits=2 full_mean_ms=166.262 full_max_ms=438.243 cpu_ms=484.375 working_MiB=41.3555 private_MiB=19.3398 peak_MiB=55.8945
```
