#pragma once
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QWidget>

class DocumentDragPreview : public QWidget {
public:
    enum Mode { Separate,Merge,Move,Blocked,Working };
    explicit DocumentDragPreview(QWidget* owner):QWidget(owner,Qt::ToolTip|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus|Qt::WindowTransparentForInput) {
        setObjectName("documentDragPreview");
        setAttribute(Qt::WA_TranslucentBackground); setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents); setFocusPolicy(Qt::NoFocus);
        resize(344,66);
    }
    void present(QString name,QString instruction,Mode state,bool dark,QPoint cursor) {
        title=std::move(name); subtitle=std::move(instruction); mode=state; darkMode=dark;
        setProperty("mode",int(mode)); setProperty("instruction",subtitle);
        QPoint position=cursor+QPoint(20,24);
        if(auto screen=QGuiApplication::screenAt(cursor)) {
            const auto area=screen->availableGeometry();
            if(position.x()+width()>area.right()) position.setX(cursor.x()-width()-20);
            if(position.y()+height()>area.bottom()) position.setY(cursor.y()-height()-16);
            position.setX(qMax(area.left(),position.x())); position.setY(qMax(area.top(),position.y()));
        }
        move(position); update(); show();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        const auto panel=QRectF(rect()).adjusted(2,2,-4,-5);
        painter.setPen(Qt::NoPen); painter.setBrush(QColor(0,0,0,darkMode ? 80 : 30));
        painter.drawRoundedRect(panel.translated(2,3),9,9);
        const QColor accent=mode==Merge ? QColor("#16a34a") : mode==Blocked ? QColor("#d97706") : QColor("#3b82f6");
        painter.setPen(QPen(darkMode ? QColor("#536171") : QColor("#cbd5e1"),1));
        painter.setBrush(darkMode ? QColor("#202a38") : QColor("#ffffff")); painter.drawRoundedRect(panel,9,9);
        painter.setPen(Qt::NoPen); auto tint=accent; tint.setAlpha(28); painter.setBrush(tint);
        painter.drawRoundedRect(QRectF(12,14,32,32),7,7);
        painter.setPen(QPen(accent,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(18,22,18,16),2,2); painter.drawLine(18,27,36,27);
        if(mode==Merge) { painter.drawLine(27,29,27,35); painter.drawLine(24,32,30,32); }
        else if(mode==Blocked) painter.drawLine(19,37,35,23);
        else { painter.drawLine(28,22,37,15); painter.drawLine(31,15,37,15); painter.drawLine(37,15,37,21); }
        QFont heading=font(); heading.setWeight(QFont::DemiBold); painter.setFont(heading);
        painter.setPen(darkMode ? QColor("#f1f5f9") : QColor("#182433"));
        painter.drawText(QRect(54,13,width()-70,20),Qt::AlignVCenter,QFontMetrics(heading).elidedText(title,Qt::ElideMiddle,width()-70));
        painter.setFont(font()); painter.setPen(mode==Merge || mode==Blocked ? accent : darkMode ? QColor("#cbd5e1") : QColor("#536171"));
        painter.drawText(QRect(54,34,width()-70,18),Qt::AlignVCenter,fontMetrics().elidedText(subtitle,Qt::ElideRight,width()-70));
    }
private:
    QString title,subtitle;
    Mode mode=Separate;
    bool darkMode=false;
};

class DocumentDropIndicator : public QWidget {
public:
    explicit DocumentDropIndicator(QWidget* owner):QWidget(owner) {
        setObjectName("documentDropIndicator"); setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground); setFocusPolicy(Qt::NoFocus);
    }
    void present(QRect bar,int insertionX,int index) {
        setGeometry(bar); marker=qBound(3,insertionX,qMax(3,width()-4)); success=false;
        setProperty("previewActive",true); setProperty("insertionIndex",index);
        update(); raise(); show();
    }
    void complete(QRect bar,QRect inserted) {
        setGeometry(bar); selected=inserted; success=true; setProperty("previewActive",false);
        update(); raise(); show();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        const QColor accent=success ? QColor("#16a34a") : QColor("#3b82f6");
        auto fill=accent; fill.setAlpha(success ? 42 : 20);
        painter.setBrush(fill); painter.setPen(QPen(accent,2));
        const QRectF area=success ? QRectF(selected.intersected(rect())).adjusted(1,1,-1,-1) : QRectF(rect()).adjusted(1,1,-1,-1);
        painter.drawRoundedRect(area,4,4);
        if(!success) {
            painter.setPen(QPen(accent,3,Qt::SolidLine,Qt::RoundCap)); painter.drawLine(marker,4,marker,qMax(4,height()-5));
            painter.setPen(Qt::NoPen); painter.setBrush(accent);
            painter.drawPolygon(QPolygonF{QPointF(marker-4,2),QPointF(marker+4,2),QPointF(marker,7)});
        }
    }
private:
    int marker=3;
    bool success=false;
    QRect selected;
};
