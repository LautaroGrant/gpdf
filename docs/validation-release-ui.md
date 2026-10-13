# Ajustes previos a release — 13 de octubre de 2026

- Pestañas: ancho máximo de 240 píxeles lógicos, elipsis a la derecha, desplazamiento entre pestañas y ruta completa en el tooltip.
- Menú Ver: Búsqueda refleja el panel y su botón; Miniaturas y marcadores reflejan visibilidad y pestaña de navegación activa. El menú nativo de Windows sincroniza las marcas al abrirse.
- Inglés: guardar Ctrl+S, guardar como Ctrl+Shift+S, resaltado primario Ctrl+H, secundario Ctrl+Shift+H, búsqueda Ctrl+F e ir a página Ctrl+G.
- Ambos idiomas: anotaciones Ctrl+N y subrayado Ctrl+U. Los valores predeterminados anteriores se migran; las asignaciones personalizadas se conservan. Cambiar el idioma adapta los valores predeterminados al reiniciar.

Validación: las cinco suites CTest aprobaron (núcleo/persistencia, interfaz, corpus, español e inglés). Las regresiones comprueban ancho de pestañas, tooltip, paneles, atajos por idioma, migración desde Ctrl+A y cambio de idioma en un proceso nuevo conservando personalizaciones. Registro: build-gpdf/validation-release-ui.log.

Paquete recompilado: dist/GPDF-0.2.6. Instalador actualizado: dist/installers/GPDF-0.2.6-Setup.exe. La variante TestSigned anterior no se regeneró. Sin commit, tag ni push; se conserva la versión 0.2.6 pendiente de preparar la release.
