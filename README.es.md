# GPDF 0.2.7

[English](README.md)

GPDF (antes StudyPDF) es un lector de PDF para estudiar en Windows x64, construido con C++20, Qt Widgets y PDFium. Estado: beta local funcional.

## Instalar y ejecutar

Para utilizar GPDF, obtener el instalador o el paquete portable completo de la distribución de la versión. Las rutas `dist/` indicadas abajo son resultados de una compilación local; los binarios no están incluidos en el repositorio de código fuente.

1. Ejecutar `GPDF-0.2.7-Setup.exe` en Windows x64.
2. Elegir English o Español y seleccionar la carpeta de instalación y los accesos directos.
3. Finalizar la instalación y abrir GPDF desde el menú Inicio.

Para utilizar el portable, extraer el paquete completo en una carpeta y ejecutar `GPDF.exe` dentro de ella. No se necesitan herramientas de desarrollo.

El paquete portable vigente es `dist/GPDF-0.2.7/GPDF.exe`. La carpeta completa debe mantenerse junto al ejecutable; no hace falta instalar Qt. `build-info.json` identifica versión, commit de base, estado del código y hashes de los binarios.

El instalador de Windows está en `dist/installers/GPDF-0.2.7-Setup.exe`. Instala para el usuario actual, sin solicitar permisos de administrador, en `%LOCALAPPDATA%\Programs\GPDF` por defecto. Permite elegir otra carpeta durante la instalación y las actualizaciones. Incluye acceso en Inicio, acceso opcional en el escritorio y desinstalador. Registra GPDF como opción para abrir PDF, pero conserva el programa predeterminado elegido por el usuario. Para desinstalarlo, utilizar Aplicaciones instaladas de Windows; las preferencias y copias de recuperación se conservan.

Se puede generar además una variante de pruebas firmada con un certificado autofirmado a nombre de Lautaro Grant: `dist/installers/GPDF-0.2.7-Setup-TestSigned.exe`. Incluye firmas del ejecutable, instalador y desinstalador. Esta firma no es confiable automáticamente para Windows y no sustituye un certificado validado para distribución pública; detalles en `docs/test-signing.md`.

Las actualizaciones proponen la carpeta de la instalación anterior; se puede cambiar desde el instalador. La compilación firmada de prueba anterior se eliminó durante la limpieza. Utilizar el último `Setup.exe` para esta versión; la guía de firma de pruebas explica cómo generar una nueva variante firmada.

Abrir un PDF desde Inicio o Archivo. Abrir otro archivo agrega una pestaña. Windows permite asociar PDFs mediante «Abrir con». La aplicación reutiliza la instancia abierta y recupera su estado maximizado o normal al abrir PDFs desde el escritorio.

El instalador comienza con English seleccionado por defecto, incluso en Windows en español y al actualizar. Permite elegir Español al comenzar. Esa elección se guarda como idioma inicial del programa aunque no se marque abrir GPDF al finalizar. Una actualización conserva el idioma previamente guardado. El paquete portable comienza en español. El menú Ayuda → Idioma permite cambiarlo para el siguiente inicio. Ambos idiomas comparten el mismo ejecutable, motor PDF, cachés y atajos; las traducciones no modifican los textos del documento ni los comentarios del usuario.

## Funciones

- Vista continua, página individual y dos páginas; zoom, rotación y pantalla completa. El indicador de página permite escribir el número de destino y confirmar con Enter.
- Doble clic selecciona una palabra; triple clic selecciona su renglón visual. Selección y copia de texto; búsqueda sin distinguir mayúsculas ni acentos. Enter en el campo o sobre un resultado seleccionado avanza a la siguiente coincidencia.
- Comentarios, cuadros de texto, resaltados, subrayados y lápiz; edición, color, borrado y deshacer/rehacer. Crear un comentario o hacer clic en su icono con Seleccionar o Comentario abre su contenido sobre la página; permite copiar texto y se cierra con ×, Esc o un clic fuera.
- Pestañas, miniaturas, índice del PDF, marcadores personales y progreso de lectura. Las miniaturas admiten navegación con mouse y flechas. Los marcadores personales conservan la selección al navegar para poder renombrarlos o borrarlos. El clic central sobre una pestaña cierra ese documento, con aviso si tiene cambios sin guardar.
- Recuperación de cambios y restauración de sesión. Las copias de recuperación no reemplazan el original.
- Exportación de anotaciones a Markdown, texto o HTML; impresión y vista previa.
- Interfaz oscura con páginas blancas y atajos personalizables.

