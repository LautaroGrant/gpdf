# Validación de GPDF 0.2.3

Fecha: 12 de octubre de 2026. Windows x64, Qt 6.8.3 y PDFium chromium/8086.

## Cambios verificados

- Traslado de comentarios dentro de la página y entre páginas: conserva contenido Unicode, saltos de línea y color; actualiza los índices de anotaciones; deshacer y rehacer representan un solo cambio. Guardado y reapertura mantienen la página de destino. Destinos inválidos y comentarios bloqueados no modifican el documento ni su historial.
- Arrastre con Seleccionar en dos páginas y vista continua, incluida la activación de otra página durante el desplazamiento. El panel del comentario se oculta al iniciar el movimiento. El borde de la vista desplaza el documento; en vista individual/doble avanza al llegar al extremo. Esc o desactivar la ventana cancela el arrastre.
- Clic central sobre una pestaña cierra el documento. Se conserva el aviso de cambios sin guardar y Cancelar mantiene la pestaña.
- Ctrl+B abre Búsqueda. El antiguo valor predeterminado Ctrl+F se migra una sola vez, sin sustituir combinaciones personalizadas. Restaurar predeterminados utiliza Ctrl+B.
- Campo de página con límites del documento, navegación al escribir y confirmar con Enter, sincronización de la página activa y funcionamiento en las tres vistas.

## Ejecución

Las suites pdf_roundtrip, annotation_ui y pdf_corpus se ejecutan mediante scripts/build.ps1. La interfaz se verifica también con QT_QPA_PLATFORM=windows, sin impresión física. Los registros y capturas permanecen en build-gpdf, excluido de Git.

El proyecto y la distribución permanecen locales; no se configura un remoto ni se realiza push.

## Resultados

- Las tres suites CTest aprobaron (pdf_roundtrip, annotation_ui y pdf_corpus).
- annotation_ui aprobó con Qt Windows, incluido el cierre cancelado de una pestaña inactiva con cambios sin guardar.
- Se revisó visualmente la captura de dos páginas: el campo editable y el total se ven completos junto al zoom.
- Registros: build-gpdf/validation-0.2.3.log y build-gpdf/validation-0.2.3-native.log. Capturas: build-gpdf/verification-0.2.3-native.
