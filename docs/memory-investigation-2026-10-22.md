# Memoria al abrir Parerga y Paralipómena

Pruebas del 22 de octubre de 2026, Windows, código de trabajo en commit `0bb28cd2bac191c71b282a60e6ee11d51efac362`, Release, Qt 6.8.3, PDFium chromium/8086.

## Resultado

El principal disparador es el recorrido automático de comprobación de texto para OCR. Al abrir, `Window::processDocumentOcr()` procesa hasta ocho páginas por llamada del temporizador de 25 ms. Para comprobar si cada página necesita OCR, llama a `PdfDocument::pageText()`, que carga tanto la página como su página de texto de PDFium. Aunque se cierran esos manejadores, el proceso conserva mucha memoria hasta cerrar el documento.

Desactivar solamente ese temporizador en la herramienta de diagnóstico reduce la memoria residente de apertura estabilizada a unos 121 MiB. Con él activo, el escenario completo termina en unos 843 MiB residentes y 907 MiB privados. La carga de páginas de texto, aislada de Qt y del servicio de reconocimiento OCR, reproduce por sí sola unos 760 MiB. No es necesario ejecutar reconocimiento de imágenes para producir el aumento.

## Archivo y método

- Original del escritorio: `Arthur Schopenhauer, Parerga y paralipómena l.pdf`, 80.131.374 bytes (76,42 MiB), 495 páginas, cero anotaciones.
- SHA-256 antes y después: `9dc2d8dd4ee1a1049f2e44ab95c36475a77459538231a239f17a330ea4800df9`. No se escribió el original.
- Métricas: `GetProcessMemoryInfo`, WorkingSetSize (residente), PrivateUsage (privada), PeakWorkingSetSize (máximo residente). MiB = 1.048.576 bytes. Las métricas incluyen el proceso completo; las herramientas Python y Qt tienen bases diferentes.
- Prueba aislada: Python ctypes con la DLL del proyecto, procesos nuevos por escenario. Carga en memoria, primera página y texto, tamaños de las 495 páginas; luego renderizado o recorrido de páginas. Se cierran bitmaps, páginas y páginas de texto. Renderizado de 1.200 píxeles de ancho.
- Aplicación: herramienta C++ compilada con las fuentes actuales, `Window`, ajustes de usuario aislados en directorio temporal y plataforma Qt offscreen. Mismo escenario con y sin el temporizador de OCR. Apertura, espera de tres segundos, panel de anotaciones, navegación de treinta páginas y espera final. La sesión real del usuario no se restaura.

## Resultados aislados

| Operación | Residente MiB | Pico residente MiB |
|---|---:|---:|
| Proceso Python inicial | 15,7 | 15,7 |
| Archivo leído | 93,5 | 93,5 |
| Documento, primera página/texto y tamaños | 99,5–99,7 | 99,5–99,7 |
| Renderizar 30 páginas | 121,4 | 123,1 |
| Renderizar las 495 páginas y liberar bitmaps | 208,6 | 219,4 |
| Recorrer anotaciones de las 495 páginas | 202,8 | 202,8 |
| Cargar texto de las 495 páginas | 759,8 | 759,8 |
| Después de cerrar documento, escenario de texto | 22,2 | 759,8 |

Repetición independiente: anotaciones 202,9 MiB; texto 759,5 MiB. El recorrido de anotaciones tardó aproximadamente 1,3–1,5 segundos y el de texto 3,4–3,7 segundos, sin contar cierre. El indicador `FPDF_RENDER_LIMITEDIMAGECACHE` no redujo el pico de renderizado de treinta páginas: ambos escenarios alcanzaron 123,1 MiB.

## Aplicación completa

| Etapa | OCR automático activo: residente / privada MiB | Temporizador OCR detenido: residente / privada MiB |
|---|---:|---:|
| Ventana vacía | 30,9 / 10,1 | 31,0 / 10,2 |
| Apertura síncrona terminada | 113,7 / 144,0 | 113,8 / 144,0 |
| Después de tres segundos | 475,3 / 523,1 | 120,8 / 150,5 |
| Panel de anotaciones abierto, después de espera | 781,4 / 845,4 | 224,5 / 254,5 |
| Navegación de treinta páginas y espera | 842,9 / 906,9 | 291,5 / 321,9 |

Con OCR activo, otros diez segundos sin interacción mantuvieron 842,9 MiB. Al destruir la ventana bajó a 32,4 MiB residentes y 10,8 MiB privados. Los resultados de las etapas no se pueden restar para asignar costos exactos: con OCR activo el recorrido sigue avanzando durante las esperas y otras operaciones. El control sin OCR y la prueba aislada permiten identificar su contribución principal.

## Interpretación y límites

