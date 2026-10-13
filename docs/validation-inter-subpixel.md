# Prueba visual: Inter Medium 10 con subpíxel

13 de octubre de 2026. Variante local de GPDF 0.2.5 solicitada para comparar nitidez.

Se mantienen Inter, 10 puntos y peso Medium. Se utiliza el motor DirectWrite y PreferAntialias, retirando NoSubpixelAntialias. Se conserva el ajuste completo. La selección del motor es local a GPDF y respeta las opciones de plataforma explícitas; no se modifican las preferencias de Windows ni las fuentes instaladas.

Paquete: `dist/GPDF-0.2.5-Inter-Medium-10-Subpixel`. Los paquetes anteriores se conservan para comparar.

La prueba nativa captura propiedades en tema claro y oscuro y una muestra de caracteres normales y en negrita. Informa el número de píxeles con diferencias entre canales RGB para comprobar si el renderizado subpíxel está activo en esta instalación. La prueba de suavizado permite también la configuración de Windows con suavizado gris.

Registros: `build-gpdf/package-inter-subpixel.log` y `build-gpdf/validation-inter-subpixel-native.log`. Capturas: `build-gpdf/verification-inter-subpixel-native`.

Las tres suites CTest y la verificación nativa aprobaron. La muestra nativa registró 2082 píxeles con diferencias entre canales y 7 de suavizado gris, confirmando renderizado subpíxel. Se revisó visualmente la captura de propiedades en tema claro.

No se realizó commit ni push.
