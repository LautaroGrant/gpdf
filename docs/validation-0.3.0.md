# Validación de GPDF 0.3.0

Fecha: 22 de octubre de 2026.

## Reconstrucción con corrección del actualizador — versión 0.3.0 conservada

Por decisión del usuario se regeneraron los artefactos manteniendo 0.3.0.
Esta sección corresponde a los archivos actuales; las secciones posteriores
documentan la preparación inicial anterior a la corrección.

- Compilación Release y despliegue estándar de Qt: correctos.
- CTest: **13/13 aprobadas**, 37,03 s.
- Instalación, reinstalación, hashes, apertura PDF y desinstalación aisladas: aprobadas con el paquete regenerado; registro y asociación PDF predeterminada conservados.
- ZIP: sus 81 archivos coinciden por SHA-256 con el directorio portable.
- El instalador y el portable anteriores, sus hashes y el manifiesto están conservados en `dist/archive/GPDF-0.3.0-before-update-fix/`.
- Base Git de esta reconstrucción: `8c2d785da6f4331cdd0af3ce9e5eda0978b1819c`, con cambios locales sin commit. No se hizo push ni reemplazo en GitHub.
- Los clientes que ya indiquen 0.3.0 necesitan reinstalar manualmente esta compilación; no se anuncia como una versión superior.

| Artefacto actual | Bytes |
| --- | ---: |
| `dist/installers/GPDF-0.3.0-Setup.exe` | 16.474.598 |
| `dist/GPDF-0.3.0-Portable.zip` | 20.668.331 |
| Distribución sin comprimir | 46.533.658 |

```text
3c208ccc7309bd9b75f33ab9e539066c5b4ce838d461f6f7c10b76331266af28  GPDF-0.3.0-Setup.exe
f7bf551e78aa6ec83d6f60a3d74737167a25f44a6590bef9b7d5454449cd1220  GPDF-0.3.0-Portable.zip
```

Evidencia adicional: `build-release-030/validation-installer-update-fix.log`.
Los hashes actuales para distribución están en `dist/SHA256SUMS-0.3.0.txt`.

## Compilación y procedencia

- Compilación Release nueva en `build-release-030`, con C++20, MinGW 13.1, Qt 6.8.3 y PDFium chromium/8086.
- Generación final con `scripts/build-installer.ps1 -BuildDirectory build-release-030`, incluido despliegue estándar de Qt e Inno Setup 6.7.3.
- Versión de `VERSION`, README inglés/español, manifiestos e instalador: `0.3.0`.
- Base Git: `dc2788fcadbbdba8d825cd15b7d502e522657736`. Los manifiestos indican `workingTreeDirty: true`: se compilaron los cambios locales sin crear commits.
- Se conservaron la identidad de instalación y el `AppId` existente. No se creó tag, commit, push ni publicación en GitHub.
- El instalador generado no tiene firma Authenticode (`NotSigned`).

## Resultados

- CTest: **12/12 pruebas aprobadas**, 31,09 s en la ejecución final completa.
- `ocr_smoke --native`: aprobado con el motor real de Windows, documento mixto, páginas escaneadas no visitadas, búsqueda completa, búsqueda posterior reutilizando texto y ausencia de opciones OCR en Ver.
- La prueba de texto OCR comprueba selección, geometría, búsqueda, anotaciones, conservación al deshacer/rehacer y búsqueda en documentos de más de 64 páginas reconocidas.
- `validate-installer.ps1`, inglés: aprobado.
- `validate-installer.ps1`, español: aprobado.
- En cada idioma se verificaron dos pasadas de instalación/reinstalación in situ, hashes de todos los archivos, comandos e iconos PDF, apertura de un PDF desde la aplicación instalada y desinstalación.
- Se conservó un archivo ajeno al instalador y se comprobó que no cambiara el lector PDF predeterminado. Se eliminaron las claves y la instalación de prueba.
- ZIP portable: los **81 archivos** coinciden por SHA-256 con la distribución sin comprimir.
- `git diff --check`: sin errores de espacios en blanco.

Los temporales se ubicaron en `build-release-030/test-temp`. Las herramientas
nativas y las pruebas con procesos/comunicación local se ejecutaron con los
permisos necesarios para evitar restricciones del entorno de validación.

## Ajustes para preparar el repositorio

- `.gitignore` permite incluir las fuentes de las pruebas requeridas por CMake y cuatro PDF sintéticos documentados; mantiene excluidos los binarios, herramientas, temporales y demás datos de prueba.
- Se corrigió el aislamiento de la prueba de recuperación: detiene también el guardado periódico de la ventana anterior para que una ejecución lenta no sobrescriba los manifiestos del escenario siguiente. El guardado periódico del programa no cambió.

## Artefactos finales

| Artefacto | Bytes | MB decimales |
| --- | ---: | ---: |
| `dist/installers/GPDF-0.3.0-Setup.exe` | 16.476.639 | 16,48 |
| `dist/GPDF-0.3.0-Portable.zip` | 20.670.703 | 20,67 |
| Distribución `dist/GPDF-0.3.0/` | 46.542.800 | 46,54 |

SHA-256:

```text
452174816a2904942637935a1a5830d1c7d8ead249a26163b451c597c4e3b550  GPDF-0.3.0-Setup.exe
12f1bc6222e50441753a43769a9ba4d81e8e0afd94a79fa39e35b36e31a8872c  GPDF-0.3.0-Portable.zip
```

El archivo listo para distribuir con estos hashes es `dist/SHA256SUMS-0.3.0.txt`.
La procedencia y el hash del ejecutable están en `dist/GPDF-0.3.0/build-info.json`;
los del instalador, en `dist/installers/GPDF-0.3.0-Setup.json`.

## Evidencia local

- `build-release-030/Testing/Temporary/LastTest.log`
- `build-release-030/validation-installer-english.log`
- `build-release-030/validation-installer-spanish.log`
- `build-release-030/validation-english/`
- `build-release-030/validation-spanish/`

Las notas bilingües y los requisitos para publicar están en `docs/release-0.3.0.md`.
La prueba del actualizador valida sus reglas localmente; no se publicó una release
ni se ejecutó una actualización contra un lanzamiento real en GitHub.
