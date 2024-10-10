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
#include <QTabWidget>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDebug>
#include <QTimer>

bool btrue=1;
// Forward declarations
class CodeEditor;
class FindReplaceDialog;

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
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// Syntax Highlighter
class SyntaxHighlighter : public QSyntaxHighlighter {
public:
    SyntaxHighlighter(QTextDocument *parent = nullptr) : QSyntaxHighlighter(parent) {}

protected:
    void highlightBlock(const QString &text) override {
        // Simple keyword highlighting for demonstration
        QRegularExpression keywordPattern("\\b(if|else|for|while|int|double|QString|return|void|class|public|private|protected|include)\\b");
        QTextCharFormat keywordFormat;
        keywordFormat.setForeground(Qt::blue);
        keywordFormat.setFontWeight(QFont::Bold);

        QRegularExpressionMatchIterator i = keywordPattern.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            setFormat(match.capturedStart(), match.capturedLength(), keywordFormat);
        }

        // String literals
        QRegularExpression stringPattern("\".*?\"");
        QTextCharFormat stringFormat;
        stringFormat.setForeground(Qt::darkGreen);
        QRegularExpressionMatchIterator j = stringPattern.globalMatch(text);
        while (j.hasNext()) {
            QRegularExpressionMatch match = j.next();
            setFormat(match.capturedStart(), match.capturedLength(), stringFormat);
        }

        // Comments
        QRegularExpression commentPattern("//[^\n]*");
        QTextCharFormat commentFormat;
        commentFormat.setForeground(Qt::gray);
        QRegularExpressionMatchIterator k = commentPattern.globalMatch(text);
        while (k.hasNext()) {
            QRegularExpressionMatch match = k.next();
            setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
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
    void replaceAllText(const QString &text, const QString &replacement);

private slots:
    void find();
    void replace();
    void replaceAll();

private:
    QLineEdit *findLineEdit;
    QLineEdit *replaceLineEdit;
    QPushButton *findButton;
    QPushButton *replaceButton;
    QPushButton *replaceAllButton;
};

FindReplaceDialog::FindReplaceDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Find and Replace");
    setModal(false);
    setFixedSize(400, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Find Section
    QHBoxLayout *findLayout = new QHBoxLayout();
    QLabel *findLabel = new QLabel("Find:", this);
    findLineEdit = new QLineEdit(this);
    findLayout->addWidget(findLabel);
    findLayout->addWidget(findLineEdit);
    mainLayout->addLayout(findLayout);

    // Replace Section
    QHBoxLayout *replaceLayout = new QHBoxLayout();
    QLabel *replaceLabel = new QLabel("Replace:", this);
    replaceLineEdit = new QLineEdit(this);
    replaceLayout->addWidget(replaceLabel);
    replaceLayout->addWidget(replaceLineEdit);
    mainLayout->addLayout(replaceLayout);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    findButton = new QPushButton("Find", this);
    replaceButton = new QPushButton("Replace", this);
    replaceAllButton = new QPushButton("Replace All", this);
    buttonLayout->addWidget(findButton);
    buttonLayout->addWidget(replaceButton);
    buttonLayout->addWidget(replaceAllButton);
    mainLayout->addLayout(buttonLayout);

    // Connect buttons
    connect(findButton, &QPushButton::clicked, this, &FindReplaceDialog::find);
    connect(replaceButton, &QPushButton::clicked, this, &FindReplaceDialog::replace);
    connect(replaceAllButton, &QPushButton::clicked, this, &FindReplaceDialog::replaceAll);
}

void FindReplaceDialog::find() {
    emit findText(findLineEdit->text());
}

void FindReplaceDialog::replace() {
    emit replaceText(findLineEdit->text(), replaceLineEdit->text());
}

void FindReplaceDialog::replaceAll() {
    emit replaceAllText(findLineEdit->text(), replaceLineEdit->text());
}

// Main Editor Window with menu-based actions and QFileOpenEvent handling
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void openFileFromEvent(const QString &fileName);
    void newDocument();

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openFile();
    void saveFile();
    void saveFileAs();
    void showFindReplaceDialog();
    void findText(const QString &text);
    void replaceText(const QString &text, const QString &replacement);
    void replaceAllText(const QString &text, const QString &replacement);
    void closeTab(int index);
    void doLater();
private:
    QTabWidget *tabWidget;
    FindReplaceDialog *findReplaceDialog;

    CodeEditor* currentEditor();

    bool saveToFile(CodeEditor *editor, const QString &fileName);
    bool saveCurrentFile();
    bool promptSave(CodeEditor *editor);
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Advanced Qt Text Editor");
    resize(800, 600);

    // Initialize Tab Widget
    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    setCentralWidget(tabWidget);

