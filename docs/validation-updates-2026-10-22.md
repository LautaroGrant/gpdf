# Diagnóstico y corrección local del actualizador

Fecha: 22 de octubre de 2026. Corrección incorporada a la reconstrucción local de 0.3.0; publicación pendiente.

## Reproducción

- Se inspeccionó la copia instalada 0.2.8 indicada por el usuario. Tiene `unins000.exe` y la búsqueda automática habilitada.
- `/repos/LautaroGrant/gpdf/releases/latest` devuelve `v0.3.0`, estable, publicada el 22 de octubre de 2026 a las 22:59:40 UTC.
- El instalador publicado tiene 16.476.639 bytes y SHA-256 `452174816a2904942637935a1a5830d1c7d8ead249a26163b451c597c4e3b550`.
- La búsqueda manual en la copia 0.2.8 mostró correctamente que 0.3.0 está disponible. La búsqueda automática podía omitirse por la fecha compartida `updates/lastCheck` dentro de las últimas 24 horas.
- Una descarga real mediante Qt coincidió con el tamaño y hash publicados. La descarga no es la causa del error de ejecución.
- Se reprodujo el bloqueo de Windows usando un ejecutable de diagnóstico propio guardado como `QTemporaryFile`: tras `close()`, el proceso no pudo iniciarse porque otro proceso estaba usando el archivo. Al destruir el objeto y liberar su handle, el mismo ejecutable arrancó correctamente.
- La captura enviada por el usuario corresponde al mensaje de fallo de `QProcess::startDetached` en ese punto.

## Corrección

- `UpdateDownload` guarda el contenido con `QFile` dentro de una carpeta `QTemporaryDir` única. Verifica tamaño, SHA-256 y escritura, y libera realmente el handle antes del arranque.
- Los archivos cancelados o inválidos se eliminan. Un instalador verificado solo se conserva cuando su proceso se inicia correctamente.
- Se desconecta la cancelación del diálogo de progreso antes de ocultarlo al finalizar la descarga.
- La comprobación automática se hace una vez por inicio. `updates/lastCheck` deja de bloquearla durante 24 horas.
- Si hay un diálogo abierto o la ventana está oculta/minimizada, se conserva el resultado y se posterga el aviso sin repetir la consulta a GitHub.

## Validación

- Compilación de GPDF y pruebas: correcta.
- CTest: **13/13 aprobadas**, 27,39 s en la ejecución final.
- `update_flow_smoke`: ejecuta un payload de prueba propio antes de destruir el objeto de descarga, comprobando que el handle de escritura ya está liberado. También rechaza tamaño incompleto, tamaño excedido y SHA-256 incorrecto, y comprueba la limpieza de archivos.
- `update_flow_smoke --live`: aprobado contra GitHub. El código corregido, compilado con versión de prueba 0.2.8, detectó la release real pese a una comprobación reciente y mostró el aviso después de cerrar el diálogo inicial. No aceptó la instalación.
- Las pruebas utilizan preferencias aisladas; no instalan la actualización sobre la copia del usuario.
- `git diff --check`: correcto.

## Distribución

Las copias ya instaladas o publicadas 0.2.8/0.3.0 conservan su código anterior.
Para instalar 0.3.0 desde 0.2.8, se puede ejecutar manualmente el instalador
corregido manualmente, evitando el arranque defectuoso del cliente antiguo. Por
decisión del usuario se regenera el paquete local manteniendo 0.3.0, con nuevos
hashes. Las copias que ya indiquen 0.3.0 requieren reinstalación manual para
recibir la corrección. No se reemplazaron archivos en GitHub ni se hizo commit,
push o publicación de esta corrección.
