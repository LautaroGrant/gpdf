# Selección por clics — 13 de octubre de 2026

En la herramienta Seleccionar, doble clic selecciona la palabra completa y un tercer clic próximo, dentro del intervalo de doble clic de Windows, selecciona su renglón visual. Una pulsación aislada mantiene la selección por arrastre existente. El doble clic sobre comentarios y cuadros de texto conserva su apertura/edición.

La selección opera sobre índices de caracteres de PDFium, sin alterar el documento. Las palabras incluyen letras Unicode, números, marcas combinadas y apóstrofes internos. La selección de renglón usa orígenes y orientación de caracteres, respeta saltos de línea y evita unir columnas separadas por grandes espacios. En PDFs con orden de texto irregular puede haber diferencias frente a un editor con estructura de párrafos explícita.

Pruebas: selección con acentos, apóstrofes y puntuación; índices inválidos; doble y triple clic con dos renglones independientes, recorte PDF, rotación propia del PDF y las cuatro rotaciones de vista. Se comprobaron las cinco suites del proyecto y se generó el paquete y el instalador. Registro: build-gpdf/validation-word-selection.log.

Artefactos: dist/GPDF-0.2.6 y dist/installers/GPDF-0.2.6-Setup.exe. La variante TestSigned anterior no se regeneró. Sin commit, tag ni push.
