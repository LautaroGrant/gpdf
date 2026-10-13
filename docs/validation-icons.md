# Prueba de iconos y fuente original

13 de octubre de 2026. Build local `dist/GPDF-0.2.5-Icons`, sin commit ni push.

Las dos imágenes proporcionadas se conservan en `resources/icons`. Se generaron ICO transparentes con tamaños 16, 24, 32, 48, 64, 128 y 256. El ejecutable incluye dos grupos: aplicación en índice 0 y documento en índice 1. Qt utiliza el icono de aplicación en sus ventanas.

La interfaz vuelve a Segoe UI de 10 puntos y al motor predeterminado de Qt.

En un arranque normal, GPDF registra en HKCU su ProgID, comando de apertura, icono de documentos, capacidades y disponibilidad en Abrir con. No escribe UserChoice ni cambia el valor predeterminado de .pdf. Las capturas con --snapshot omiten este registro. El usuario debe ejecutar el paquete y elegir GPDF en las aplicaciones predeterminadas de Windows. Si se mueve el paquete, volver a ejecutarlo actualiza las rutas.

Las tres suites CTest aprobaron. ExtractIconEx de Windows informó dos grupos y extrajo correctamente los iconos de aplicación y documento del ejecutable empaquetado. Las capturas están en `build-gpdf/verification-icons`. La elección del programa predeterminado y el resultado en Explorer quedan para la prueba del usuario; no se cambiaron asociaciones durante la validación.

Referencias: https://learn.microsoft.com/en-us/windows/win32/shell/default-programs y https://doc.qt.io/qt-6/qsettings.html#accessing-common-registry-settings-on-windows.