Con Seleccionar, arrastrar el icono de un comentario para moverlo dentro de la página o a otra página. Acercarlo al borde superior o inferior de la vista permite desplazarse durante el arrastre; Esc cancela. El traslado conserva el contenido y permite deshacerlo en un solo paso.

Las anotaciones se guardan dentro del PDF. Los marcadores personales y el progreso son locales y se identifican por ruta. El cambio de nombre conserva la ubicación de configuración de StudyPDF para mantener preferencias, marcadores y recuperación. Guardar como adopta el nuevo archivo como documento actual.

Los iconos de lápiz, resaltado y subrayado muestran el color seleccionado. Seleccionar texto y pulsar Subrayar; su menú desplegable permite elegir color y grosor (0,5–12 pt). Los subrayados nuevos dejan una separación de 0,75 pt debajo del texto. La lista de anotaciones muestra página y tipo en negrita. El arranque restaura la ventana antes de mostrarla y evita abrirla minimizada.

Los menús de colores de Lápiz, Resaltado y Subrayado se despliegan debajo de sus botones. La selección y la detección del cursor contemplan el recorte interno de las páginas y su rotación, evitando desplazamientos entre las letras y la marca de selección. Las pruebas incluyen páginas con recorte desplazado y las cuatro rotaciones de la vista.

Los resaltados nuevos no incluyen texto de comentario ni una anotación de comentario adicional. El fragmento seleccionado se obtiene del PDF para la lista y la exportación. Los lectores externos pueden incluir resaltados en su panel general de anotaciones; los comentarios ya existentes se conservan.

## Atajos predeterminados

La tabla corresponde a español. En inglés: guardar con Ctrl+S/Ctrl+Shift+S, resaltar con Ctrl+H/Ctrl+Shift+H, búsqueda con Ctrl+F e ir a página con Ctrl+G. Anotaciones utiliza Ctrl+N y subrayar Ctrl+U en ambos idiomas. Los valores predeterminados siguen el idioma de interfaz al reiniciar; las personalizaciones se conservan.

| Acción | Atajo |
|---|---|
| Seleccionar / Comentario / Texto / Lápiz | F1 / F2 / F3 / F4 |
| Resaltar con color primario / secundario | Ctrl+S / Ctrl+Shift+S |
| Guardar / Guardar como | Ctrl+G / Ctrl+Shift+G |
| Buscar / Ir a página | Ctrl+B / Ctrl+I |
| Mostrar anotaciones / Seleccionar todo | Ctrl+N / Ctrl+Shift+A |
| Subrayar | Ctrl+U |
| Agregar marcador personal | Ctrl+D |
| Siguiente pestaña / Anterior | Ctrl+Tab / Ctrl+Shift+Tab |
| Pantalla completa | F11; Esc para salir |
| Zoom con rueda (25 puntos porcentuales) | Ctrl+rueda |
| Zoom con botones (10 puntos porcentuales) | + / − en la barra |

Un clic con el botón central del mouse activa el desplazamiento automático. El cursor nativo de Windows cambia a arriba o abajo al mover el mouse; no se crea un indicador fijo. Otro clic o Esc lo detiene.

Suprimir borra una anotación seleccionada; dentro de un campo de texto borra caracteres. Enter confirma comentarios y textos; Shift+Enter agrega una línea. El menú Atajos permite modificar estas combinaciones.

