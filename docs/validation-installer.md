# Instalador Windows de GPDF 0.2.6

Los cambios previos se guardaron en el commit b6bfc8f, sin nuevo tag. El paquete ejecutable se recompiló y aprobó las tres suites CTest. El instalador y sus scripts se prepararon después y permanecen sin commit.

Inno Setup 6.7.3, descargado de la publicación oficial y verificado con SHA-256 9C73C3BAE7ED48D44112A0F48E66742C00090BDB5BEF71D9D3C056C66E97B732 y firma válida de Pyrsys B.V. El compilador se extrajo con /PORTABLE=1 dentro de .tools, sin instalarlo globalmente.

Instalación en español para el usuario actual, Windows 10 o posterior compatible con x64. Ruta predeterminada: %LOCALAPPDATA%\Programs\GPDF. Acceso en Inicio, acceso opcional en escritorio y desinstalador. Incluye las DLL, complementos, iconos, documentación y licencias del paquete. No distribuye herramientas de desarrollo.

El registro anuncia GPDF para PDF, utiliza el icono de documento en índice 1 y el de aplicación en índice 0. No escribe UserChoice ni el valor predeterminado de .pdf. Desinstalar retira archivos y registro de la aplicación; no borra configuración, recuperación ni archivos ajenos al paquete.

validate-installer.ps1 compiló el mismo script con una rama de registro aislada y sin accesos directos ni entrada real en Aplicaciones instaladas. Instaló y reinstaló en una carpeta del proyecto, comparó hashes de todos los archivos con el paquete y comprobó comando e icono registrados. Abrió un PDF con el ejecutable instalado y generó una captura. Desinstaló, comprobó eliminación del ejecutable y ProgID de prueba, conservación de un archivo ajeno y que UserChoice no cambió. La rama de prueba se eliminó al finalizar.

Resultado aprobado en build-gpdf/validation-installer.log. Captura: build-gpdf/installer-tests/installed-program.png. El instalador de producción no se instaló en la cuenta del usuario durante estas pruebas. Los accesos directos y la entrada real de desinstalación se definieron y compilaron, pero no se ejercitaron en la cuenta actual.

Artefactos: dist/installers/GPDF-0.2.6-Setup.exe y manifiesto SHA-256 junto al instalador. No se aplicó firma digital al instalador de GPDF; la firma verificada corresponde al compilador descargado.

Referencias: https://jrsoftware.org/isdl.php, https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm y https://jrsoftware.org/ishelp/topic_registrysection.htm.

Actualización del 13 de octubre de 2026: LanguageDetectionMethod=none y UsePreviousLanguage=no seleccionan English por defecto también en Windows en español y al actualizar; Español sigue disponible. DisableDirPage=no mantiene visible la elección de carpeta en instalaciones y actualizaciones. Se recompiló dist/installers/GPDF-0.2.6-Setup.exe y su manifiesto. La variante TestSigned anterior no se regeneró en esta revisión. La validación predeterminada omite /LANG para comprobar el inglés y utiliza /DIR con una ruta personalizada. Registro de esta revisión: build-gpdf/validation-installer-default-english.log.
