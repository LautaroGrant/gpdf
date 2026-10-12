# Study PDF — prueba mínima

Lector de escritorio en C++20, Qt 6.8.3 Widgets y PDFium chromium/8086 (Windows x64).

## Ejecutar

Abrir `StudyPDF.exe` en la carpeta distribuida después de compilar y empaquetar. Los menús Archivo, Ver, Editar, Ir y Atajos utilizan la barra nativa de Windows. Las herramientas están centradas y Anotaciones queda al extremo derecho. No requiere instalar Qt globalmente.

1. Elegir **Inicio → Abrir**. La vista inicial es **Continua**: desplazarse verticalmente con la rueda o la barra lateral.
2. Cambiar el selector de vista a **Página individual** para navegar una página por vez, o a **Dos páginas** para ver parejas lado a lado. Las flechas avanzan una página o una pareja según la vista; en la continua llevan a la página siguiente. La última página impar se muestra sola. El panel de anotaciones corresponde a la página activa: cambia al desplazarse o al hacer clic en una página.
3. Con **Seleccionar**, arrastrar sobre las palabras y presionar **Resaltar**. Ctrl+C copia la selección.
4. Elegir **Comentario** y hacer clic para agregar una nota.
5. Elegir **Texto** y hacer clic para escribir sobre la página; admite acentos y varias líneas.
6. Elegir **Lápiz** y arrastrar para dibujar.
7. En el panel lateral, seleccionar una anotación: su posición se marca en la página. **Editar** (o doble clic/F2) modifica el comentario o el texto visible. **Color** cambia resaltados y trazos. **Borrar** (o Supr con el panel enfocado) elimina la anotación seleccionada.
8. Elegir **Guardar** (Ctrl+G) para actualizar el archivo actual o **Guardar como** (Ctrl+Shift+G) para elegir otro destino y reabrir el archivo para verificar los cambios.

Editar un resaltado o trazo modifica su comentario asociado. El texto libre ajusta su altura al nuevo contenido; si no cabe, se conserva la versión anterior. Los cuatro tipos admitidos son comentarios, texto libre, resaltados y trazos. Las anotaciones bloqueadas y los documentos que impiden modificar anotaciones no admiten edición ni borrado.

El panel de anotaciones empieza oculto al abrir cada PDF. El título de la ventana muestra el nombre del archivo y un asterisco si hay cambios pendientes. Después de **Guardar como**, el nuevo destino pasa a ser el archivo actual.

El botón **Anotaciones** de la barra (o **Ctrl+Shift+A**) muestra u oculta el panel lateral. En la herramienta **Seleccionar**, el cursor cambia al de selección de texto cuando pasa sobre palabras seleccionables, en cualquiera de las tres vistas.

**Buscar** o **Ctrl+F** abre el panel izquierdo. Escribir un término inicia la búsqueda en todas las páginas; Enter o el botón Buscar la inicia de inmediato. Los resultados incluyen el número de página y contexto: seleccionar uno navega y marca la coincidencia. La búsqueda no distingue mayúsculas/minúsculas ni acentos (Cortazar encuentra Cortázar) y requiere una capa de texto (no hace OCR). El botón × cierra el panel.

Las anotaciones se guardan como objetos PDF estándar (Highlight, Text, FreeText e Ink). El texto libre incluye una apariencia de glifos vectoriales y conserva su contenido Unicode. Las notas y trazos se visualizan con PDFium; su interacción en otros lectores puede variar.

## Compilar y probar

En PowerShell desde la carpeta del proyecto, con Python 3.11+:

```powershell
./scripts/bootstrap.ps1
./scripts/build.ps1 -Package
```

Las dependencias quedan en `.tools` y `third_party`, sin modificar PATH global. Los binarios PDFium provienen de la distribución comunitaria bblanchon/pdfium-binaries; el script fija la versión y comprueba el SHA256 observado del archivo.

`pdf_smoke` genera su propio PDF, extrae texto y geometría de selección, crea los cuatro tipos de anotaciones, edita texto y colores, borra anotaciones y verifica cada etapa al guardar y reabrir. Compara contenido Unicode y renderizado, y comprueba que las ediciones inválidas no cambien el documento. `ui_smoke` verifica las tres vistas con cinco páginas, navegación de parejas, última página impar, anotaciones en la página correcta y edición/borrado desde la interfaz. Para conservar sus resultados:

```powershell
./build/pdf_smoke.exe ./build/verification
./build/ui_smoke.exe ./build/verification
```

## Alcance