## Compilar y verificar

Requisitos: Windows x64, PowerShell, Python 3.11+ disponible mediante `python` y conexión a Internet para descargar las dependencias inicialmente. El comando `tar` de Windows debe estar disponible en PATH. No hace falta instalar Visual Studio ni Qt por separado: bootstrap instala MinGW, Qt, CMake, Ninja y PDFium dentro del proyecto.

Descargar y extraer el código fuente, o clonar el repositorio, y abrir PowerShell en la raíz del proyecto (la carpeta que contiene `CMakeLists.txt`). Instalar las dependencias de desarrollo una vez:

```powershell
./scripts/bootstrap.ps1
```

Compilar en modo Release y ejecutar las pruebas automatizadas:

```powershell
./scripts/build.ps1 -BuildDirectory build-gpdf
```

El ejecutable queda en `build-gpdf/GPDF.exe`. Para compilar y preparar una distribución portable con bibliotecas y licencias:

```powershell
./scripts/build.ps1 -BuildDirectory build-gpdf -Package -PackageName GPDF-0.2.7
```

El resultado queda en `dist/GPDF-0.2.7/`. Después de preparar ese paquete, crear el instalador y verificar instalación, actualización y desinstalación:

```powershell
./scripts/build-installer.ps1 -SkipBuild
./scripts/validate-installer.ps1
./scripts/validate-installer.ps1 -InstallerLanguage spanish
```

El instalador queda en `dist/installers/GPDF-0.2.7-Setup.exe`. Como alternativa, `./scripts/build-installer.ps1` sin `-SkipBuild` compila, ejecuta las pruebas y prepara el paquete antes de generar el instalador. El nombre del instalador sigue el archivo `VERSION`; al cambiar la versión, actualizar también el nombre del paquete portable.

Si PowerShell bloquea los scripts locales, habilitarlos para esa sesión de terminal con `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass` y repetir el comando.

El script prepara y verifica el paquete, y descarga Inno Setup 6.7.3 en modo portable dentro de `.tools` si falta. Comprueba el hash y la firma del compilador antes de ejecutarlo. La prueba utiliza una rama aislada del registro y omite accesos directos para no alterar la asociación PDF ni una instalación existente.

Las dependencias se instalan en el proyecto. La compilación ejecuta cinco suites: núcleo/persistencia, interfaz, corpus PDF, español e inglés. Las pruebas de interfaz usan Qt offscreen y necesitan acceso a archivos temporales e IPC local de Windows. Las pruebas de idiomas verifican menús, herramientas, Acerca de GPDF, exportaciones, atajos personalizados, catálogo compilado y persistencia al reiniciar.

Para verificar archivos propios sin modificar los originales:

```powershell
./scripts/validate-corpus.ps1 -PdfPaths 'C:/ruta/libro.pdf','C:/ruta/apuntes.pdf'
./scripts/validate-corpus.ps1 -Stress -OutputDirectory './build/corpus-output'
# Si Poppler está instalado y en PATH:
./scripts/validate-corpus.ps1 -Poppler -PdfPaths 'C:/ruta/libro.pdf'
```

Sin rutas se generan un documento de 130 páginas y un PDF de imágenes sin texto. `-Stress` agrega un caso con 64 MiB de relleno: comprueba tamaño de entrada, no complejidad gráfica. `-OutputDirectory` conserva copias en una subcarpeta única; de lo contrario se eliminan los temporales.

## Límites actuales

No incluye OCR: un escaneo sin capa de texto se puede leer y anotar, pero no buscar ni resaltar palabras. Los PDFs se cargan completos en memoria. Apertura y guardado siguen siendo operaciones síncronas; se redujo el alcance del bloqueo de PDFium durante la lectura, recuperación y confirmación del guardado, pero documentos grandes pueden generar pausas. El historial tiene límites de entradas y memoria.

