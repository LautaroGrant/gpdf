#pragma once
namespace AppIdentity {
// Retain storage and IPC identities from StudyPDF so renaming the application
// preserves settings, reading progress, bookmarks and pending recovery copies.
inline constexpr const char* settingsOrganization="StudyPDF";
inline constexpr const char* settingsApplication="StudyPDF";
inline constexpr const char* instancePrefix="StudyPDF-";
}
