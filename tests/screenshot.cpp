#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QToolButton>

#include "../src/mainwindow.h"
#include "../src/theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString target = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::homePath();
    const QString out = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("/tmp/markdown-viewer.png");

    MainWindow window(Theme::load(), false);
    window.resize(1180, 820);
    window.show();
    window.setInitialPath(target);
    app.processEvents();
    if (qEnvironmentVariableIsSet("MDV_SEARCH")) {
        for (QToolButton *button : window.findChildren<QToolButton *>()) {
            if (button->property("glyph").toString() == QLatin1String("search")) {
                button->click();
                break;
            }
        }
        if (auto *field = window.findChild<QLineEdit *>())
            field->setText(QStringLiteral("wel"));
        app.processEvents();
    }
    app.processEvents();
    window.grab().save(out);
    return 0;
}
