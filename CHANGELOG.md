# Changelog

## 0.3.1 - October 23, 2026

- Increased definition of letters and words.
- Automatic OCR is limited to 15 pages document in order to save memory. Option and suggestion pop-ups to apply OCR for larger documents.
- Overall better memory handling.

## 0.3.0 — October 22, 2026

- Implementation of automatic local Windows OCR for scanned PDF pages, using the OCR languages installed in Windows without bundling additional recognition models.

### Notes

- Windows PowerShell 5.1 and an installed Windows OCR language are required. Device policies can prevent OCR from running.
- OCR character positions within a word are approximate. Saving a PDF does not embed a searchable OCR text layer; results are retained locally by GPDF.

## 0.2.8 — October 17, 2026

- Implementation of standalone eraser button. Removes highlights, underlines, and ink strokes with the existing undo support.
- Automatic checks for stable updates on GitHub Releases, manual checks from Help, an automatic-check preference, and the option to skip a version.
- Installer downloads with SHA-256 verification, progress, and cancellation. Updates use the normal prompts for unsaved documents before closing GPDF and opening the installer. Portable copies open the download page.
- PDF pages render at the monitor's physical resolution, and the render cache refreshes when display scaling changes.
- Page images and edges align to physical pixels to avoid additional resampling, including at fractional display scales and with 90° rotations.
- The bottom overview bar is hidden to provide more reading space.
- Middle-click autoscroll also moves horizontally when the document is wider than the viewport.
- The rotation button has an icon and rotates pages clockwise in 90° increments with Ctrl+R in both languages.
- Normal shutdown no longer restores the previous documents on the next launch. Recovery of unsaved changes after unexpected interruptions remains available.
- Fix: Reopening an existing PDF checks its contents and reloads it if it changed externally, with a prompt for unsaved annotations.
- Fix: Single-key shortcuts respect typing in annotations, comments, search boxes, and other input fields.
- Fix: Word and line selection now handles PDFs that apply scaling through text or page transformation matrices.


## 0.2.7 — October 13, 2026

- First public release; application version, portable package, and installer updated.

## 0.2.6 — October 13, 2026

- Double-click to select words and triple-click to select a visual line, including accented characters, apostrophes, cropped pages, and rotation.
- Underlines separated from the text; keyboard and mouse navigation for thumbnails; personal bookmarks with an Add button and persistent selection for renaming or deletion.
- Enter advances from the selected search result and wraps to the first result at the end.
- Cursor cache limited to 2 MiB, thumbnails limited to 140 × 190 pixels, chunked export, and reuse of valid images and models while navigating.
- Spanish and English interface text reviewed; underline filter restored between sessions.
- Document tabs limited to 240 pixels, with ellipsis and the full path in the tooltip.
- Search and navigation panels show their open/closed state in the View menu.
- Default shortcuts adapted to English; Ctrl+N for annotations and Ctrl+U for underlining in both languages, preserving custom assignments.
- Installer defaults to English and allows folder selection during updates as well as initial installation.
- Help menu at the end of the native Windows menu bar, with About GPDF and centered version and author information.
- Standard PDF underlines with color, thickness, and a toolbar indicator; editing, deletion, copying, and undo/redo.
- New highlights have no comment body; text excerpts remain available in the panel and exports.
- Neutral Spanish, bold annotation page numbers and types, and color indicators for Pen and Highlight.
- Startup restores the saved state before showing the window; autoscroll has no fixed indicator.
- Application and PDF document icons embedded in the executable; per-user registration allows choosing GPDF as the default without changing that choice automatically.

## 0.2.3 — October 12, 2026

- Select can move comments within and between pages, with edge scrolling and a single undo step.
- Middle-click a tab to close its document, preserving the unsaved-changes prompt.
- Default search shortcut changed to Ctrl+B; the old Ctrl+F default is migrated while custom shortcuts are preserved.
- Editable current-page field with direct navigation by page number and Enter.

## 0.2.2 — October 12, 2026

- Opening a PDF from another instance restores the maximized or normal window without losing its previous state.
- Toolbar + and − buttons change zoom by 10 percentage points; Ctrl+wheel retains 25-point increments.
- Comments appear in a popup over the page, accessible from Select and Comment, with text copying and dismissal through ×, Esc, or an outside click.
- Autoscroll with an origin indicator and native Windows vertical cursors for each direction.

## 0.2.1 — October 12, 2026

- Project, window, and executable renamed to GPDF.
- Settings, recovery, single-instance handling, and existing annotation metadata retain compatibility with StudyPDF.
- README and packaging updated; repository kept exclusively local at this stage.

## 0.2.0 — October 12, 2026

- Startup, Window, PageView, supporting components, translations, session persistence, and export separated. Tests no longer include main.cpp.
- Automated corpus covering 130-page documents, scanned PDFs without text, Unicode annotations, undo/redo, recovery, and reopening; originals verified with SHA-256.
- Tool for validating personal PDFs and 64 MiB inputs without including them in the repository.
- File reading and recovery writes moved outside PDFium's global mutex; save confirmation performed outside the lock, with revision checks to preserve concurrent changes.
- Search results presented in batches of 64.
- Single version source in VERSION, distribution manifest, and reorganized documentation.

## 0.1 — Initial baseline

Reading, annotations, tabs, search, recovery, export, printing, bookmarks, dark mode, and shortcuts.
