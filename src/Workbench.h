#pragma once
#include "UiSupport.h"
#include "WorkbenchPdf.h"
#include <QProgressBar>
#include <QTemporaryDir>
#include <QDropEvent>

class WorkbenchPageList : public QListWidget {
public:
    using QListWidget::QListWidget;
    void moveSelectionTo(int row);
    std::function<void()> orderChanged;
protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dropEvent(QDropEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    int insertion=-1;
    int insertionAt(QPoint position) const;
};

class Workbench : public QDialog {
public:
    enum Tool { Merge,Split,Remove,Extract,Organize,Ocr };
    explicit Workbench(QWidget* parent=nullptr);
    ~Workbench() override;
    bool addFile(const QString& path,const QByteArray& password={},const QString& label={});
    void importFiles(const QStringList& paths);
    void setCurrentDocument(std::function<void(Workbench&)> callback);
    void useSnapshot(const PdfDocument& document,const QString& label,const QString& originalPath);
protected:
    bool eventFilter(QObject* object,QEvent* event) override;
    void reject() override;
    void closeEvent(QCloseEvent* event) override;
private:
    struct Input { std::shared_ptr<PdfDocument> pdf; QString path,label,original; bool modifyPermitted=true; };
    QVector<Input> inputs;
    QTemporaryDir snapshots;
    QListWidget *tools,*files,*pages;
    QLabel *title,*description,*pageSummary,*status;
    QLineEdit *range,*groups,*destination;
    QComboBox* splitMode;
    QSpinBox* chunk;
    QPushButton *add,*current,*removeFile,*fileUp,*fileDown,*rotate,*removePage,*reset,*selectAll,*save,*cancel,*browse,*reveal;
    QString lastSavedFile;
    QWidget *fileControls,*rangeControls,*splitControls,*pageControls;
    QProgressBar* progress;
    QTimer thumbnailTimer;
    QThreadPool workers;
    bool busy=false,thumbnailPending=false,syncSelection=false;
    bool needsPassword=false;
    int previewGeneration=0;
    QList<QPair<int,int>> thumbnailOrder;
    QSet<int> failedThumbnails;
    std::shared_ptr<std::atomic_bool> cancellation;
    std::function<void(Workbench&)> currentDocument;
    Tool tool() const { return Tool(tools->currentRow()); }
    int inputIndex() const;
    void chooseFiles();
    void changeTool();
    void loadPages();
    void refreshSelection();
    void selectRange();
    void thumbnail();
    void chooseDestination();
    void run();
    void setBusy(bool value);
    void showError(const QString& error);
    void moveFile(int direction);
};
