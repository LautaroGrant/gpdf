# Validación de StudyPDF 0.2.0

Fecha: 12 de octubre de 2026. Verificación local en Windows x64; Qt 6.8.3 y PDFium chromium/8086. No se cambiaron versiones de dependencias.

## Cobertura y resultados

- CTest: pdf_roundtrip, annotation_ui y pdf_corpus aprobadas. Núcleo, reapertura, errores, recuperación, tres vistas y funciones de interfaz.
- Interfaz con Qt Windows aprobada, además de offscreen. No se hizo impresión física ni se automatizó el diálogo nativo de impresión.
- Seis PDFs reales locales: cuatro de una página, uno de ocho y uno de 407. Todas las páginas se recorrieron para texto/tamaño; primera, intermedia y última se renderizaron. Las anotaciones de prueba se guardaron en copias y se verificaron al reabrir. Los SHA-256 originales se conservaron.
- PDF escaneado generado: tres páginas, imagen visible y sin capa de texto; búsqueda vacía esperada. Se verificaron comentarios, historial, recuperación y guardado.
- PDF generado de 130 páginas con texto y vectores: búsqueda por acentos en la última página y persistencia aprobadas.
- Entrada de 64 MiB adicionales de relleno: aprobación de apertura, recorrido, recuperación y guardado. Es una prueba de tamaño de archivo, no de complejidad gráfica.
- Poppler abrió las copias y respaldos; renderizó 36 páginas representativas. Inspección visual de una página del libro y una del escaneo correcta. Hubo advertencias por fuentes de sustitución Symbol/ArialUnicode en el entorno de Poppler; los renderizados terminaron, pero no se afirma equivalencia visual píxel a píxel.

## Mediciones actuales

Valores de una ejecución; no son garantías ni una comparación controlada de rendimiento.

| Caso | Apertura | Recorrido y renderizado | Recuperación | Guardado |
|---|---:|---:|---:|---:|
| Libro real de 407 páginas | 2 ms | 1.516 ms | 54 ms | 61 ms |
| Generado de 130 páginas | 0 ms* | 43 ms | 4 ms | 8 ms |
| Entrada con 64 MiB de relleno | 265 ms | 44 ms | 183 ms | 11 ms |

*Medición entera inferior a 1 ms. El recorrido incluye extracción de texto en todas las páginas y tres renderizados, no renderizado completo del libro. Guardar puede eliminar relleno, por lo que el último guardado no escribe una salida de 64 MiB.

Búsqueda completa de «Marechal» con Qt offscreen y la lista de resultados: 2.717,83 ms iniciales; 900,82 y 949,86 ms en repeticiones, con 427 coincidencias. El hash original quedó intacto. Estas cifras no prueban una mejora para todos los documentos o equipos.

## Reproducir

```powershell
./scripts/build.ps1 -BuildDirectory build-interface-polish
./scripts/validate-corpus.ps1 -BuildDirectory build-interface-polish -Stress -Poppler
./scripts/validate-corpus.ps1 -BuildDirectory build-interface-polish -PdfPaths 'C:/ruta/documento.pdf' -Poppler
$env:QT_QPA_PLATFORM='windows'
./build-interface-polish/ui_smoke.exe ./build-interface-polish/verification-native
```

Poppler es opcional para la validación independiente y no se incorpora a la aplicación. Los registros de esta ejecución y copias personales quedan bajo build-interface-polish, excluido de Git. tests/data sólo contiene PDFs sintéticos anteriores.

## Límites pendientes

La edición e interacción con anotaciones en Edge y Acrobat requieren una comprobación manual en esos productos. Poppler aporta un motor independiente para apertura/renderizado, no una prueba de edición en esos lectores. No se realizó fuzzing exhaustivo.

La carga completa en memoria y las operaciones síncronas de apertura/guardado permanecen. Se redujo la contención del mutex durante la lectura, escritura de recuperación y confirmación del archivo. Trasladar todas esas operaciones a tareas de fondo requiere un diseño adicional de cancelación y orden de checkpoints. No se presenta como resuelto.
