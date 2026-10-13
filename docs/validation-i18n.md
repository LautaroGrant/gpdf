# Interfaz bilingüe, 0.2.6

Trabajo local en `feature/i18n`, sin commit, etiqueta ni push. La versión sigue siendo 0.2.6. Los cambios anteriores del instalador y firma de pruebas se conservan.

## Implementación

- Un mismo ejecutable ofrece español e inglés; no hay dos implementaciones del lector ni ramas permanentes por idioma.
- Español es el idioma fuente. `src/Translations.h` expone `gpdfTr()` con contexto `GPDF`. El catálogo `resources/translations/gpdf_en.ts` contiene 255 mensajes y el `.qm` compilado ocupa 25.469 bytes.
- Se traducen menús Qt y Win32, herramientas, cuadros de diálogo, errores, propiedades y encabezados generados de las exportaciones. No se traducen texto del PDF, metadatos originales, comentarios del usuario, claves de configuración ni identificadores internos.
- Solo se carga el catálogo del idioma activo al iniciar. Español carga además el catálogo existente de controles Qt. No se agregan bibliotecas al paquete ni trabajo al renderizado, búsqueda, cachés o desplazamiento de páginas.
- Ayuda → Idioma permite elegir Español o English. La preferencia `ui/language` se guarda en la configuración existente de StudyPDF y se aplica al siguiente inicio. La interfaz activa mantiene su idioma hasta cerrar.
- El instalador muestra la elección de idioma. Al terminar ejecuta `--initial-language=es` o `--initial-language=en` junto con `--initialize-language-only`, incluso si el usuario no marca abrir la aplicación. Ese modo guarda la preferencia inicial y termina antes de crear la ventana, registrar asociaciones o comunicarse con otra instancia. Si ya existe una elección, la conserva.
- El paquete portable conserva español como idioma inicial. Los atajos predeterminados se adaptan al idioma de interfaz: en inglés, Ctrl+S/Ctrl+Shift+S para guardar, Ctrl+H/Ctrl+Shift+H para resaltar, Ctrl+F para búsqueda y Ctrl+G para ir a página. En español se conservan Ctrl+G/Ctrl+Shift+G, Ctrl+S/Ctrl+Shift+S, Ctrl+B y Ctrl+I respectivamente. Ambos idiomas utilizan Ctrl+N para anotaciones y Ctrl+U para subrayar. Se conservan las personalizaciones y los valores predeterminados se adaptan al cambiar de idioma y reiniciar. La migración inicial solo sustituye las asignaciones conocidas de versiones antiguas y completa valores ausentes.
- El README principal está en inglés y `README.es.md` en español. Ambos se incluyen en el paquete. Para editar traducciones, modificar el `.ts` y ejecutar `scripts/update-translations.ps1`; incluir `.ts` y `.qm` juntos cuando se autorice un commit.

## Comprobaciones

Las cinco suites de CTest pasaron: persistencia PDF, interfaz, corpus PDF, español e inglés. La verificación nativa de Windows pasó para la interfaz y ambos idiomas.

Las pruebas de idiomas comprueban menús, controles, Acerca de GPDF, exportación Markdown/TXT/HTML, conservación de comentarios, metadato de idioma HTML, catálogo fuente/compilado, placeholders y cobertura de todos los literales `gpdfTr()`. Comprueban también atajos personalizados, elección del idioma desde el menú, aplicación al reiniciar en otro proceso y conservación de una preferencia anterior durante la inicialización del instalador.

Las pruebas Win32 comprueban los seis menús y su traducción y activan la opción de idioma mediante `WM_COMMAND`. Las preferencias de pruebas se guardan en un directorio temporal, sin tocar las personales.

El instalador se verifica en un entorno aislado: instalación, actualización, hashes, comando e icono PDF, idioma de registro, ejecución, desinstalación, conservación de archivos ajenos y del lector PDF predeterminado. El instalador de prueba omite la escritura de la preferencia personal; esa lógica se verifica con las pruebas de idiomas.

## Rendimiento

Comparación local contra `b6bfc8f` con el mismo compilador Release, Qt y PDFium. PDF sintético de 100 páginas; 30 páginas renderizadas al 125 % y búsqueda de una palabra en las 100 páginas. Dos ejecuciones de calentamiento y mediana de siete muestras por proceso. Orden antes/después/después/antes para comprobar variación:

| Ejecución | Apertura (ms) | Render de 30 páginas (ms) | Búsqueda de 100 páginas (ms) |
|---|---:|---:|---:|
| Antes, 1 | 0,908 | 39,450 | 22,338 |
| Después, 1 | 0,801 | 38,314 | 23,583 |
| Después, 2 | 0,743 | 36,986 | 23,281 |
| Antes, 2 | 0,714 | 37,231 | 23,042 |

Los tiempos están en el mismo rango; la búsqueda varía alrededor de 1 ms. Las funciones de renderizado y búsqueda no tienen cambios de algoritmo ni consultas de traducción. Esta comparación no garantiza tiempos idénticos en todos los equipos o PDFs y no mide el arranque completo del proceso.

Antes del cambio, las tres suites existentes terminaron en 8,31 s. En la primera verificación bilingüe, esas mismas suites tomaron aproximadamente 7,56 s; con las dos suites nuevas, el total fue 8,34 s. Son mediciones locales, no un benchmark universal.

Los probes, logs y capturas están en `build-gpdf`, excluidos de Git. Las herramientas Qt Linguist solo amplían el entorno de desarrollo; no se distribuyen.

La verificación final de las cinco suites terminó en 8,72 s, todas aprobadas. El paquete portable contiene 81 archivos y ocupa 46.368.785 bytes (46,37 MB / 44,22 MiB). El ejecutable ocupa 1.293.205 bytes: 50.711 bytes más que el build anterior de menús desplegables. El instalador bilingüe con firma de pruebas ocupa 16.439.512 bytes (16,44 MB).

Las comprobaciones nativas de ambos idiomas y las pruebas del instalador en español y en inglés con firma de pruebas pasaron. Las firmas del ejecutable, instalador y desinstalador usan el certificado de pruebas ya creado; no se creó otro certificado ni se alteró la confianza de Windows.
