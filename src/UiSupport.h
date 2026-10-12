#pragma once
#ifdef GPDF_PERF_PROBE
#include <QElapsedTimer>
#include <QDebug>
#endif
#include "PdfDocument.h"
#include "AppIdentity.h"
#include "SingleInstance.h"
#include <QCryptographicHash>
#include <QFormLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QThreadPool>
#include <QPointer>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <atomic>
#include <QTreeWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QStandardPaths>
#include <QWidgetAction>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QTranslator>
#include <QActionGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QIconEngine>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QMenuBar>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif
#include <QTimer>
#include <QColorDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSignalBlocker>
#include <QComboBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QAbstractItemView>
#include <QScreen>
#include <QShortcut>
#include <QFrame>
#include <QKeyEvent>
#include <QMimeData>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QPrinter>
#include <QPrintDialog>
#include <QScopedValueRollback>
#include <QPrintPreviewDialog>
#include <QScrollBar>
#include <QCache>
#include <QTabBar>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <memory>
#include <vector>
#include <QResizeEvent>
#include <QPaintEvent>
#include <cmath>
#include <limits>
#include <functional>
#include <algorithm>

#ifdef Q_OS_WIN
// Disable only this window's transition animation while Qt restores its native frame.
class WindowTransitionsGuard {
    HWND hwnd; BOOL previous=FALSE; bool changed=false;
    using SetAttribute=HRESULT (WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
    SetAttribute setAttribute=nullptr;
public:
    explicit WindowTransitionsGuard(HWND target):hwnd(target) {
        if(!hwnd) return;
        static HMODULE dwm=LoadLibraryW(L"dwmapi.dll");
        using GetAttribute=HRESULT (WINAPI*)(HWND,DWORD,PVOID,DWORD);
        auto getAttribute=dwm ? reinterpret_cast<GetAttribute>(GetProcAddress(dwm,"DwmGetWindowAttribute")) : nullptr;
        setAttribute=dwm ? reinterpret_cast<SetAttribute>(GetProcAddress(dwm,"DwmSetWindowAttribute")) : nullptr;
        if(getAttribute && setAttribute && SUCCEEDED(getAttribute(hwnd,DWMWA_TRANSITIONS_FORCEDISABLED,&previous,sizeof(previous)))) {
            BOOL disabled=TRUE; changed=SUCCEEDED(setAttribute(hwnd,DWMWA_TRANSITIONS_FORCEDISABLED,&disabled,sizeof(disabled)));
        }
    }
    ~WindowTransitionsGuard() { if(changed) setAttribute(hwnd,DWMWA_TRANSITIONS_FORCEDISABLED,&previous,sizeof(previous)); }
};
#endif

class GpdfToolIcon : public QIconEngine {
    int tool;
public:
    explicit GpdfToolIcon(int value):tool(value) {}
    QIconEngine* clone() const override { return new GpdfToolIcon(tool); }
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state) override {
        QPixmap image(size); image.fill(Qt::transparent);
        QPainter painter(&image); paint(&painter,QRect(QPoint(0,0),size),mode,state);
        return image;
    }
    void paint(QPainter* painter,const QRect& rect,QIcon::Mode mode,QIcon::State) override {
        painter->save(); painter->setRenderHint(QPainter::Antialiasing);
        double side=std::min(rect.width(),rect.height());
        painter->translate(rect.x()+(rect.width()-side)/2,rect.y()+(rect.height()-side)/2); painter->scale(side/24,side/24);
        QColor color=QApplication::palette().color(mode==QIcon::Disabled ? QPalette::Disabled : QPalette::Active,QPalette::ButtonText);
        painter->setPen(QPen(color,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); painter->setBrush(Qt::NoBrush);
        QPainterPath path;
        if(tool==0) { path.moveTo(5,3); path.lineTo(5,19); path.lineTo(9,15); path.lineTo(12,21); path.lineTo(15,19.5); path.lineTo(12,13.5); path.lineTo(18,13.5); path.closeSubpath(); painter->drawPath(path); }
        else if(tool==1) { path.moveTo(5,4); path.lineTo(19,4); path.quadTo(21,4,21,6); path.lineTo(21,15); path.quadTo(21,17,19,17); path.lineTo(10,17); path.lineTo(5,21); path.lineTo(5,17); path.quadTo(3,17,3,15); path.lineTo(3,6); path.quadTo(3,4,5,4); painter->drawPath(path); painter->drawLine(QPointF(7,8),QPointF(17,8)); painter->drawLine(QPointF(7,12),QPointF(14,12)); }
        else if(tool==2) { painter->drawLine(QPointF(5,5),QPointF(19,5)); painter->drawLine(QPointF(12,5),QPointF(12,20)); painter->drawLine(QPointF(8,20),QPointF(16,20)); }
        else if(tool==3) { path.moveTo(4,16); path.lineTo(15,5); path.quadTo(17,3,19,5); path.quadTo(21,7,19,9); path.lineTo(8,20); path.lineTo(3,21); path.closeSubpath(); painter->drawPath(path); painter->drawLine(QPointF(13,7),QPointF(17,11)); painter->drawLine(QPointF(4,16),QPointF(8,20)); }
        else { path.moveTo(7,15); path.lineTo(15,4); path.lineTo(21,9); path.lineTo(13,19); path.closeSubpath(); painter->drawPath(path); painter->drawLine(QPointF(6,16),QPointF(3,20)); painter->drawLine(QPointF(3,20),QPointF(9,20)); painter->drawLine(QPointF(9,20),QPointF(11,18)); painter->setPen(QPen(QColor("#ffd232"),2.5,Qt::SolidLine,Qt::RoundCap)); painter->drawLine(QPointF(3,23),QPointF(21,23)); }
        painter->restore();
    }
};

class GpdfCommentDialog : public QInputDialog {
public:
    explicit GpdfCommentDialog(QWidget* parent,const QString& title,const QString& contents):QInputDialog(parent) {
        setWindowTitle(title); setLabelText("Texto (Enter para guardar, Shift+Enter para nueva línea):");
        setOption(QInputDialog::UsePlainTextEditForTextInput); setInputMode(QInputDialog::TextInput); setTextValue(contents);
        qApp->installEventFilter(this);
    }
    ~GpdfCommentDialog() override { qApp->removeEventFilter(this); }
protected:
    bool eventFilter(QObject* object,QEvent* event) override {
        auto input=qobject_cast<QPlainTextEdit*>(object);
        if(input && input->window()==this && event->type()==QEvent::KeyPress) {
            auto key=static_cast<QKeyEvent*>(event);
            if((key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter) && !key->modifiers().testFlag(Qt::ShiftModifier)) { accept(); return true; }
        }
        return QInputDialog::eventFilter(object,event);
    }
};
static QString studyMultilineText(QWidget* parent,const QString& title,const QString& contents,bool* accepted) {
    GpdfCommentDialog dialog(parent,title,contents);
    *accepted=dialog.exec()==QDialog::Accepted;
    return dialog.textValue();
}

void installSpanishTranslations();

class SearchResultDelegate : public QStyledItemDelegate {
    mutable QCache<QString,QSize> hintCache{4*1024};
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        QStyleOptionViewItem background(option); initStyleOption(&background,index); background.text.clear();
        auto style=option.widget ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,&background,painter,option.widget);
        if(option.state & QStyle::State_Selected) painter->fillRect(option.rect,option.palette.brush(QPalette::Highlight));
        QTextDocument text; text.setDefaultFont(option.font); text.setHtml(QString("<div style=\"color: %1\">%2</div>").arg(option.palette.color(option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text).name(),index.data(Qt::UserRole).toString())); text.setTextWidth(option.rect.width()-8);
        QAbstractTextDocumentLayout::PaintContext context; context.palette=option.palette;
        if(option.state & QStyle::State_Selected) context.palette.setColor(QPalette::Text,option.palette.color(QPalette::HighlightedText));
        painter->save(); painter->setClipRect(option.rect); painter->translate(option.rect.topLeft()+QPoint(4,2)); text.documentLayout()->draw(painter,context); painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        int width=option.widget ? option.widget->width()-12 : 240;
        QString html=index.data(Qt::UserRole).toString();
        QString key=QString::number(width)+'|'+option.font.toString()+'|'+html;
        if(auto size=hintCache.object(key)) return *size;
        QTextDocument text; text.setDefaultFont(option.font); text.setHtml(html); text.setTextWidth(std::max(100,width));
        QSize size(width,int(std::ceil(text.size().height()))+4);
        hintCache.insert(key,new QSize(size),std::max<qsizetype>(1,(key.capacity()*sizeof(QChar)+sizeof(QSize)+1023)/1024)); return size;
    }
};