Esta es una prueba técnica, no la aplicación final. Ofrece desplazamiento continuo, página individual y dos páginas, con navegación y zoom. La vista doble ajusta el ancho de las páginas al espacio disponible. Renderiza sólo las páginas visibles y conserva una caché de hasta 64 MiB y hasta 8 MiB adicionales de vistas previas. Renderiza y busca en tareas de fondo con acceso serializado a PDFium. Incluye deshacer/rehacer, movimiento y redimensionado de cuadros de texto. Permite abrir varios PDFs mediante Archivo > Abrir, con pestañas visibles al tener dos o más documentos; cada documento conserva su página, vista, zoom, búsqueda y cambios. No incluye OCR. La caché y el historial tienen límites de memoria; la apertura y serialización de archivos muy grandes todavía pueden tardar. El resaltado requiere una capa de texto. El texto libre debe caber en la página.

Las pruebas verifican reapertura con PDFium y se revisaron apariencias con Poppler como motor independiente. La edición en Edge/Acrobat todavía requiere una comprobación adicional en esos lectores.

## Fuentes y licencias

- [API de anotaciones PDFium](https://pdfium.googlesource.com/pdfium/+/main/public/fpdf_annot.h).
- [Distribución binaria](https://github.com/bblanchon/pdfium-binaries).
- [Licencias de Qt](https://doc.qt.io/qt-6/licensing.html).

Qt se utiliza mediante bibliotecas dinámicas. El paquete incluye avisos de Qt y PDFium. Antes de distribuir públicamente, revisar el cumplimiento de las licencias de Qt, PDFium y sus dependencias, incluyendo el acceso a las fuentes correspondientes cuando aplique.

## Apertura y navegación

La aplicación reutiliza la instancia abierta: los PDFs recibidos por la línea de comandos se agregan como pestañas. Si el archivo ya está abierto, activa su pestaña. Para el doble clic desde Windows, elegir StudyPDF.exe en «Abrir con» y establecerlo como lector predeterminado; Windows administra esa asociación.

Ctrl+Tab pasa al documento siguiente y Ctrl+Shift+Tab al anterior. Un clic con el botón de la rueda sobre el documento activa el desplazamiento vertical automático; mover el cursor arriba o abajo modifica su dirección y velocidad. Otro clic o Esc lo detiene.

## Propiedades, navegación y atajos

Archivo > Propiedades muestra tamaño, ubicación, páginas, dimensiones y los metadatos disponibles del PDF. El menú Ir permite navegar entre páginas, saltar al principio/final y recorrer el historial de cada documento. Atajos > Ver y modificar atajos permite editar combinaciones, eliminar asignaciones y restaurar los valores predeterminados. Los cambios se conservan entre sesiones y se rechazan combinaciones repetidas.

Ctrl+A muestra/oculta Anotaciones; Ctrl+Shift+A selecciona todo. Suprimir elimina la anotación seleccionada sobre la página o en la lista, y puede deshacerse. Dentro de un campo de texto, Suprimir sigue borrando caracteres. El desplazamiento automático acelera cuanto más se aleja el cursor de su punto de origen.

## Recuperación y sesión

Se crean checkpoints tras cambios (1,5 s) y cada 15 s, en la carpeta recovery junto al archivo de configuración de StudyPDF. Son copias separadas: el PDF original únicamente se reemplaza al Guardar. Al iniciar se reabren los documentos y se recuperan página, desplazamiento, vista y zoom. Si hay cambios pendientes, se ofrece recuperarlos, incluido el texto que estuviera en edición. Los documentos cifrados pueden volver a solicitar su contraseña; no se almacena la contraseña en el manifiesto de sesión.

## Herramientas de estudio

- Ver > Miniaturas / Marcadores del PDF abre la navegación lateral; las miniaturas se generan bajo demanda y los marcadores utilizan el índice incorporado en el documento.
- Archivo > Exportar anotaciones crea un resumen de todo el documento en Markdown, texto o HTML, con el tipo y la página de cada entrada.
- Las flechas junto a Lápiz y Resaltar abren color y opacidad. Lápiz también permite elegir grosor y activar el borrador. Se puede aplicar el formato a un trazo o resaltado seleccionado. El borrador respeta anotaciones bloqueadas y permite deshacer.
- Ver > Modo oscuro oscurece la interfaz y mantiene las páginas blancas, sin invertir sus colores. Ver > Pantalla completa (F11) amplía el espacio de lectura; F11 o Esc permite salir.

La ventana conserva tamaño y estado normal, maximizado o minimizado al cerrar. En pantalla completa se oculta el menú nativo. Ctrl + rueda ajusta el zoom. Ctrl+S resalta la selección con el color primario (amarillo por defecto); Ctrl+Shift+S usa el secundario (naranja). Ambos colores y la opacidad (60 % por defecto) se configuran en el desplegable Resaltar.

## Correcciones de recuperación y exportación

Cancelar la contraseña o no poder abrir una copia de recuperación conserva el respaldo y su entrada de sesión, incluso después de nuevos checkpoints o de cerrar el programa. Se vuelve a ofrecer recuperarlo al iniciar. La pestaña activa se restaura por identidad de documento; las sesiones anteriores también se adaptan cuando faltan archivos. El manifiesto se cierra antes de actualizarlo para permitir el guardado atómico en Windows.

La exportación de anotaciones acepta únicamente .md, .txt y .html, y bloquea el reemplazo de documentos abiertos. El renderizado rechaza escalas inválidas y libera las páginas cargadas mediante gestión automática de recursos. Las pruebas incluyen recuperación cifrada cancelada y reintentada, pestañas con archivos faltantes, exportaciones rechazadas sin alterar los PDFs y renderizados inválidos.

La selección manual permanece azul aunque haya resultados de búsqueda activos. Al iniciar una selección se retira el indicador de búsqueda de la página. El zoom y los cambios de tamaño conservan imágenes anteriores; una caché adicional de vistas previas evita páginas en blanco cuando se reemplazan imágenes grandes. El presupuesto de renderizado se reparte entre las páginas visibles. Durante el ajuste puede mostrarse temporalmente una vista previa de menor resolución.

## Progreso y marcadores

Al volver a abrir un archivo se recuperan su página, desplazamiento, vista, zoom y orientación, aunque se haya cerrado su pestaña. El progreso se guarda en la configuración local de StudyPDF, asociado a la ruta del archivo. Si se mueve o renombra desde otro programa se considera un archivo diferente. Guardar como conserva el progreso y los marcadores en la nueva ruta.

Ver > Marcadores personales abre la pestaña Personales. Ctrl+D agrega un marcador a la página actual; los botones permiten agregar, renombrar y borrar. Un clic en el marcador navega a su página. Son marcadores locales y no modifican el índice incorporado del PDF.

Anotaciones incluye búsqueda de comentarios y textos sin distinguir mayúsculas ni acentos. Se puede exportar por página desde el panel o Archivo > Exportar anotaciones, conservando referencias de página para todo el documento.

El historial utiliza guardados incrementales y comparte los bytes de base en lugar de conservar una copia completa por edición. La recuperación escribe base y cambios directamente al archivo; Guardar sigue generando un PDF completo. La prueba con una base de 8,39 MB y diez comentarios conserva 8,40 MB de historial. Se mantienen límites de entradas y memoria, y los guardados incrementales que no conservan un prefijo compartible usan un respaldo completo. No se agregan bibliotecas de ejecución.


## Rendimiento al anotar documentos grandes

Las anotaciones se almacenan en una caché por página. Editar invalida únicamente la página afectada; deshacer, rehacer y cambiar de documento restauran las cachés correspondientes. En documentos de más de 50 páginas, el recorrido completo se inicia al mostrar Anotaciones; con el panel oculto o el filtro Página actual solo se consultan las anotaciones de esa página. Al reabrir el panel se recupera la lista de todo el documento.

El redibujado conserva la imagen anterior de la página editada y no espera a que PDFium termine una tarea en segundo plano. El historial incremental sigue siendo inmediato, y las llamadas a PDFium continúan serializadas. No se agregan dependencias. La medición sobre el PDF real de Marechal y sus límites están en `docs/highlight-performance.md`; la herramienta de medición es opcional y no se distribuye.


## Idioma y diálogos de Windows

La traducción española de Qt 6.8.3 se incorpora como recurso en el ejecutable. Traduce botones, menús contextuales, campos de atajos y vista previa sin depender de archivos de idioma externos. Los cuadros Abrir e Imprimir vuelven a ser los originales de Windows también en modo noche, y utilizan el idioma y el aspecto del sistema operativo. Guardar como, exportar y los diálogos propios conservan el tema del lector.

Ctrl + rueda acumula los eventos de alta resolución y aplica pasos de veinticinco puntos porcentuales por cada movimiento completo. Salir de pantalla completa con F11 o Esc restaura el estado maximizado o el tamaño normal que había antes de entrar.

La herramienta Seleccionar permite elegir comentarios, cuadros de texto, trazos de lápiz y resaltados sobre la página y borrarlos con Suprimir. Lápiz y Comentario utilizan un cursor de cruz; Texto utiliza el cursor de inserción. Durante la edición de comentarios y textos, Enter confirma el contenido y Shift+Enter inserta una nueva línea.

Las herramientas se muestran como iconos vectoriales y conservan sus nombres y atajos al posar el mouse. F1: Seleccionar; F2: Comentario; F3: Texto; F4: Lápiz. Ctrl+S: resaltar con el color primario; Ctrl+Shift+S: resaltar con el secundario. Ctrl+G: Guardar; Ctrl+Shift+G: Guardar como; Ctrl+I: Ir a página.
