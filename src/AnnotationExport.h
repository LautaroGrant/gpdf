#pragma once
#include "PdfDocument.h"
namespace AnnotationExport {
bool write(const QString& path,const QString& name,const QVector<PdfDocument::AnnotationEntry>& entries);
}
