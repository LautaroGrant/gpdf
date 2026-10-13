# Cambios

## 0.2.3 — 12 de octubre de 2026

- Seleccionar permite mover comentarios dentro de la página y entre páginas, con desplazamiento en los bordes y un único paso de deshacer.
- Clic central sobre una pestaña para cerrar el documento, conservando el aviso de cambios sin guardar.
- Búsqueda predeterminada con Ctrl+B; se migra el antiguo Ctrl+F y se conservan los atajos personalizados.
- Campo editable de página actual con navegación directa por número y Enter.

## 0.2.2 — 12 de octubre de 2026

- Abrir un PDF desde otra instancia restaura la ventana maximizada o normal sin perder el estado anterior.
- Botones + y − de la barra: 10 puntos porcentuales; Ctrl+rueda conserva pasos de 25.
- Comentarios en una ventana sobre la página, accesibles desde Seleccionar y Comentario, con copia de texto y cierre mediante ×, Esc o clic fuera.
- Desplazamiento automático con indicador de origen y cursores verticales nativos de Windows según dirección.

## 0.2.1 — 12 de octubre de 2026

- Proyecto, ventana y ejecutable renombrados a GPDF.
- Configuración, recuperación, instancia única y metadatos de anotaciones anteriores conservan compatibilidad con StudyPDF.
- README y empaquetado actualizados; repositorio exclusivamente local.

## 0.2.0 — 12 de octubre de 2026

- Repositorio Git local y punto de partida conservado; sin remoto ni push.
- Separación del arranque, Window, PageView, componentes auxiliares, traducciones, persistencia de sesión y exportación. Las pruebas ya no incluyen main.cpp.
- Corpus automatizado: 130 páginas, PDF escaneado sin texto, anotaciones Unicode, deshacer/rehacer, recuperación y reapertura; verificación de originales por SHA-256.
- Herramienta para validar PDFs propios y entradas de 64 MiB sin incluirlos en el repositorio.
- Lectura del archivo y escritura de recuperación fuera del mutex global de PDFium; confirmación del guardado fuera del bloqueo y control de revisión para conservar cambios concurrentes.
- Presentación de resultados de búsqueda mediante lotes de 64 elementos.
- Versión única en VERSION, manifiesto de distribución y documentación reorganizada.

## 0.1 — Base previa

Lectura, anotaciones, pestañas, búsqueda, recuperación, exportación, impresión, marcadores, modo oscuro y atajos. Auditoría y mediciones previas en docs/audit-2026-10-12.md y docs/highlight-performance.md.