class ViewModeCombo : public QComboBox {
public:
    using QComboBox::QComboBox;
    void showPopup() override {
        QComboBox::showPopup();
        auto popup=view()->window();
        auto position=mapToGlobal(QPoint(0,height()));
        const auto available=screen()->availableGeometry();
        popup->resize(popup->width(),std::min(popup->height(),std::max(1,available.bottom()-position.y()+1)));
        popup->move(position);
    }
};

class TextResizeGrip : public QWidget {
    QPoint start;
    QSize original;
    QWidget* target;
public:
    explicit TextResizeGrip(QWidget* parent):QWidget(parent),target(parent) { setMouseTracking(true); setFixedSize(16,16); setCursor(Qt::SizeFDiagCursor); setToolTip("Arrastrá para ampliar o reducir el recuadro"); }
protected:
    void paintEvent(QPaintEvent*) override { QPainter p(this); p.setPen(QColor("#788694")); for(int i=4;i<=12;i+=4) p.drawLine(i,14,14,i); }
    void mousePressEvent(QMouseEvent* e) override { start=e->globalPosition().toPoint(); original=target->size(); e->accept(); }
    void mouseMoveEvent(QMouseEvent* e) override { if(e->buttons() & Qt::LeftButton) { auto delta=e->globalPosition().toPoint()-start; target->resize(original+QSize(delta.x(),delta.y())); } e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override { e->accept(); }
};
class InlineTextBox : public QWidget {
public:
    std::function<void()> resized;
    explicit InlineTextBox(QWidget* parent):QWidget(parent) {}
protected:
    void resizeEvent(QResizeEvent* event) override { QWidget::resizeEvent(event); if(resized) resized(); }
};
