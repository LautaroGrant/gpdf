# GPDF 0.2.8

[Español](README.es.md)

GPDF (formerly StudyPDF) is a lightweight PDF reader and annotation editor for Windows x64, built with C++20, Qt Widgets and PDFium. Current status: working local beta.

## Install and run

To use GPDF, obtain the installer or the complete portable package from the release distribution. The `dist/` paths below are local build outputs; binaries are not included in the source repository.

1. Run `GPDF-0.2.8-Setup.exe` on Windows x64.
2. Choose English or Español, then choose the installation folder and shortcuts.
3. Finish the installation and open GPDF from the Start menu.

For portable use, extract the complete package into a folder and run `GPDF.exe` inside it. No development tools are needed.

Run `dist/GPDF-0.2.8/GPDF.exe` and keep the complete portable folder together. Installing Qt separately is unnecessary. `build-info.json` records the version, base commit, uncommitted-source status and binary hashes.

The Windows installer is `dist/installers/GPDF-0.2.8-Setup.exe`. It installs for the current user without administrator privileges, under `%LOCALAPPDATA%\Programs\GPDF` by default. The destination folder can be changed during installation and updates. It includes a Start menu shortcut, an optional desktop shortcut and an uninstaller. It advertises GPDF as a PDF application while preserving the user's default PDF reader. Uninstall through Windows Installed apps; preferences and recovery copies remain available.

The installer defaults to **English**, including on Spanish Windows and during updates. You can choose **Español** when it starts. This becomes GPDF's initial language, even when the optional launch is unchecked. Updates preserve an existing language preference. Portable installations start in Spanish. Change the language through **Help > Language** or **Ayuda > Idioma**; it applies at the next launch.

`dist/installers/GPDF-0.2.8-Setup-TestSigned.exe` is the output name for an optional test variant signed with a self-signed certificate named Lautaro Grant. The application, installer and uninstaller are signed. Windows does not automatically trust this certificate. See [test signing](docs/test-signing.md).

Updates suggest the previously installed folder; you can change it in the installer. The outdated test-signed build was removed during cleanup. Use the latest `Setup.exe` for this version; see the test-signing guide to generate a new signed test build.

Open PDFs from File or Windows Open with. Additional documents open in tabs. Opening from Explorer reuses the existing instance and restores its normal or maximized state.

## Features

- Continuous, single-page and two-page views, zoom, rotation and full screen. Enter a page number and press Enter to jump to it.
- Double-click selects a word; triple-click selects its visual line. Text selection and copying; search ignores case and accents. Enter in the search field or on a selected result advances to the next match.
- Comments, text boxes, highlights, underlines and pen strokes; editing, colors, deletion and undo/redo.
- Tabs, thumbnails, PDF outlines, personal bookmarks and reading progress. Use the mouse or arrow keys to navigate thumbnails. Personal bookmarks retain selection after navigation so they can be renamed or deleted. Middle-click a tab to close it.
- Recovery copies after an unexpected interruption; originals are preserved. Closing normally starts the next launch without the previous documents.
- Annotation export to Markdown, text or HTML; printing and print preview.
- Spanish and English in one executable, dark interface with white document pages, and customizable shortcuts.

Creating a comment or clicking its icon with Select or Comment opens its contents over the page. Close it with the close button, Esc or a click outside. Use Select to drag the icon within a page or onto another page. Move near the top or bottom edge to scroll; Esc cancels. Moves preserve contents and can be undone in one step.

Annotations are saved in the PDF. Personal bookmarks and reading progress are local and identified by file path. The original StudyPDF settings location is retained. Save as adopts the saved file as the current document.

Pen, highlight and underline icons show the selected color. Underline's dropdown sets color and thickness (0.5-12 pt). New underlines leave a 0.75 pt gap below the text. Drawing dropdowns always open below their buttons. Annotation page numbers and types appear in bold. Startup restores the window before showing it.

Selection accounts for internal page crops and rotation. Regression tests cover shifted crop boxes and all four view rotations. New highlights contain neither comment text nor an additional comment annotation. Other readers may include highlights in their general annotation panels.

Translations affect interface text and generated export headings only. PDF text, metadata and user comments retain their original contents. Both languages share the same engine and caches.

## Default shortcuts

Defaults follow the interface language. The table below uses English; Spanish uses Ctrl+G / Ctrl+Shift+G for saving, Ctrl+S / Ctrl+Shift+S for highlighting, Ctrl+B for search and Ctrl+I for go to page. Customize them from Shortcuts; existing custom assignments are preserved.

| Action | Shortcut |
|---|---|
| Select / Comment / Text / Pen | F1 / F2 / F3 / F4 |
| Highlight, primary / secondary color | Ctrl+H / Ctrl+Shift+H |
| Save / Save as | Ctrl+S / Ctrl+Shift+S |
| Search / Go to page | Ctrl+F / Ctrl+G |
| Show annotations / Select all | Ctrl+N / Ctrl+Shift+A |
| Underline | Ctrl+U |
| Add personal bookmark | Ctrl+D |
| Next / Previous tab | Ctrl+Tab / Ctrl+Shift+Tab |
| Full screen | F11; Esc to exit |
| Wheel zoom, 25 percentage points | Ctrl+wheel |
| Toolbar zoom, 10 percentage points | + / - buttons |

