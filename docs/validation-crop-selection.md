# Selección de texto en páginas recortadas

El PDF revisado tiene MediaBox [0, 0, 595.22, 842] y CropBox [59.5276, 161.575, 541.645, 842]. PDFium devuelve coordenadas de texto en el espacio de la página original y renderiza la región recortada. PageView asumía un origen cero para ambos, desplazando la selección y la detección del cursor respecto del texto visible.

La conversión utiliza ahora FPDF_PageToDevice y FPDF_DeviceToPage, con coordenadas normalizadas de alta precisión. Se aplica después la rotación elegida en GPDF; las transformaciones de PDFium también contemplan el recorte y la rotación propia del archivo. Anotaciones y selección conservan sus coordenadas originales en el PDF.

Regresión sintética construida por ui_smoke: dos páginas con recorte desplazado, una con Rotate=90. Comprueba origen visible, ida/vuelta del cursor con las cuatro rotaciones de GPDF y coincidencia de rectángulos de texto con píxeles de glifos renderizados. No incorpora documentos personales al repositorio.

Las tres suites CTest aprobaron. El componente real PageView produjo una captura sobre el PDF revisado con las primeras líneas del resumen seleccionadas correctamente: build-gpdf/text-investigation/selection-fixed.png. Registros: build-gpdf/package-crop-fix.log y build-gpdf/validation-crop-fix-native.log.

Paquete de prueba: dist/GPDF-0.2.6-Crop-Fix. Sin commit ni push; el tag v0.2.6 no cambia. El PDF original conserva SHA-256 737FE4BB2DE265EB229109801BDEE35044ED9D9D8AD124D63403666C47E0AD4D.
