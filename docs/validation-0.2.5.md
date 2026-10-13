# Renderizado de Inter: GPDF 0.2.5

13 de octubre de 2026. Cambios locales sin commit; base Git 50c5ae8. Se conservan las modificaciones de 0.2.4 y la fuente Inter elegida por el usuario.

## Diagnóstico

La pantalla de prueba informa 96 DPI y devicePixelRatio=1, coherente con el escalado del 100 %. Qt resuelve Inter Regular, con tamaño efectivo de 13 píxeles; la cara elegida no tiene tabla fvar, por lo que la prueba no está utilizando la variante variable. Existen caras estáticas OTF y una variante TTF instalada; no se modificó ninguna.

Se compararon cinco políticas: predeterminada, estilo Regular explícito, escala de grises con ajuste completo, vertical y sin ajuste solicitado. Se probaron los motores Windows DirectWrite (predeterminado), GDI y FreeType con los mismos controles, texto y tamaño.

En un recorte idéntico de las etiquetas, se contaron píxeles con diferencia máxima entre canales RGB mayor que 20: DirectWrite conserva 943 píxeles de color tanto con la política predeterminada como con NoSubpixelAntialias y ajuste completo. GDI con esa política produjo 0. FreeType produjo 0, pero resolvió Inter Light en esta instalación, por lo que no fue seleccionado. El ajuste vertical de GDI volvió a producir franjas de color; se conserva el ajuste completo.

Las comparaciones están en build-gpdf/font-probe.png, font-probe-gdi.png y font-probe-freetype.png; el programa diagnóstico permanece en build-gpdf/font_probe.cpp. Son artefactos locales excluidos de Git.

## Implementación

src/UiFont.h configura el motor antes de construir QApplication. Si QT_QPA_PLATFORM no está definido, o es exactamente windows, se utiliza windows:fontengine=gdi. Una plataforma con opciones explícitas se conserva, incluida una selección deliberada de DirectWrite. Offscreen y otras plataformas no cambian.

La fuente mantiene Inter, 10 pt y peso normal, con PreferAntialias, NoSubpixelAntialias y PreferFullHinting. No se fija styleName=Regular para permitir que los controles y el texto enriquecido seleccionen correctamente la cara Bold. Las propiedades globales de Windows/ClearType y las fuentes instaladas permanecen sin cambios.

Las pruebas de interfaz utilizan ahora la misma política de fuente que main.cpp. La regresión nativa dibuja C/S/P, 1/2, porcentajes y texto español, con peso normal y negrita; exige canales iguales y píxeles intermedios en gris para comprobar que no se desactivó el suavizado.

## Fuentes

- Qt 6.8 cambió el motor predeterminado de Windows a DirectWrite y permite seleccionar GDI: https://doc.qt.io/qt-6.8/whatsnew68.html
- Preferencias de suavizado e hinting: https://doc.qt.io/qt-6.8/qfont.html#StyleStrategy-enum y https://doc.qt.io/qt-6.8/qfont.html#HintingPreference-enum

La desaparición de franjas RGB se comprueba en esta instalación; la percepción de contraste y suavidad depende también del monitor. La configuración no sustituye el ajuste del monitor.

## Resultados

- Las tres suites CTest aprobaron durante el empaquetado final.
- annotation_ui aprobó con Windows/GDI, aplicando Inter a toda la aplicación de prueba.
- La regresión de caracteres normales y en negrita encontró 0 píxeles con franjas de color y más de 100 píxeles de suavizado gris.
- Se revisaron visualmente inter-grayscale.png y dark-propertiesAction.png en build-gpdf/verification-0.2.5-native.
- Paquete dist/GPDF-0.2.5: versión 0.2.5, workingTreeDirty=true, base 50c5ae8. Los hashes de ejecutable y README coinciden con build y fuente.
- No se hizo commit, tag ni push. Los cambios anteriores siguen presentes.
