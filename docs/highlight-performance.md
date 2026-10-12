# Evaluación de resaltado: Marechal, Adán Buenosayres

Fecha: 2026-10-11. PDF real del escritorio: 407 páginas, 3.364.960 bytes.

Prueba en Windows con Qt Fusion: página 11, ocho resaltados consecutivos de 61 caracteres, con el panel Anotaciones oculto y visible. Se esperan 1.500 ms antes de iniciar cada serie y se atiende el bucle de eventos durante 450 ms después de cada acción. Los tiempos de acción incluyen la creación del resaltado y la actualización inmediata de la interfaz. Las pausas posteriores corresponden al tiempo de ejecución de processEvents, medido por separado.

| Panel | Mediana antes | Mediana después | Máximo antes | Máximo después | Mayor pausa posterior antes / después |
|---|---:|---:|---:|---:|---:|
| Oculto | 63.3 ms | 7.87 ms | 250.48 ms | 10.04 ms | 119.07 / 3.85 ms |
| Visible | 112.9 ms | 9.58 ms | 378.21 ms | 11.26 ms | 583.48 / 5.46 ms |

En una primera ejecución sin el desglose de tiempos hubo acciones de hasta 755,71 ms. Los tiempos varían entre ejecuciones; no son una garantía para todas las páginas, archivos o máquinas.

El historial incremental tarda generalmente entre 7 y 17 ms en la medición inicial. El redibujado de geometría cuesta menos de 1 ms, pero la espera por el mutex compartido de PDFium y la reconstrucción de Anotaciones generan los bloqueos mayores. El análisis en segundo plano recorría las 407 páginas tras cada edición, aun con el panel oculto.

Cambios: caché de anotaciones por página, invalidada únicamente para la página editada; no iniciar el recorrido completo cuando un panel de documento grande está oculto o se usa el filtro de página actual; actualizar la lista al abrir el panel; agrupar las llamadas inmediatas de edición y actualización bajo el mismo acceso a PDFium; conservar la imagen anterior al editar; pintar sin bloquear mientras PDFium trabaja en segundo plano. Se conservan la serialización de PDFium y el historial incremental para evitar concurrencia insegura y cambios en deshacer/rehacer.

La prueba comprueba el SHA-256 del PDF original antes y después, sin escribirlo. Los guardados se hacen en una carpeta temporal. Incluye deshacer, rehacer y reapertura de la copia guardada, verificando la cantidad de anotaciones.

La herramienta opcional tests/perf_probe.cpp se habilita mediante STUDYPDF_BUILD_PERF_PROBE=ON. No se incorpora al paquete distribuido. Los perfiles detallados quedan en build-window-fixes/marechal-before-profile.log y marechal-after-profile.log.
