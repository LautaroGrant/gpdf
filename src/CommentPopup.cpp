#include "CommentPopup.h"
#include <QTextEdit>
#include <QTextDocument>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>
#include <cmath>

CommentPopup::CommentPopup(QWidget* parent):QFrame(parent) {
    setObjectName("commentPopup");
    setStyleSheet("QFrame#commentPopup { background: palette(window); border: 1px solid palette(mid); border-radius: 5px; }");
    setAutoFillBackground(true);
    auto layout=new QVBoxLayout(this); layout->setContentsMargins(8,8,8,8); layout->setSpacing(6);
    auto header=new QHBoxLayout;
    auto title=new QLabel("Comentario"); auto font=title->font(); font.setBold(true); title->setFont(font);
    header->addWidget(title); header->addStretch();
    auto close=new QToolButton; close->setText("×"); close->setObjectName("closeCommentPopup");
    close->setToolTip("Cerrar comentario (Esc)"); close->setAccessibleName("Cerrar comentario"); close->setFixedSize(22,22);
    connect(close,&QToolButton::clicked,this,&QWidget::hide); header->addWidget(close); layout->addLayout(header);
    contents=new QTextEdit; contents->setObjectName("commentContents"); contents->setReadOnly(true);
    contents->setFrameShape(QFrame::NoFrame); contents->setAcceptRichText(false);
    contents->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
    layout->addWidget(contents);
    hide();
}
void CommentPopup::showContents(const QString& text,const QRect& anchor,const QRect& available) {
    const auto bounds=available.adjusted(8,8,-8,-8);
    if(bounds.width()<100 || bounds.height()<75) { hide(); return; }
    const int width=std::min(320,bounds.width());
    contents->setPlainText(text.isEmpty() ? "[Sin comentario escrito]" : text);
    contents->document()->setTextWidth(width-24);
    const int textHeight=std::clamp(int(std::ceil(contents->document()->size().height()))+8,42,170);
    const int height=std::min(textHeight+50,bounds.height());
    setFixedSize(width,height);
    int x=anchor.right()+8;
    if(x+width>bounds.right()+1) x=anchor.left()-width-8;
    x=std::clamp(x,bounds.left(),bounds.right()-width+1);
    const int y=std::clamp(anchor.top(),bounds.top(),bounds.bottom()-height+1);
    move(x,y); show(); raise();
}