La compatibilidad de edición con Edge/Acrobat y la impresión física requieren validación manual. Las suites automatizadas no cubren todos los PDFs posibles. La separación de componentes es gradual: la ventana aún concentra coordinación de sesión, búsqueda y herramientas.

## Desarrollo y distribución

La versión 0.2.7 utiliza Segoe UI de 10 pt y el motor de fuentes predeterminado de Qt. Incluye los iconos de aplicación y documento proporcionados por el usuario. Al ejecutar GPDF, se registra como opción para abrir PDF en la cuenta actual; no cambia el programa predeterminado. Para probar el icono de documentos, ejecutar primero el build desde su ubicación definitiva y seleccionar GPDF como aplicación predeterminada para PDF en Configuración de Windows. El icono de documentos está integrado en el ejecutable, separado del icono de aplicación.

El último menú, Ayuda, incluye Acerca de GPDF con el nombre completo, la versión y el autor.

```text
Grant's PDF Reader versión 0.2.7
Creado por Lautaro Grant
```

El paquete completo ocupa aproximadamente 46,3 MB (44,2 MiB), incluidas las bibliotecas y licencias. Las herramientas de desarrollo y los builds anteriores no son necesarios para utilizarlo. Las copias de recuperación y los documentos requieren espacio adicional variable.

El trabajo de idiomas está en la rama `feature/i18n`, para revisar y luego integrar en `main` cuando se autorice. Un único repositorio y una única versión contienen ambos idiomas; no se mantienen ramas permanentes separadas por idioma. `baseline-0.1` conserva el punto de partida. Fuentes, catálogos, scripts del instalador y PDFs sintéticos se versionan; herramientas, compilaciones, paquetes, claves privadas y documentos personales quedan fuera del repositorio.

El español es el idioma fuente. Los mensajes de interfaz usan `gpdfTr()` y el catálogo inglés se edita en `resources/translations/gpdf_en.ts`. Después de modificarlo, ejecutar `./scripts/update-translations.ps1` y el build. En un commit autorizado deben incluirse el `.ts` y el `.qm` regenerado. Las herramientas de Qt Linguist solo son necesarias para desarrollo; no agregan bibliotecas al programa distribuido. Consultar `docs/validation-i18n.md` para la implementación y las comprobaciones.

- `docs/validation-0.2.4.md`: revisión del idioma, arranque, resaltados, subrayado e interfaz.
- `docs/validation-0.2.6-help.md`: menú Ayuda, cuadro Acerca de GPDF y pruebas de archivos grandes.
- `docs/validation-crop-selection.md`: corrección y pruebas de selección en páginas recortadas y rotadas.
- `docs/validation-downward-menus.md`: comprobación de la posición de los menús de colores.
- `docs/validation-installer.md`: instalación, actualización, ejecución y desinstalación verificadas en un entorno de prueba.
- `docs/validation-i18n.md`: idiomas, elección desde el instalador, preferencias, pruebas y mediciones.
- `docs/validation-icons.md`: verificación de iconos y restauración de la fuente original.
- `docs/validation-0.2.5.md`: investigación y comparación del renderizado de Inter al 100 %.
- `docs/architecture.md`: componentes y decisiones de concurrencia.
- `docs/validation-0.2.3.md`: verificación de traslado de comentarios, cierre de pestañas, búsqueda y navegación por número.
- `docs/validation-0.2.2.md`: verificación de apertura externa, zoom, ventanas de comentarios y cursores nativos.
- `docs/validation-0.2.1.md`: verificación del cambio de nombre a GPDF.
- `docs/validation-0.2.0.md`: cobertura y mediciones de la consolidación anterior.
- `CHANGELOG.md`: cambios por versión.
- `docs/development-history.md`: notas históricas anteriores.

Qt se enlaza dinámicamente. Los paquetes incluyen avisos de Qt y PDFium en `licenses/`. Las herramientas de pruebas no se distribuyen. Dependencias: Qt 6.8.3 y PDFium chromium/8086, con hash fijado en bootstrap.
