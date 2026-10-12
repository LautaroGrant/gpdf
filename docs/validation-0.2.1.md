# Validación de GPDF 0.2.1

Fecha: 12 de octubre de 2026. Cambio de nombre de StudyPDF a GPDF.

- Compilación Release desde un directorio nuevo (build-gpdf) aprobada.
- Las tres suites CTest aprobaron: pdf_roundtrip, annotation_ui y pdf_corpus.
- La suite de interfaz verifica el título GPDF al cerrar el documento.
- Se inspeccionó la conservación de la identidad de QSettings y del canal de instancia única mediante AppIdentity.h. No se cambia la ubicación de preferencias, progreso, marcadores ni recuperación.
- Las claves históricas StudyPDFTextStyle permanecen para conservar la lectura del formato de anotaciones anterior.
- El ejecutable y el empaquetado utilizan GPDF.exe. Las distribuciones previas no se sobrescriben.
- No se configuró un remoto ni se realizó un push.

Los límites de compatibilidad con Edge/Acrobat, impresión física y operaciones síncronas siguen siendo los documentados en validation-0.2.0.md. Este cambio no agrega afirmaciones nuevas de rendimiento.
