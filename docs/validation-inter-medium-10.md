# Prueba visual: Inter Medium 10

13 de octubre de 2026. Variante local de GPDF 0.2.5 solicitada para comparar la nitidez de la interfaz.

La fuente de interfaz utiliza Inter de 10 puntos con peso Medium. Se conservan el motor GDI, el suavizado en gris y el ajuste completo. Los controles que solicitan negrita siguen pudiendo seleccionar el peso Bold.

Paquete: `dist/GPDF-0.2.5-Inter-Medium-10`. La versión se mantiene en 0.2.5 durante estas pruebas visuales; los paquetes anteriores se conservan para comparar.

Capturas y registro de la verificación nativa: `build-gpdf/verification-inter-medium-10-native` y `build-gpdf/validation-inter-medium-10-native.log`. Registro del empaquetado y CTest: `build-gpdf/package-inter-medium-10.log`.

Las tres suites CTest y la verificación nativa de interfaz aprobaron. Se revisó la captura de propiedades en tema claro. La regresión de suavizado en gris también aprobó.

No se realizó commit ni push.