Middle-click the document for automatic scrolling using native Windows cursors, without a stationary indicator. Click again or press Esc to stop. Delete removes the selected annotation; in text fields it deletes characters. Enter confirms text; Shift+Enter adds a line.

## Build and verify

Requirements: Windows x64, PowerShell, Python 3.11+ available as `python`, and an Internet connection for the initial dependency downloads. Windows `tar` must be available in PATH. Visual Studio and a separate Qt installation are unnecessary: bootstrap installs MinGW, Qt, CMake, Ninja and PDFium inside the project.

Download and extract the source code, or clone the repository, then open PowerShell in the project root (the folder containing `CMakeLists.txt`). Install the development dependencies once:

```powershell
./scripts/bootstrap.ps1
```

Compile in Release mode and run the automated tests:

```powershell
./scripts/build.ps1 -BuildDirectory build-gpdf
```

The executable is `build-gpdf/GPDF.exe`. To build and prepare a portable distribution, including runtime libraries and licenses:

```powershell
./scripts/build.ps1 -BuildDirectory build-gpdf -Package -PackageName GPDF-0.2.8
```

The output is `dist/GPDF-0.2.8/`. After preparing that package, create the installer and validate installation, update and uninstall:

```powershell
./scripts/build-installer.ps1 -SkipBuild
./scripts/validate-installer.ps1
./scripts/validate-installer.ps1 -InstallerLanguage spanish
```

The installer is written to `dist/installers/GPDF-0.2.8-Setup.exe`. Alternatively, `./scripts/build-installer.ps1` without `-SkipBuild` compiles, tests and packages the application before creating the installer. Installer names follow `VERSION`; update the portable package name when changing that version.

If PowerShell blocks local scripts, allow them for the current terminal session with `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass`, then rerun the command.

Dependencies are installed inside the project. The installer script downloads and verifies Inno Setup 6.7.3 in portable mode if needed. Installer tests use an isolated registry branch and omit shortcuts to preserve existing installations and PDF defaults.

The build runs five suites: document persistence, interface, PDF corpus, Spanish and English. Interface tests use Qt offscreen and require temporary files and local Windows IPC. Language tests check preferences, restart behavior, custom shortcuts, About, exports and compiled catalog consistency.

To validate personal PDFs without modifying originals:

```powershell
./scripts/validate-corpus.ps1 -PdfPaths 'C:/path/book.pdf','C:/path/notes.pdf'
./scripts/validate-corpus.ps1 -Stress -OutputDirectory './build/corpus-output'
# With Poppler installed in PATH:
./scripts/validate-corpus.ps1 -Poppler -PdfPaths 'C:/path/book.pdf'
```

Without paths, tests generate a 130-page document and an image-only PDF. Stress adds 64 MiB of padding to check input size rather than graphic complexity. OutputDirectory retains copies in a unique folder; otherwise temporary files are removed.

## Translations and Git

Use one repository and one implementation. Language work is on `feature/i18n`, ready for review before merging into `main`. Languages do not need separate long-lived branches or tags. One release contains both languages.

Spanish is the source language. UI messages use `gpdfTr()` in the GPDF context. Edit `resources/translations/gpdf_en.ts`, then regenerate the embedded English catalog:

```powershell
./scripts/update-translations.ps1
./scripts/build.ps1 -BuildDirectory build-gpdf -Package -PackageName GPDF-0.2.8
```

When committing an approved translation change, include the `.ts` source and regenerated `.qm` together. Qt Linguist tools are development dependencies only. The embedded catalog requires no extra runtime library. See [language validation](docs/validation-i18n.md).

## Current limits

OCR is not included. Image-only scans can be viewed and annotated, but their words cannot be searched or selected without a text layer. PDFs are loaded fully into memory. Opening and saving are synchronous, so large documents may cause pauses. Undo history has entry and memory limits.

Edge/Acrobat editing interoperability and physical printing require manual checks. Automated tests cannot cover every PDF. The main window still coordinates sessions, searching and tools.

## Development and distribution

Version 0.2.8 uses Segoe UI at 10 pt and Qt's default font backend. Application and document icons were provided by the user. Run GPDF from its final location and choose it in Windows Default apps to test the document icon.

Help includes About GPDF with the program name, version and Lautaro Grant as author. The full portable folder occupies approximately 46 MB including libraries and licenses; see the current validation report for measured size. Development tools and old builds are unnecessary to run it. Documents and recovery copies require additional storage.

Builds, packages, tools and personal PDFs are excluded from Git. Sources, catalogs, installer scripts and synthetic fixtures belong in the repository. Private signing keys and certificate passwords do not. A remote push is unnecessary for local builds and testing.

- [Architecture](docs/architecture.md)
- [Installer validation](docs/validation-installer.md)
- [Test signing](docs/test-signing.md)
- [Language validation](docs/validation-i18n.md)
- [Crop and selection regression](docs/validation-crop-selection.md)
- [Drawing dropdown positioning](docs/validation-downward-menus.md)
- [Icons and original font](docs/validation-icons.md)
- [About and large-file checks](docs/validation-0.2.6-help.md)
- [Changelog](CHANGELOG.md)
- [Development history](docs/development-history.md)

Qt is linked dynamically. Packages contain Qt and PDFium notices in `licenses/`. Test executables are not distributed. Dependencies: Qt 6.8.3 and PDFium chromium/8086, with the PDFium hash pinned in bootstrap.
