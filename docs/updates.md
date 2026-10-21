# Actualizaciones desde GitHub Releases

GPDF consulta `https://api.github.com/repos/LautaroGrant/gpdf/releases/latest` en segundo plano cinco segundos después del inicio. Solo consulta una vez cada 24 horas tras una respuesta válida; los errores automáticos no muestran ventanas. Ayuda → Buscar actualizaciones permite consultar en cualquier momento, incluso una versión omitida. Ayuda → Buscar actualizaciones automáticamente controla la comprobación al iniciar.

Se admiten etiquetas estables `vX.Y.Z` o `X.Y.Z`; la comparación de versiones es numérica. Se excluyen borradores y versiones preliminares. El aviso muestra las notas como texto, con Descargar e instalar, Más tarde y Omitir esta versión. Si hay otro diálogo abierto al recibir la respuesta automática, no se interrumpe ese diálogo.

## Publicar una versión compatible

1. Actualizar `VERSION`, changelog y documentación, compilar y probar con `scripts/build-installer.ps1`.
2. Crear una publicación estable en `LautaroGrant/gpdf`, con etiqueta `vX.Y.Z` o `X.Y.Z` y notas de cambios. Marcarla como la última publicación estable.
3. Adjuntar `GPDF-X.Y.Z-Setup.exe`. GitHub debe informar su campo `digest` con formato `sha256:…` en la API. La descarga integrada exige ese hash y un tamaño válido de hasta 256 MiB. No se necesita un servicio adicional ni credenciales dentro del programa.
4. Adjuntar también el ZIP portable y el archivo SHA256SUMS si se distribuye esa edición.

El instalador debe estar en la ruta del repositorio y etiqueta consultados. Se utiliza HTTPS y se impiden redirecciones a HTTP. El archivo se descarga a un temporal único, se comprueba su tamaño y SHA-256 y solo se ejecuta después de la confirmación. Cancelar, fallar la descarga o no coincidir el hash elimina el temporal. El hash detecta corrupción respecto del artefacto publicado en GitHub; no sustituye una firma de código del editor.

## Instalación y edición portable

La instalación se reconoce por `unins000.exe` junto a GPDF. El instalador se abre en modo interactivo, conservando la carpeta actual y el idioma de la interfaz. Antes de iniciarlo se solicita cerrar GPDF y se utiliza el cierre normal de la ventana, incluidos guardado, descarte o cancelación en todos los documentos. Cancelar ese cierre impide ejecutar el instalador. El instalador maneja el reemplazo de archivos y ofrece abrir GPDF al finalizar.

La edición portable, o una publicación sin instalador verificable, abre la página oficial de descargas en el navegador. No se reemplazan archivos de la edición portable automáticamente. El instalador descargado correctamente queda en la carpeta temporal del sistema cuando se inicia, para no borrarlo mientras se ejecuta; puede limpiarse después con las herramientas habituales de Windows.

Preferencias locales bajo la identidad existente StudyPDF: `updates/automatic`, `updates/lastCheck` y `updates/skipped`. Las pruebas y las capturas de validación no inician comprobaciones automáticas.

La función comienza a estar disponible al instalar una versión que incluya este código. Las versiones anteriores necesitan esa primera actualización manual.

Las publicaciones deben ser públicas y estables. La comprobación sin credenciales del 19 de octubre de 2026 confirmó que el repositorio es público y que la publicación `v0.2.7` existe, pero está marcada como `prerelease: true`. GitHub la excluye de `/releases/latest`, que devuelve HTTP 404 al no haber una publicación estable. Para distribuir futuras actualizaciones por este canal, publicar una release sin la marca de versión preliminar. No se modificó la publicación existente ni se incluyen tokens privados en el programa. Todavía no se validó una actualización completa contra una nueva publicación real.

Referencias: [API de publicaciones](https://docs.github.com/en/rest/releases/releases), [hash de los artefactos](https://docs.github.com/en/rest/releases/assets), [redirecciones de Qt](https://doc.qt.io/qt-6.8/qnetworkrequest.html).
