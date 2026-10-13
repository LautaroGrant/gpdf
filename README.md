# GPDF 0.2.3

GPDF (antes StudyPDF) es un lector de PDF para estudiar en Windows x64, construido con C++20, Qt Widgets y PDFium. Estado: beta local funcional.

## Ejecutar

La distribución vigente es `dist/GPDF-0.2.3/GPDF.exe`. La carpeta completa debe mantenerse junto al ejecutable; no hace falta instalar Qt. `build-info.json` identifica versión, commit, estado del código y hashes de los binarios. Las distribuciones anteriores se conservan como referencia.

Abrí un PDF desde Inicio o Archivo. Abrir otro archivo agrega una pestaña. Windows permite asociar PDFs mediante «Abrir con». La aplicación reutiliza la instancia abierta y recupera su estado maximizado o normal al abrir PDFs desde el escritorio.

## Funciones

- Vista continua, página individual y dos páginas; zoom, rotación y pantalla completa. El indicador de página permite escribir el número de destino y confirmar con Enter.
- Selección y copia de texto; búsqueda sin distinguir mayúsculas ni acentos.
- Comentarios, cuadros de texto, resaltados y lápiz; edición, color, borrado y deshacer/rehacer. Crear un comentario o hacer clic en su icono con Seleccionar o Comentario abre su contenido sobre la página; permite copiar texto y se cierra con ×, Esc o un clic fuera.
- Pestañas, miniaturas, índice del PDF, marcadores personales y progreso de lectura. El clic central sobre una pestaña cierra ese documento, con aviso si tiene cambios sin guardar.
- Recuperación de cambios y restauración de sesión. Las copias de recuperación no reemplazan el original.
- Exportación de anotaciones a Markdown, texto o HTML; impresión y vista previa.
- Interfaz oscura con páginas blancas y atajos personalizables.

Con Seleccionar, arrastrá el icono de un comentario para moverlo dentro de la página o a otra página. Acercalo al borde superior o inferior de la vista para desplazarte durante el arrastre; Esc cancela. El traslado conserva el contenido y permite deshacerlo en un solo paso.

Las anotaciones se guardan dentro del PDF. Los marcadores personales y el progreso son locales y se identifican por ruta. El cambio de nombre conserva la ubicación de configuración de StudyPDF para mantener preferencias, marcadores y recuperación. Guardar como adopta el nuevo archivo como documento actual.

## Atajos predeterminados

| Acción | Atajo |
|---|---|
| Seleccionar / Comentario / Texto / Lápiz | F1 / F2 / F3 / F4 |
| Resaltar con color primario / secundario | Ctrl+S / Ctrl+Shift+S |
| Guardar / Guardar como | Ctrl+G / Ctrl+Shift+G |
| Buscar / Ir a página | Ctrl+B / Ctrl+I |
| Mostrar anotaciones / Seleccionar todo | Ctrl+A / Ctrl+Shift+A |
| Agregar marcador personal | Ctrl+D |
| Siguiente pestaña / Anterior | Ctrl+Tab / Ctrl+Shift+Tab |
| Pantalla completa | F11; Esc para salir |
| Zoom con rueda (25 puntos porcentuales) | Ctrl+rueda |
| Zoom con botones (10 puntos porcentuales) | + / − en la barra |

Un clic con el botón central del mouse activa el desplazamiento automático. Un indicador queda en el punto de inicio; el cursor nativo de Windows cambia a arriba o abajo al mover el mouse. Otro clic o Esc lo detiene.

Suprimir borra una anotación seleccionada; dentro de un campo de texto borra caracteres. Enter confirma comentarios y textos; Shift+Enter agrega una línea. El menú Atajos permite modificar estas combinaciones.

## Compilar y verificar

En PowerShell, con Python 3.11+ para instalar las herramientas:

```powershell
./scripts/bootstrap.ps1
./scripts/build.ps1 -Package -PackageName GPDF-0.2.3
```

Las dependencias se instalan en el proyecto. La compilación ejecuta tres suites: núcleo/persistencia, interfaz y corpus PDF. Las pruebas de interfaz usan Qt offscreen y necesitan acceso a archivos temporales e IPC local de Windows.

Para verificar archivos propios sin modificar los originales:

```powershell
./scripts/validate-corpus.ps1 -PdfPaths 'C:/ruta/libro.pdf','C:/ruta/apuntes.pdf'
./scripts/validate-corpus.ps1 -Stress -OutputDirectory './build/corpus-output'
# Si Poppler está instalado y en PATH:
./scripts/validate-corpus.ps1 -Poppler -PdfPaths 'C:/ruta/libro.pdf'
```

Sin rutas se generan un documento de 130 páginas y un PDF de imágenes sin texto. `-Stress` agrega un caso con 64 MiB de relleno: comprueba tamaño de entrada, no complejidad gráfica. `-OutputDirectory` conserva copias en una subcarpeta única; de lo contrario se eliminan los temporales.

## Límites actuales

No incluye OCR: un escaneo sin capa de texto se puede leer y anotar, pero no buscar ni resaltar palabras. Los PDFs se cargan completos en memoria. Apertura y guardado siguen siendo operaciones síncronas; se redujo el alcance del bloqueo de PDFium durante la lectura, recuperación y confirmación del guardado, pero documentos grandes pueden generar pausas. El historial tiene límites de entradas y memoria.

La compatibilidad de edición con Edge/Acrobat y la impresión física requieren validación manual. Las suites automatizadas no cubren todos los PDFs posibles. La separación de componentes es gradual: la ventana aún concentra coordinación de sesión, búsqueda y herramientas.

## Desarrollo y distribución

El repositorio Git es local, sin remoto configurado. `baseline-0.1` conserva el punto de partida. Fuentes y PDFs sintéticos se versionan; herramientas, compilaciones, paquetes y documentos personales quedan fuera del repositorio.

- `docs/architecture.md`: componentes y decisiones de concurrencia.
- `docs/validation-0.2.3.md`: verificación de traslado de comentarios, cierre de pestañas, búsqueda y navegación por número.
- `docs/validation-0.2.2.md`: verificación de apertura externa, zoom, ventanas de comentarios y cursores nativos.
- `docs/validation-0.2.1.md`: verificación del cambio de nombre a GPDF.
- `docs/validation-0.2.0.md`: cobertura y mediciones de la consolidación anterior.
- `CHANGELOG.md`: cambios por versión.
- `docs/development-history.md`: notas históricas anteriores.

Qt se enlaza dinámicamente. Los paquetes incluyen avisos de Qt y PDFium en `licenses/`. Las herramientas de pruebas no se distribuyen. Dependencias: Qt 6.8.3 y PDFium chromium/8086, con hash fijado en bootstrap.
