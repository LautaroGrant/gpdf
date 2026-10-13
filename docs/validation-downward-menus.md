# Menús de dibujo hacia abajo

Los desplegables de Lápiz, Resaltado y Subrayado utilizan DownwardToolMenu, que coloca la ventana inmediatamente debajo del botón al mostrarse. La posición horizontal se ajusta al área disponible del monitor. Se mantienen los controles y el funcionamiento de los botones divididos.

La prueba de interfaz abre cada menú solicitando deliberadamente una posición superior y comprueba que termina visible bajo el botón correspondiente. Se ejecuta tanto con Qt offscreen como con Windows nativo.

Paquete local: dist/GPDF-0.2.6-Downward-Menus. Incluye los cambios pendientes anteriores, Ayuda y la corrección de selección en páginas recortadas. Registros: build-gpdf/package-downward-menus.log y build-gpdf/validation-downward-menus-native.log.

No se realiza commit, tag ni push.
