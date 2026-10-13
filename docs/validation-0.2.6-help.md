# GPDF 0.2.6: cierre y Ayuda

Cambios locales posteriores al tag v0.2.6, sin nuevo commit ni modificación del tag.

- Texto de recuperación en español neutro.
- Ayuda es el sexto y último menú, tanto en Win32 como en el menú Qt de respaldo.
- Acerca de GPDF abre un diálogo pequeño con texto centrado: Grant's PDF Reader versión 0.2.6 y Creado por Lautaro Grant. La versión procede de VERSION. Se cierra con Cerrar o los controles de ventana.
- Paquete actualizado: dist/GPDF-0.2.6. No se añade OCR.

Las tres suites CTest aprobaron. La prueba nativa comprobó el título y posición del menú y envió WM_COMMAND para verificar que abre el diálogo con el texto y alineación solicitados. Captura: build-gpdf/verification-0.2.6-native/about-gpdf.png.

Corpus de apertura/guardado aprobado: documento de 130 páginas y variante con 64 MiB de relleno. En esta ejecución la variante grande tardó 265 ms en abrir, 177 ms en preparar recuperación y 14 ms en guardar. Los originales conservaron su hash. El relleno comprueba tamaño de entrada, no complejidad gráfica ni comportamiento con cualquier PDF grande.

Impresión física y compatibilidad de edición en Edge/Acrobat continúan pendientes de comprobación manual. Registro: build-gpdf/corpus-0.2.6.log.
