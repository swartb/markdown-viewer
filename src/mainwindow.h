#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>

#include "theme.h"

class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QSplitter;
class QStackedWidget;
class QTextBrowser;
class QTimer;
class QToolButton;

class ElidedLabel;
class FileRowDelegate;
class PlaceRowDelegate;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const Theme &theme, bool persist = true, QWidget *parent = nullptr);
    ~MainWindow() override;

    void setInitialPath(const QString &path);
    bool openDir(const QString &dir);
    bool openFile(const QString &file);

    QString currentDir() const { return m_dir; }
    QString currentFile() const { return m_file; }
    int visibleItemCount() const;
    QString documentPlainText() const;
    bool isDocumentHeaderVisible() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void applyTheme(const Theme &theme);
    void watchTheme();
    void reloadTheme();

    bool navigateTo(const QString &dir, bool remember = true);
    void goUp();
    void goBack();
    void chooseDir();
    void refreshList();
    void populatePlaces();
    void updatePlacesSelection();
    void filterList(const QString &text);
    void updateBreadcrumb();
    void updateStatus();
    void updateDocumentHeader();
    void selectFileInList(const QString &path);
    void setSearchVisible(bool visible);

    void renderDocument();
    void styleDocument();
    void scaleImages();
    void handleAnchor(const QUrl &url);
    void showPlaceholder(const QString &text);

    QString prettyPath(const QString &path) const;
    static bool isMarkdown(const QString &path);

    Theme m_theme;
    bool m_persist = true;

    QToolButton *m_back = nullptr;
    QToolButton *m_up = nullptr;
    QToolButton *m_searchButton = nullptr;
    QToolButton *m_chooseButton = nullptr;
    ElidedLabel *m_crumb = nullptr;

    QStackedWidget *m_header = nullptr;
    QWidget *m_headerLabels = nullptr;
    QLabel *m_nameHeader = nullptr;
    QLabel *m_sizeHeader = nullptr;
    QWidget *m_headerSearch = nullptr;
    QLineEdit *m_search = nullptr;

    QListWidget *m_places = nullptr;
    PlaceRowDelegate *m_placesDelegate = nullptr;

    QListWidget *m_list = nullptr;
    FileRowDelegate *m_delegate = nullptr;

    QWidget *m_docHeader = nullptr;
    QLabel *m_docName = nullptr;
    QLabel *m_docMeta = nullptr;
    QTextBrowser *m_doc = nullptr;
    QSplitter *m_split = nullptr;

    QLabel *m_statusLeft = nullptr;
    QLabel *m_statusRight = nullptr;

    QFileSystemWatcher *m_themeWatcher = nullptr;
    QTimer *m_resizeTimer = nullptr;

    QString m_dir;
    QString m_file;
    QString m_body;
    QStringList m_history;
    bool m_syncingSelection = false;
    bool m_hasImages = false;
    bool m_searchVisible = false;
};

#endif // MAINWINDOW_H
