# Cambios

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
