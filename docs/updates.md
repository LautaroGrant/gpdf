# Actualizaciones desde GitHub Releases

GPDF consulta `https://api.github.com/repos/LautaroGrant/gpdf/releases/latest` en segundo plano cinco segundos después del inicio, una vez por inicio de la aplicación. La fecha de la última consulta se conserva como información, pero no impide detectar una publicación nueva al volver a abrir GPDF. Los errores automáticos no muestran ventanas. Ayuda → Buscar actualizaciones permite consultar en cualquier momento, incluso una versión omitida. Ayuda → Buscar actualizaciones automáticamente controla la comprobación al iniciar.

Se admiten etiquetas estables `vX.Y.Z` o `X.Y.Z`; la comparación de versiones es numérica. Se excluyen borradores y versiones preliminares. El aviso muestra las notas como texto, con Descargar e instalar, Más tarde y Omitir esta versión. Si hay otro diálogo abierto o la ventana está oculta o minimizada, el resultado se conserva y el aviso se posterga hasta que pueda mostrarse, sin volver a consultar GitHub.

## Publicar una versión compatible

1. Actualizar `VERSION`, changelog y documentación, compilar y probar con `scripts/build-installer.ps1`.
2. Crear una publicación estable en `LautaroGrant/gpdf`, con etiqueta `vX.Y.Z` o `X.Y.Z` y notas de cambios. Marcarla como la última publicación estable.
3. Adjuntar `GPDF-X.Y.Z-Setup.exe`. GitHub debe informar su campo `digest` con formato `sha256:…` en la API. La descarga integrada exige ese hash y un tamaño válido de hasta 256 MiB. No se necesita un servicio adicional ni credenciales dentro del programa.
4. Adjuntar también el ZIP portable y el archivo SHA256SUMS si se distribuye esa edición.

El instalador debe estar en la ruta del repositorio y etiqueta consultados. Se utiliza HTTPS y se impiden redirecciones a HTTP. El archivo se descarga con `QFile` a una carpeta temporal única; se comprueba su tamaño y SHA-256 y se cierra realmente el archivo antes de ejecutarlo, después de la confirmación. No se usa `QTemporaryFile` para el ejecutable: su `close()` conserva un handle interno que impide iniciar el instalador en Windows. Cancelar, fallar la descarga o no coincidir el hash elimina el temporal. El hash detecta corrupción respecto del artefacto publicado en GitHub; no sustituye una firma de código del editor.

## Instalación y edición portable

La instalación se reconoce por `unins000.exe` junto a GPDF. El instalador se abre en modo interactivo, conservando la carpeta actual y el idioma de la interfaz. Antes de iniciarlo se solicita cerrar GPDF y se utiliza el cierre normal de la ventana, incluidos guardado, descarte o cancelación en todos los documentos. Cancelar ese cierre impide ejecutar el instalador. El instalador maneja el reemplazo de archivos y ofrece abrir GPDF al finalizar.

La edición portable, o una publicación sin instalador verificable, abre la página oficial de descargas en el navegador. No se reemplazan archivos de la edición portable automáticamente. El instalador descargado correctamente queda en la carpeta temporal del sistema cuando se inicia, para no borrarlo mientras se ejecuta; puede limpiarse después con las herramientas habituales de Windows.

Preferencias locales bajo la identidad existente StudyPDF: `updates/automatic`, `updates/lastCheck` y `updates/skipped`. Las pruebas y las capturas de validación no inician comprobaciones automáticas.

La función comienza a estar disponible al instalar una versión que incluya este código. Las versiones anteriores necesitan esa primera actualización manual.

Las publicaciones deben ser públicas y estables. La comprobación sin credenciales del 22 de octubre de 2026 confirmó que `/releases/latest` devuelve `v0.3.0`, estable, y un instalador de 16.476.639 bytes con digest SHA-256 válido. La descarga real coincidió con el tamaño y hash publicados. Se reprodujo y corrigió localmente el bloqueo del ejecutable temporal mediante un proceso de prueba inocuo; no se ejecutó el instalador sobre la copia del usuario ni se modificó la publicación.

Las copias 0.2.8 y 0.3.0 publicadas antes de esta corrección conservan el fallo del archivo temporal y la espera de 24 horas. Esa primera actualización debe hacerse ejecutando manualmente el instalador. Por decisión del usuario, los paquetes locales se regeneran manteniendo 0.3.0. Una copia que ya indique 0.3.0 no anunciará otra compilación con el mismo número: debe reinstalarse manualmente el paquete corregido. Los archivos de GitHub no se reemplazan automáticamente al reconstruir los paquetes locales.

Referencias: [API de publicaciones](https://docs.github.com/en/rest/releases/releases), [hash de los artefactos](https://docs.github.com/en/rest/releases/assets), [redirecciones de Qt](https://doc.qt.io/qt-6.8/qnetworkrequest.html).