    // Handle tab close requests
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    // Create actions
    QAction *newAction = new QAction("New", this);
    QAction *openAction = new QAction("Open", this);
    QAction *saveAction = new QAction("Save", this);
    QAction *saveAsAction = new QAction("Save As", this);
    QAction *findReplaceAction = new QAction("Find and Replace", this);

    // Set shortcuts
    newAction->setShortcut(QKeySequence::New);
    openAction->setShortcut(QKeySequence::Open);
    saveAction->setShortcut(QKeySequence::Save);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    findReplaceAction->setShortcut(QKeySequence::Find);

    // Connect actions
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(findReplaceAction, &QAction::triggered, this, &MainWindow::showFindReplaceDialog);

    // Create menu
    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);

    QMenu *editMenu = menuBar()->addMenu("Edit");
    editMenu->addAction(findReplaceAction);

    // Initialize Find/Replace Dialog
    findReplaceDialog = new FindReplaceDialog(this);
    connect(findReplaceDialog, &FindReplaceDialog::findText, this, &MainWindow::findText);
    connect(findReplaceDialog, &FindReplaceDialog::replaceText, this, &MainWindow::replaceText);
    connect(findReplaceDialog, &FindReplaceDialog::replaceAllText, this, &MainWindow::replaceAllText);

    QTimer::singleShot(100, this, SLOT(doLater()));



    // Create an initial blank document
    //newDocument();
}

CodeEditor* MainWindow::currentEditor() {
    return qobject_cast<CodeEditor*>(tabWidget->currentWidget());
}

void MainWindow::doLater() {
    if (btrue) {newDocument();}
}

void MainWindow::newDocument() {
    // Create a new CodeEditor
    CodeEditor *editor = new CodeEditor(this);
    new SyntaxHighlighter(editor->document());

    // Add to tab widget
    QString displayName = "Untitled";
    tabWidget->addTab(editor, displayName);
    tabWidget->setCurrentWidget(editor);

    // No file path yet
    editor->setProperty("filePath", QString());
}

void MainWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open File", "",
                                                    "Text Files (*.txt *.cpp *.h *.py *.md);;All Files (*)");
    if (!fileName.isEmpty()) {
        openFileFromEvent(fileName);
    }
}

void MainWindow::openFileFromEvent(const QString &fileName) {
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString content = in.readAll();
        file.close();

        // Check if file is already open
        for (int i = 0; i < tabWidget->count(); ++i) {
            CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
            if (editor && editor->property("filePath").toString() == fileName) {
                tabWidget->setCurrentIndex(i);
                return;
            }
        }

        // Create a new CodeEditor
        CodeEditor *editor = new CodeEditor(this);
        editor->setPlainText(content);
        new SyntaxHighlighter(editor->document());

        // Add to tab widget
        QString displayName = QFileInfo(fileName).fileName();
        tabWidget->addTab(editor, displayName);
        tabWidget->setCurrentWidget(editor);

        // Store the file path as property
        editor->setProperty("filePath", fileName);
        editor->document()->setModified(false); // Reset modified flag
        btrue=false;
    } else {
        QMessageBox::warning(this, "Error", "Could not open file");
    }
}

bool MainWindow::saveToFile(CodeEditor *editor, const QString &fileName) {
    if (fileName.isEmpty()) {
        return false;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << editor->toPlainText();
        file.close();

        // Update tab title
        QString displayName = QFileInfo(fileName).fileName();
        int index = tabWidget->indexOf(editor);
        if (index != -1) {
            tabWidget->setTabText(index, displayName);
        }

        // Update the filePath property
        editor->setProperty("filePath", fileName);
        editor->document()->setModified(false); // Reset modified flag
        return true;
    } else {
        QMessageBox::warning(this, "Error", "Could not save file");
        return false;
    }
}

bool MainWindow::saveCurrentFile() {
    CodeEditor *editor = currentEditor();
    if (!editor)
        return false;

    QString fileName = editor->property("filePath").toString();
    if (fileName.isEmpty()) {
        return false;
    }

    return saveToFile(editor, fileName);
}

void MainWindow::saveFile() {
    CodeEditor *editor = currentEditor();
    if (!editor)
        return;

    QString fileName = editor->property("filePath").toString();
    if (fileName.isEmpty()) {
        saveFileAs();
    } else {
        saveToFile(editor, fileName);
    }
}

void MainWindow::saveFileAs() {
    CodeEditor *editor = currentEditor();
    if (!editor)
        return;

    QString fileName = QFileDialog::getSaveFileName(this, "Save File As", "",
                                                    "Text Files (*.txt *.cpp *.h *.py *.md);;All Files (*)");
    if (!fileName.isEmpty()) {
        saveToFile(editor, fileName);
    }
}

void MainWindow::showFindReplaceDialog() {
    findReplaceDialog->show();
    findReplaceDialog->raise();
    findReplaceDialog->activateWindow();
}

