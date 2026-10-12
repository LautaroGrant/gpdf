#pragma once
#include <QFrame>
#include <QRect>
class QTextEdit;
class CommentPopup : public QFrame {
public:
    explicit CommentPopup(QWidget* parent);
    void showContents(const QString& text,const QRect& anchor,const QRect& available);
private:
    QTextEdit* contents;
};
