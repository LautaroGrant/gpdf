# Arquitectura

- `main.cpp`: configuración y arranque, instancia única, argumentos y restauración inicial.
- `Window.h`: coordinación de documentos, acciones, búsqueda, sesión y paneles.
- `PageView.h`: geometría, interacción con páginas, selección, texto y cachés de renderizado.
- `UiSupport.h`: controles auxiliares, delegado de búsqueda, iconos y soporte nativo de Windows.
- `SessionStore`: lectura, guardado atómico del manifiesto y limpieza posterior de respaldos.
- `AnnotationExport`: formato y escritura de resúmenes, separado de la validación de destinos de la ventana.
- `Translations.cpp`: instalación de traducciones, compilada en study_ui.
- `PdfDocument`: acceso a PDFium, persistencia, historial y cachés de texto/anotaciones.
- `SingleInstance`: elección de instancia y transmisión de rutas por IPC local.

Los componentes de interfaz mantienen métodos inline por ahora para conservar el comportamiento durante la extracción. Window sigue siendo el componente mayor; la persistencia de sesión y el formato de exportación tienen implementaciones independientes. La coordinación de búsqueda aún permanece en Window.

## Concurrencia y persistencia

PDFium usa un mutex recursivo global. Las tareas de búsqueda/renderizado validan revisiones antes de entregar resultados. La lectura del archivo de entrada ocurre antes de tomar ese mutex; la carga de PDFium y sustitución del documento siguen protegidas.

La recuperación retiene copias compartidas e inmutables de base y cola del historial bajo el mutex y luego escribe fuera de él. Una edición posterior reemplaza o separa su almacenamiento. QSaveFile conserva el reemplazo atómico.

Guardar serializa con PDFium bajo el mutex, lo libera para confirmar el archivo y vuelve a tomarlo. Sólo marca el documento como limpio si su revisión sigue siendo la guardada. No se agregan bucles de eventos anidados ni escrituras asíncronas de sesión, para conservar el orden entre checkpoint, manifiesto y eliminación de respaldos.

Estas medidas reducen contención con tareas de fondo. No convierten las operaciones de disco en tareas de fondo ni establecen una garantía de latencia. Los documentos y las cachés por pestaña pueden acumular memoria.

## Verificación

CTest ejecuta pdf_roundtrip, annotation_ui y pdf_corpus. La herramienta corpus_smoke recorre tamaños y texto de todas las páginas, renderiza primera/intermedia/última y edita sólo copias. Sus hashes se calculan por streaming. Documentos del usuario no se incorporan como fixtures ni se versionan.
