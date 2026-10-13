# Revisión de navegación, textos y eficiencia — 13 de octubre de 2026

## Correcciones funcionales

- El borde superior de los subrayados nuevos queda 0,75 puntos PDF debajo del texto. El desplazamiento aumenta con medio grosor para mantener esa separación también en líneas gruesas. Los cuadriláteros originales siguen identificando el texto; el dibujo, su rectángulo y la detección para borrar/seleccionar contemplan la nueva posición. Las pruebas verifican píxeles, grosor, deshacer/rehacer, guardado, reapertura y copia a otra página.
- Miniaturas: los cambios de fila con flechas navegan al documento. Se bloquean las señales durante actualizaciones programáticas para evitar navegación recursiva. Se verificaron eventos reales de mouse y teclado.
- Marcadores personales: botón Añadir/Add y conservación de la entrada seleccionada al navegar y renombrar. Se probaron navegación, renombrado y borrado mediante los botones.
- Enter en un resultado avanza desde la fila seleccionada y vuelve al primero al finalizar. Conserva el comportamiento del campo de búsqueda y espera resultados pendientes si la búsqueda sigue ejecutándose.
- Restauración de sesión: el filtro Subrayado ya no se recorta al índice del filtro Resaltado.

## Revisión de código y mejoras aplicadas

Se revisaron las rutas de apertura y guardado, historial, renderizado, selección, anotaciones, búsqueda, navegación, sesiones/recuperación, exportación, interfaz, traducciones, IPC y registro de Windows en los módulos propios de src. Las dependencias Qt y PDFium no se modificaron.

| Área | Mejora | Alcance |
|---|---|---|
| Navegación y búsqueda | Se conservan imágenes válidas al cambiar de página/resultado; se invalidan si cambia la escala o el contenido | Evita renderizados repetidos del mismo contenido |
| Miniaturas e índice PDF | Mantiene elementos al navegar o editar anotaciones; cancela imágenes obsoletas por generación y rotación | Evita reconstruir el modelo y volver a leer el índice sin cambios |
| Miniaturas | Imagen ajustada en ambas dimensiones a 140×190; descarta resultados fuera de la zona solicitada | Hasta 106.400 bytes de píxeles ARGB por miniatura, antes de estructuras Qt |
| Marcadores personales | Compara documento y contenido antes de reconstruir; conserva selección por ID | Evita reconstrucciones en cada cambio de página |
| Cursor sobre texto | Caché LRU de regiones con presupuesto de 2 MiB | Antes retenía regiones sin límite por todas las páginas visitadas; una página excepcional puede usar memoria temporal adicional |
| Exportación | Agrupa referencias por número de página y escribe fragmentos mediante QSaveFile | Evita duplicar entradas y construir simultáneamente el archivo completo en UTF-16 y UTF-8; conserva reemplazo atómico |
| Teclado y menú Ver | Busca el editor de texto una vez por evento; construye la lista de paneles una vez por sincronización | Reduce búsquedas y asignaciones repetidas |

No se cambiaron las reglas de persistencia del historial ni los mutex de PDFium. El historial ya comparte la base y guarda colas incrementales; tiene un límite blando de 128 MiB/30 estados que conserva al menos dos estados. Las cachés de imágenes y búsquedas ya tienen presupuestos de 64, 8 y 16 MiB respectivamente. Estos límites no equivalen a un límite total de RAM: se suman el PDF original, las estructuras de PDFium, las pestañas y los trabajos en curso. El trazado continuo todavía realiza recorridos proporcionales al número de páginas; cualquier rediseño de ese comportamiento requiere perfilado específico con PDFs grandes.

## Idiomas y validación

Revisión de los 256 mensajes del catálogo: textos en español e inglés, tildes, mayúsculas, puntuación, placeholders y nuevas etiquetas. Se normalizó Marcadores personales y se incluyó el subrayado en la descripción del borrador. Se actualizó la ayuda de Enter para incluir la lista de resultados. Se regeneró el catálogo compilado, sin entradas vacías ni pendientes. Las pruebas de idioma comprueban la cobertura de todos los literales gpdfTr y correspondencia del catálogo fuente/compilado.

Las cinco suites CTest aprobaron: núcleo/persistencia, interfaz, corpus, español e inglés. Registro: build-gpdf/validation-navigation-efficiency.log (8,76 s en esta ejecución).

Verificación adicional con capturas offscreen: build-gpdf/validation-navigation-visual.log. En el PDF sintético de dos páginas, 100 cambios de página tardaron 55,36 ms y provocaron 0 reinicios del modelo de miniaturas; editar una anotación tampoco reinició ese modelo. Este dato describe ese ensayo y no constituye una comparación de CPU/RAM global con la versión anterior.

Capturas inspeccionadas: build-gpdf/navigation-review/underline-and-annotations.png y study-tools.png. La prueba de renderizado confirma la separación del subrayado independientemente de la captura de interfaz.

Paquete actualizado: dist/GPDF-0.2.6. Instalador actualizado: dist/installers/GPDF-0.2.6-Setup.exe, con manifiesto SHA-256. La variante TestSigned anterior no se regeneró. Sin commit, tag ni push.
