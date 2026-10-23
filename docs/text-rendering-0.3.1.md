# Texto en pantalla: comparación de suavizado

23 de octubre de 2026. Compilación local 0.3.1, PDFium chromium/8086.

Se comparó la página 4 del PDF de Marechal del escritorio a idénticas dimensiones de 1.175 × 1.521 píxeles. La variante original empleaba un bitmap BGRA y suavizado de texto en grises. Añadir solamente `FPDF_LCD_TEXT` no cambió ningún píxel. Usar bitmap opaco BGRx y `FPDF_LCD_TEXT` sí cambió la cobertura de los caracteres; en la comparación visual produjo bordes más finos. Ambas variantes usan cuatro bytes por píxel: no se aumentó la resolución ni el almacenamiento de la imagen.

La vista normal ahora solicita esta variante opaca LCD, solamente cuando la página no está rotada y el raster coincide con sus dimensiones físicas de pantalla. Cuando el raster se reduce por el presupuesto de memoria, o la página se rota, se conserva el suavizado anterior para no reescalar o rotar la cobertura RGB. OCR, impresión y llamadas de renderizado generales conservan el valor predeterminado anterior.

La página 11 del PDF de Parerga también se comparó. En esa página, el cuerpo principal está almacenado como imagen: el ajuste LCD solamente cambió parte del texto digital del pie. `FPDF_RENDER_FORCEHALFTONE` no produjo diferencias en esa prueba. Desactivar el suavizado de imágenes produjo contornos más dentados y no se incorporó como configuración general. La mejora de texto vectorial no debe presentarse como solución completa para texto almacenado en imágenes.

Las pruebas de renderizado verifican coincidencia exacta entre la página dibujada y un raster independiente a escala de pantalla 1, 1.25, 1.5 y 2, con cuatro rotaciones. También se comprobó la igualdad de tamaño y almacenamiento entre las variantes LCD y en grises, y que el LCD realmente altere la cobertura del texto. Una prueba adicional sobre las dos primeras páginas de Marechal a escala 1.5 pasó en las cuatro rotaciones.

La prueba de selección de texto compara ahora la selección con y sin un resultado de búsqueda superpuesto. Antes interpretaba cualquier píxel amarillo como resaltado de búsqueda, pero los bordes de glifos LCD pueden contener subpíxeles amarillos. Comparar la imagen completa con su referencia de selección comprueba directamente que no aparezca el resaltado indebido.

Artefactos locales: `build-release-031/text-quality/`, incluyendo `compare.py`, imágenes de las variantes, `reading-comparison.png` y la comparación separada de Parerga. La copia para evaluación se entrega en `dist/GPDF-0.3.1-text-quality/`; la copia anterior estaba abierta y no se reemplazó. La nitidez percibida debe evaluarse en la pantalla física del usuario: estas pruebas de píxeles no garantizan una preferencia visual universal ni reproducen todos los tipos de panel.
