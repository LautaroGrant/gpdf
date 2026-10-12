# Diálogo de impresión de Windows

Study PDF usa QPrintDialog de Qt 6.8.3, que llama a PrintDlgEx. Windows elige la interfaz nativa según su versión y configuración. No se fuerza el diálogo clásico ni se cambian preferencias del registro del sistema.

En esta máquina se observó PrintDialog.exe, alojado en Windows.PrintDialog_cw5n1h2txyewy, separado del proceso StudyPDF. La antigua prueba de cancelación solo enumeraba ventanas del proceso de la prueba y por eso no detectaba la interfaz moderna. Esa prueba continúa omitida por pedido del usuario.

Se protegió la impresión y vista previa contra llamadas simultáneas mediante una bandera restaurada automáticamente al salir. El tratamiento de títulos oscuros excluye QPrintDialog y ventanas con WA_DontShowOnScreen, para evitar crear un HWND para el contenedor invisible que Qt utiliza para el diálogo nativo.

Las comprobaciones de regresión cubren la segunda llamada mientras hay una vista previa activa y que el tratamiento de tema no materialice contenedores invisibles. Esto no demuestra todavía la causa exacta de la aparición conjunta del diálogo clásico y moderno en la captura del usuario; hace falta comprobar ese caso visualmente con la nueva versión.

Fuentes:
- Qt 6.8.3: https://github.com/qt/qtbase/blob/v6.8.3/src/printsupport/dialogs/qprintdialog_win.cpp
- Microsoft: https://learn.microsoft.com/en-us/windows/win32/printdocs/retrieving-a-printer-device-context
