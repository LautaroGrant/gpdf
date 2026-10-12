# Archivos para las pruebas

- `annotations-restricted.pdf`: PDF sintético de una página, generado con pypdf. Contraseña de lectura: `read-only`; contraseña de propietario: `owner-test`. Permite imprimir, pero no agregar anotaciones con la contraseña de lectura. Comprueba creación, pegado, guardado y protección de anotaciones existentes.
- `invalid-last-page.pdf`: PDF sintético con dos entradas en el árbol de páginas; la segunda es nula de forma deliberada. PDFium cuenta dos páginas, carga la primera y rechaza la segunda. Comprueba que una navegación fallida conserve la página anterior.
- `recovery-encrypted.pdf` y `bookmarks.pdf` pertenecen a las pruebas anteriores de recuperación cifrada y marcadores.

Estos archivos no contienen documentos del usuario y no se incluyen en la distribución.