La copia completa del archivo cuesta aproximadamente 76 MiB, y el panel de anotaciones añade alrededor de 104 MiB en este caso. Sin embargo, el recorrido anticipado de texto explica la mayor parte del salto observado. La prueba aislada de texto no conserva todas las cadenas extraídas: basta cargar y cerrar cada página de texto para reproducirlo. Esto sitúa la retención en el procesamiento interno de PDFium y sus recursos asociados; no identifica las estructuras internas concretas responsables. La liberación al cerrar indica retención ligada a la vida del documento, sin evidencia en estas pruebas de una fuga que persista tras su cierre.

No se midió Okular. No se perfiló el ejecutable instalado: su SHA-256 difiere del ejecutable de distribución 0.3.0 y del antiguo `build-gpdf/GPDF.exe`. El problema sí se reprodujo compilando las fuentes actuales. Qt offscreen no reproduce exactamente el costo gráfico de una ventana nativa ni todas las métricas mostradas por el Administrador de tareas. No se atribuye un pico exacto de 1 GB a todas las aperturas.

La siguiente corrección debería evitar el recorrido anticipado completo para OCR al abrir, priorizando páginas necesarias y manteniendo el reconocimiento disponible bajo demanda. También conviene reducir el recorrido completo de anotaciones. No se hicieron cambios al comportamiento del producto ni a la instalación durante este diagnóstico.

## Artefactos

Herramientas y registros en `build-gpdf/memory-investigation/`: `probe.py`, `probe.cpp`, `CMakeLists.txt`, `annotations.jsonl`, `annotations-repeat.jsonl`, `render.jsonl`, `render-all.jsonl`, `limited.jsonl`, `text.jsonl`, `text-repeat.jsonl`, `ui.txt`, `ui-no-ocr.txt`. Ese directorio de compilación está ignorado por Git.

Para la prueba aislada: `python build-gpdf/memory-investigation/probe.py annotations`, `... text`, `... render`, `... render all`, `... limited`. La herramienta localiza el libro por el patrón `*Parerga*` en el escritorio. La herramienta C++ recibe la ruta Unicode como primer argumento; `--no-ocr` detiene exclusivamente el temporizador `ocrTimer`. Sus dependencias se compilan mediante el CMake local y requiere las DLL de Qt/PDFium y el plugin offscreen.

## Verificación de la implementación 0.3.1

La nueva política revisa hasta cinco páginas inicialmente. Si detecta imágenes sin una capa de texto suficiente, inicia OCR automáticamente hasta 15 páginas; en documentos mayores presenta una barra compacta de confirmación. Las páginas vacías no requieren OCR. Los documentos mixtos pueden activar la oferta al visitar una página escaneada fuera de la muestra. “Ahora no” y cancelar detienen ese documento sin reiniciar automáticamente la oferta en la misma apertura. El progreso y la decisión se mantienen por pestaña.

La detección y el renderizado para OCR se realizan con documentos temporales de PDFium, compartiendo el buffer original, que se cierran después de cada página. La detección contempla imágenes dentro de objetos Form y considera insuficiente una capa con menos de veinte caracteres alfanuméricos, para cubrir páginas escaneadas que solamente tienen numeración digital. Es una heurística: no mide la calidad semántica de una capa OCR existente. No se modifican los bytes del PDF por reconocer texto.

Se repitió el escenario completo de la herramienta Qt con OCR automático habilitado (`ui-031.txt`):

| Etapa | Residente MiB | Privada MiB |
|---|---:|---:|
| Ventana vacía | 31,0 | 10,1 |
| Apertura síncrona | 113,8 | 144,1 |
| Tres segundos después de abrir | 121,5 | 151,3 |
| Panel de anotaciones y espera | 224,7 | 254,6 |
| Navegación de treinta páginas y espera | 296,4 | 327,0 |
| Otros diez segundos sin interacción | 296,4 | 327,0 |
| Ventana destruida | 31,0 | 8,7 |

En el mismo escenario que antes alcanzaba 842,9 MiB residentes, ahora se alcanzan 296,4 MiB (reducción aproximada del 65%). La apertura estabilizada queda en 121,5 MiB. El costo del panel de anotaciones y las cachés de navegación sigue presente: no fue objeto de esta modificación. Tampoco se cambió la búsqueda nativa completa, que puede necesitar procesar todas las páginas cuando el usuario la solicita.

Validación final: las trece pruebas CTest pasaron. `ocr_smoke --native` también pasó, incluyendo reconocimiento real de Windows, caché, cancelación de un proceso de reconocimiento, OCR de páginas no visitadas en un documento mixto corto y búsqueda posterior. Se verificaron capturas de la barra compacta de oferta y progreso en `build-release-031/ocr-previews/`. Para las pruebas se usó TEMP/TMP dentro de `build-release-031/test-tmp`; las integraciones de comunicación entre instancias y OCR nativo requirieron ejecución fuera del sandbox por restricciones de acceso de Windows.

La distribución portable se encuentra en `dist/GPDF-0.3.1/`, con manifiesto de compilación, dependencias y licencias. No se reemplazó la instalación del usuario. VERSION y la compilación indican 0.3.1; los README y el changelog permanecieron sin cambios y no se creó ningún commit. El SHA-256 del PDF original siguió coincidiendo después de las pruebas.