void MainWindow::findText(const QString &text) {
    if (text.isEmpty())
        return;

    CodeEditor *editor = currentEditor();
    if (!editor)
        return;

    QTextCursor cursor = editor->textCursor();
    bool found = editor->find(text);
    if (!found) {
        QMessageBox::information(this, "Find", QString("'%1' not found.").arg(text));
    }
}

void MainWindow::replaceText(const QString &text, const QString &replacement) {
    if (text.isEmpty())
        return;

    CodeEditor *editor = currentEditor();
    if (!editor)
        return;

    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection() && cursor.selectedText() == text) {
        cursor.insertText(replacement);
    }
    // Find next occurrence
    findText(text);
}

void MainWindow::replaceAllText(const QString &text, const QString &replacement) {
    if (text.isEmpty())
        return;

    CodeEditor *editor = currentEditor();
    if (!editor)
        return;

    QString content = editor->toPlainText();
    int occurrences = content.count(text);
    if (occurrences == 0) {
        QMessageBox::information(this, "Replace All", QString("No occurrences of '%1' found.").arg(text));
        return;
    }

    content.replace(text, replacement);
    editor->setPlainText(content);
    QMessageBox::information(this, "Replace All", QString("Replaced %1 occurrences of '%2' with '%3'.")
                             .arg(occurrences).arg(text).arg(replacement));
}

bool MainWindow::promptSave(CodeEditor *editor) {
    if (!editor->document()->isModified())
        return true;

    QString fileName = editor->property("filePath").toString();
    QString displayName = fileName.isEmpty() ? "Untitled" : QFileInfo(fileName).fileName();
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Save Changes",
                                  QString("Do you want to save changes to '%1'?").arg(displayName),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply == QMessageBox::Yes) {
        if (fileName.isEmpty()) {
            QString newFileName = QFileDialog::getSaveFileName(this, "Save File As", "",
                                                                "Text Files (*.txt *.cpp *.h *.py *.md);;All Files (*)");
            if (newFileName.isEmpty()) {
                return false; // User canceled save as
            }
            return saveToFile(editor, newFileName);
        } else {
            return saveToFile(editor, fileName);
        }
    } else if (reply == QMessageBox::No) {
        return true;
    } else { // Cancel
        return false;
    }
}

void MainWindow::closeTab(int index) {
    QWidget *widget = tabWidget->widget(index);
    CodeEditor *editor = qobject_cast<CodeEditor*>(widget);
    if (editor) {
        if (!promptSave(editor)) {
            return; // User canceled the close
        }
        tabWidget->removeTab(index);
        editor->deleteLater();
    }
}

bool MainWindow::event(QEvent *event) {
    // Handle QFileOpenEvent when the application is triggered from Finder
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        QString filePath = openEvent->file();
        if (!filePath.isEmpty()) { // Corrected from isValid() to isEmpty()
            openFileFromEvent(filePath);
            return true;
        }
    }
    return QMainWindow::event(event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Iterate through all tabs and prompt to save if necessary
    for (int i = 0; i < tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
        if (editor) {
            tabWidget->setCurrentIndex(i);
            if (!promptSave(editor)) {
                event->ignore();
                return; // User canceled the close
            }
        }
    }
    event->accept();
}

// Custom QApplication to handle QFileOpenEvent globally
class TextEditorApp : public QApplication {
    Q_OBJECT

public:
    TextEditorApp(int &argc, char **argv) : QApplication(argc, argv), mainWindow(nullptr) {}

    void setMainWindow(MainWindow *window) {
        mainWindow = window;
    }

protected:
    bool event(QEvent *event) override {
//        if (event->type() == QEvent::Polish)
//        {
//            if (btrue) { mainWindow->newDocument();}
//        return true;
//        }
        if (event->type() == QEvent::FileOpen) {
            QFileOpenEvent *fileOpenEvent = static_cast<QFileOpenEvent *>(event);
            QString filePath = fileOpenEvent->file();
            if (!filePath.isEmpty() && mainWindow) { // Corrected from isValid() to isEmpty()
                mainWindow->openFileFromEvent(filePath);
                return true;
            }
        }

        return QApplication::event(event);
    }

private:
    MainWindow *mainWindow;
};

// Main function
int main(int argc, char *argv[]) {
    TextEditorApp app(argc, argv);
    MainWindow mainWindow;
    app.setMainWindow(&mainWindow);


    // If files are passed as command-line arguments, open them
    QStringList args = app.arguments();
qDebug() << args.size() ;

//if (args.size() >= 2) {
            for (int i = 1; i < args.size(); ++i) { // Skip the first argument (application path)
                mainWindow.openFileFromEvent(args.at(i));
            }

    //    }
    mainWindow.show();

    return app.exec();
}

#include "main.moc"
