# Validación de GPDF 0.2.2

Fecha: 12 de octubre de 2026. Windows x64, Qt 6.8.3 y PDFium chromium/8086.

## Cambios y regresiones

1. **Apertura externa.** La instancia receptora restaura la ventana antes y después de abrir los documentos, conservando el estado visible anterior y consultando el estado nativo de Windows. La prueba minimiza una ventana maximizada, envía un PDF desde un segundo proceso por el canal real de instancia única y comprueba dos pestañas, estado maximizado y ausencia de minimización. Con Qt Windows también verifica IsZoomed/IsIconic. Se cubre la apertura con ventana maximizada visible y la recuperación de una ventana normal conservando su tamaño.
2. **Zoom.** Los botones +/− de la barra avanzan 10 puntos porcentuales. Ctrl+rueda conserva 25 y la acumulación de eventos de alta resolución. El menú Ver y sus atajos conservan el comportamiento previo. Las pruebas comprueban los pasos separados, límites y sincronización del porcentaje.
3. **Comentarios.** Crear una nota o hacer clic en una existente con Seleccionar/Comentario muestra su texto en un panel sobre la página. Es de solo lectura, admite copia, se limita a la región visible y se cierra mediante ×, Esc o clic fuera. Se verifican las tres vistas, Unicode, saltos de línea, texto literal con ángulos, selección/copia, cierre, modo oscuro y ausencia de modificaciones o notas duplicadas. Zoom, cambio de página/vista/documento y desplazamiento vertical cierran el panel para evitar contenido fuera de su ancla.
4. **Desplazamiento automático.** Se utilizan recursos de cursor de user32.dll presentes en este Windows: 32652 (neutro vertical), 32655 (arriba) y 32656 (abajo). Se conserva el indicador en el punto de inicio y la aceleración existente. La prueba verifica carga, cambio de dirección, uso del handle nativo con Qt Windows y limpieza al detenerse. Los handles compartidos no se destruyen; si faltaran recursos se usa el cursor vertical estándar como respaldo.

## Resultados

- Las tres suites CTest aprobaron: pdf_roundtrip, annotation_ui y pdf_corpus.
- annotation_ui aprobó también con Qt Windows, incluido el nuevo envío desde un proceso secundario y la restauración del marco real.
- Se revisaron capturas de comentarios en modo claro/oscuro y los recursos de cursor cargados. Los artefactos se conservan en build-gpdf/verification-0.2.2-native, excluido de Git.
- Las pruebas anteriores de anotaciones, persistencia, búsqueda, navegación, recuperación y pantalla completa siguen aprobando.

La minimización usada en la regresión reproduce el estado de una ventana al ir al escritorio; no automatiza el gesto Win+D sobre otras aplicaciones del usuario. Los límites de edición en Edge/Acrobat e impresión física permanecen.

## Reproducir

```powershell
./scripts/build.ps1 -BuildDirectory build-gpdf
$env:QT_QPA_PLATFORM='windows'
./build-gpdf/ui_smoke.exe ./build-gpdf/verification-0.2.2-native
```

Referencias de las APIs de recursos: [LoadCursorW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadcursorw) y [cursores de Windows](https://learn.microsoft.com/en-us/windows/win32/menurc/cursors). Los identificadores de desplazamiento se comprobaron contra los recursos del sistema local; no se copian ni redistribuyen imágenes del sistema.
