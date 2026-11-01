# GPDF 0.3.2 — Preparación de versión

Fecha: 1 de noviembre de 2026.

## Notas de versión

- Pestañas de documentos reordenables mediante arrastre.
- Controles de vista compactos con iconos; al cambiar de vista, el foco vuelve a la página.
- Navegación de resultados con flechas arriba/abajo y Enter/Shift+Enter, con prioridad sobre atajos personalizados en la búsqueda.
- Ajuste visual del texto de comentarios sin insertar saltos de línea.
- Iconos de Texto enmarcado y goma diagonal. Se mantienen los iconos actuales de Subrayado, Resaltar, aplicación y archivos PDF.
- Corrección de páginas en blanco por oscilación de las barras de desplazamiento, especialmente con dos páginas, Búsqueda y OCR.
- Configuración persistente de OCR automático, aplicar siempre y deshabilitado. Incluso deshabilitado, Ver → Aplicar OCR permite una ejecución manual. El modo deshabilitado no inicia OCR ni muestra sugerencias por su cuenta.
- Se conserva el texto seleccionable existente en documentos mixtos.
- Eliminación de ayudas al pasar el cursor sobre controles de página/zoom, resultados y la lista de anotaciones.
- Marca de activación de Anotaciones coherente en modo día y noche.

OCR requiere Windows PowerShell 5.1 y un idioma OCR instalado en Windows. El texto reconocido no se incrusta como una nueva capa al guardar el PDF.

## Release notes

GPDF 0.3.2 adds draggable document tabs, compact view icons, search navigation with Up/Down and Enter/Shift+Enter, visual comment wrapping, and updated Text/Eraser icons. It fixes blank pages caused by unstable scrollbar fitting, including two-page views with Search and OCR. Persistent OCR policies allow automatic, always-on and disabled operation; explicit manual OCR remains available when disabled. Existing native text is preserved. Hover help is removed from page/zoom controls and search/annotation lists.

## Archivos preparados

- `dist/installers/GPDF-0.3.2-Setup.exe`
- `dist/GPDF-0.3.2-Portable.zip`
- `dist/SHA256SUMS-0.3.2.txt`
- Distribución descomprimida: `dist/GPDF-0.3.2/`.

El portable funciona sin instalación; conserva el comportamiento actual de preferencias y caché de usuario en Windows.
El instalador usa la carpeta predeterminada estable `%LOCALAPPDATA%\Programs\GPDF` y conserva la ubicación de una instalación anterior.

Los manifiestos registran la base Git y los cambios locales sin commit. No se crean commits, tags, push ni publicaciones. Los archivos están preparados localmente para revisión. Véase `validation-0.3.2.md` para resultados y hashes.
