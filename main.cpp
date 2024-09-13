#include <QApplication>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QTextStream>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QPainter>
#include <QTextBlock>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileOpenEvent>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

// Custom editor with line numbers and syntax highlighting
class LineNumberArea;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &, int);

private:
    QWidget *lineNumberArea;
};

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}

    QSize sizeHint() const override {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *codeEditor;
};

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
    lineNumberArea = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy) {
        lineNumberArea->scroll(0, dy);
    } else {
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditor::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int) blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// Syntax Highlighter
class SyntaxHighlighter : public QSyntaxHighlighter {
public:
    SyntaxHighlighter(QTextDocument *parent = nullptr) : QSyntaxHighlighter(parent) {}

protected:
    void highlightBlock(const QString &text) override {
        QRegularExpression keywordPattern("\\b(if|else|for|while|int|double|QString)\\b");
        QTextCharFormat keywordFormat;
        keywordFormat.setForeground(Qt::blue);
        keywordFormat.setFontWeight(QFont::Bold);

        QRegularExpressionMatchIterator i = keywordPattern.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            setFormat(match.capturedStart(), match.capturedLength(), keywordFormat);
        }
    }
};

// Find and Replace Dialog
class FindReplaceDialog : public QDialog {
    Q_OBJECT

public:
    FindReplaceDialog(QWidget *parent = nullptr);

signals:
    void findText(const QString &text);
    void replaceText(const QString &text, const QString &replacement);

private slots:
    void find();
    void replace();

private:
    QLineEdit *findLineEdit;
    QLineEdit *replaceLineEdit;
};

FindReplaceDialog::FindReplaceDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Find and Replace");
    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *findLabel = new QLabel("Find:", this);
    findLineEdit = new QLineEdit(this);

    QLabel *replaceLabel = new QLabel("Replace:", this);
    replaceLineEdit = new QLineEdit(this);

    QPushButton *findButton = new QPushButton("Find", this);
    QPushButton *replaceButton = new QPushButton("Replace", this);

    layout->addWidget(findLabel);
    layout->addWidget(findLineEdit);
    layout->addWidget(replaceLabel);
    layout->addWidget(replaceLineEdit);
    layout->addWidget(findButton);
    layout->addWidget(replaceButton);

    connect(findButton, &QPushButton::clicked, this, &FindReplaceDialog::find);
    connect(replaceButton, &QPushButton::clicked, this, &FindReplaceDialog::replace);
}

void FindReplaceDialog::find() {
    emit findText(findLineEdit->text());
}

void FindReplaceDialog::replace() {
    emit replaceText(findLineEdit->text(), replaceLineEdit->text());
}

// Main Editor Window with menu-based actions and QFileOpenEvent handling
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void openFileFromEvent(const QString &fileName);

protected:
    bool event(QEvent *event) override;

private slots:
    void openFile();
    void saveFile();
    void saveFileAs();
    void showFindReplaceDialog();
    void findText(const QString &text);
    void replaceText(const QString &text, const QString &replacement);

private:
    CodeEditor *editor;
    QString currentFile;
    FindReplaceDialog *findReplaceDialog;

    bool saveToFile(const QString &fileName);
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    editor = new CodeEditor(this);
    setCentralWidget(editor);
    new SyntaxHighlighter(editor->document());

    // Create actions
    QAction *openAction = new QAction("Open", this);
    QAction *saveAction = new QAction("Save", this);
    QAction *saveAsAction = new QAction("Save As", this);
    QAction *findReplaceAction = new QAction("Find and Replace", this);

    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(findReplaceAction, &QAction::triggered, this, &MainWindow::showFindReplaceDialog);

    // Create menu
    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction(openAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);

    QMenu *editMenu = menuBar()->addMenu("Edit");
    editMenu->addAction(findReplaceAction);

    // Find/Replace dialog
    findReplaceDialog = new FindReplaceDialog(this);
    connect(findReplaceDialog, &FindReplaceDialog::findText, this, &MainWindow::findText);
    connect(findReplaceDialog, &FindReplaceDialog::replaceText, this, &MainWindow::replaceText);
}

void MainWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", "Text Files (*.txt);;All Files (*)");
    if (!fileName.isEmpty()) {
        openFileFromEvent(fileName);
    }
}

void MainWindow::openFileFromEvent(const QString &fileName) {
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        editor->setPlainText(in.readAll());
        file.close();
        currentFile = fileName;
    } else {
        QMessageBox::warning(this, "Error", "Could not open file");
    }
}

void MainWindow::saveFile() {
    if (currentFile.isEmpty()) {
        saveFileAs();
    } else {
        saveToFile(currentFile);
    }
}

void MainWindow::saveFileAs() {
    QString fileName = QFileDialog::getSaveFileName(this, "Save File As", "", "Text Files (*.txt);;All Files (*)");
    if (!fileName.isEmpty()) {
        saveToFile(fileName);
    }
}

bool MainWindow::saveToFile(const QString &fileName) {
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << editor->toPlainText();
        file.close();
        currentFile = fileName;
        return true;
    } else {
        QMessageBox::warning(this, "Error", "Could not save file");
        return false;
    }
}

void MainWindow::showFindReplaceDialog() {
    findReplaceDialog->show();
}

void MainWindow::findText(const QString &text) {
    if (!text.isEmpty()) {
        editor->find(text);
    }
}

void MainWindow::replaceText(const QString &text, const QString &replacement) {
    if (!text.isEmpty()) {
        editor->setPlainText(editor->toPlainText().replace(text, replacement));
    }
}

bool MainWindow::event(QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        openFileFromEvent(openEvent->file());
        return true;
    }
    return QMainWindow::event(event);
}

// Main Application
class TextEditorApp : public QApplication {
public:
    TextEditorApp(int &argc, char **argv) : QApplication(argc, argv) {}

protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            QFileOpenEvent *fileOpenEvent = static_cast<QFileOpenEvent *>(event);
            if (mainWindow) {
                mainWindow->openFileFromEvent(fileOpenEvent->file());
            }
            return true;
        }
        return QApplication::event(event);
    }

public:
    MainWindow *mainWindow;
};

int main(int argc, char *argv[]) {
    TextEditorApp app(argc, argv);
    MainWindow mainWindow;
    app.mainWindow = &mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();
    return app.exec();
}

#include "main.moc"
