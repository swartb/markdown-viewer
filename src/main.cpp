#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>

#include "mainwindow.h"
#include "theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("markdown-viewer"));
    QApplication::setApplicationDisplayName(QStringLiteral("Markdown viewer"));
    QApplication::setOrganizationName(QStringLiteral("bart"));
    QApplication::setDesktopFileName(QStringLiteral("markdown-viewer"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/markdown-viewer.svg")));

    QFont font = QApplication::font();
    font.setPointSizeF(10.5);
    QApplication::setFont(font);

    MainWindow window(Theme::load());
    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        window.setInitialPath(QFileInfo(args.at(1)).absoluteFilePath());
    window.show();
    return app.exec();
}
