# Archivos para las pruebas

- `annotations-restricted.pdf`: PDF sintético de una página, generado con pypdf. Contraseña de lectura: `read-only`; contraseña de propietario: `owner-test`. Permite imprimir, pero no agregar anotaciones con la contraseña de lectura. Comprueba creación, pegado, guardado y protección de anotaciones existentes.
- `invalid-last-page.pdf`: PDF sintético con dos entradas en el árbol de páginas; la segunda es nula de forma deliberada. PDFium cuenta dos páginas, carga la primera y rechaza la segunda. Comprueba que una navegación fallida conserve la página anterior.
- `recovery-encrypted.pdf` y `bookmarks.pdf` pertenecen a las pruebas anteriores de recuperación cifrada y marcadores.

Estos archivos no contienen documentos del usuario y no se incluyen en la distribución.

## Corpus generado y documentos reales

`corpus_smoke` genera de manera temporal un PDF de 130 páginas con texto/vector y otro de tres páginas con texto rasterizado y sin capa seleccionable. No se guardan en tests/data. Verifica anotaciones Unicode, deshacer/rehacer, recuperación, guardado y hashes de originales.

`scripts/validate-corpus.ps1` admite rutas de PDFs del usuario; las copias se crean en temporales o subcarpetas únicas de OutputDirectory. Nunca agrega estos documentos al repositorio. El modo Stress agrega 64 MiB de espacios al PDF generado para medir el tamaño de entrada, sin representar un PDF gráficamente complejo. El modo Poppler verifica las copias con un motor independiente instalado en PATH.
