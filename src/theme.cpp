#include "theme.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace {

QString readValue(const QString &file, const QString &section, const QString &key)
{
    QFile handle(file);
    if (!handle.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&handle);
    QString current;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith('[') && line.endsWith(']')) {
            current = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        if (current != section || !line.startsWith(key))
            continue;
        const int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        QString value = line.mid(eq + 1).trimmed();
        if (value.startsWith('"'))
            value = value.mid(1, value.indexOf('"', 1) - 1);
        return value;
    }
    return QString();
}

QColor blend(const QColor &from, const QColor &to, qreal amount)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}

} // namespace

Theme Theme::load()
{
    Theme theme;
    // Terugval: Omarchy's donkergroene standaardthema.
    theme.background = QColor("#111c18");
    theme.darkBackground = QColor("#0c1512");
    theme.darkerBackground = QColor("#090f0d");
    theme.lighterBackground = QColor("#23372b");
    theme.foreground = QColor("#c1c497");
    theme.brightForeground = QColor("#f7e8b2");
    theme.muted = QColor("#53685b");
    theme.accent = QColor("#509475");
    theme.selection = QColor("#32473b");
    theme.green = QColor("#63b07a");
    theme.red = QColor("#ff5345");

    QFile file(colorsPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.startsWith('#') || !line.contains('='))
                continue;
            const int eq = line.indexOf('=');
            const QString key = line.left(eq).trimmed();
            QString value = line.mid(eq + 1).trimmed();
            value.remove('"');
            const QColor color(value);
            if (!color.isValid())
                continue;

            if (key == "background") theme.background = color;
            else if (key == "dark_background") theme.darkBackground = color;
            else if (key == "darker_background") theme.darkerBackground = color;
            else if (key == "lighter_background") theme.lighterBackground = color;
            else if (key == "foreground") theme.foreground = color;
            else if (key == "bright_foreground") theme.brightForeground = color;
            else if (key == "muted") theme.muted = color;
            else if (key == "accent") theme.accent = color;
            else if (key == "selection") theme.selection = color;
            else if (key == "green") theme.green = color;
            else if (key == "bright_green") theme.green = color;
            else if (key == "red") theme.red = color;
        }
    }

    // shell.toml: de tekstmaat waar Flea zijn hele schaal van afleidt.
    bool ok = false;
    const int base = readValue(shellPath(), QStringLiteral("font"), QStringLiteral("base-size")).toInt(&ok);
    if (ok && base > 0)
        theme.baseSize = base;
    theme.bodySize = theme.baseSize;
    theme.bodySmallSize = qMax(1, qRound(theme.baseSize * 0.917));
    theme.captionSize = qMax(1, qRound(theme.baseSize * 0.833));
    theme.titleSize = qMax(1, qRound(theme.baseSize * 1.167));
    theme.headingSize = qMax(1, qRound(theme.baseSize * 1.333));

    theme.rowHeight = qRound(theme.bodySmallSize * 1.8) + 2 * theme.controlPaddingY;
    theme.chromeHeight = qRound(theme.rowHeight * 0.72);
    theme.railRowHeight = qRound(theme.rowHeight * 0.78);

    theme.metadata = theme.foreground.darker(140);
    theme.rule = theme.foreground;
    theme.rule.setAlphaF(0.12);
    theme.normalFill = theme.foreground;
    theme.normalFill.setAlphaF(0.04);
    theme.hoverFill = theme.foreground;
    theme.hoverFill.setAlphaF(0.08);
    theme.cursorFill = theme.accent;
    theme.cursorFill.setAlphaF(0.18);
    theme.selectionFill = theme.foreground;
    theme.selectionFill.setAlphaF(0.35);
    theme.codeBackground = blend(theme.background, theme.foreground, 0.04);
    theme.surface = theme.darkBackground;
    theme.dark = theme.background.lightness() < 128;
    return theme;
}

QString Theme::colorsPath()
{
    return QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml";
}

QString Theme::shellPath()
{
    return QDir::homePath() + "/.local/state/omarchy/current/theme/shell.toml";
}

QFont Theme::font(int pixels, bool bold) const
{
    QFont result(QStringLiteral("monospace"));
    result.setPixelSize(pixels);
    result.setBold(bold);
    result.setHintingPreference(QFont::PreferFullHinting);
    return result;
}

QString Theme::cssColor(const QColor &color)
{
    if (color.alpha() == 255)
        return color.name();
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QString Theme::appStyleSheet() const
{
    const QString hover = blend(background, foreground, 0.08).name();
    const QString fill = blend(background, foreground, 0.04).name();
    const QString press = blend(background, foreground, 0.12).name();
    return QString(
        "QWidget { color: %1; }"
        "QMainWindow, QDialog, QMenu { background: %2; }"
        "QLabel { background: transparent; }"
        "#toolbar { background: %2; border-bottom: 1px solid %3; }"
        "#placesPane { background: %2; border-right: 1px solid %3; }"
        "#placesList { background: transparent; }"
        "#listHeader { background: %12; border-bottom: 1px solid %3; }"
        "#docHeader { background: %2; border-bottom: 1px solid %3; }"
        "#docPane { background: %2; }"
        "QToolTip { background: %4; color: %1; border: 1px solid %3; padding: 3px 6px; }"

        "QToolButton { background: transparent; border: 0; padding: 0; }"
        "QToolButton:hover { background: %5; }"
        "QToolButton:pressed { background: %6; }"

        "QPushButton { background: transparent; border: 1px solid %3; padding: 4px 10px; }"
        "QPushButton:hover { background: %5; }"

        "QLineEdit { background: transparent; border: 0; padding: 0;"
        " color: %1; selection-background-color: %7; selection-color: %2; }"

        "QListWidget { background: transparent; border: 0; outline: 0; }"
        "QListWidget::item { border: 0; }"
        "QStackedWidget { background: transparent; border: 0; }"

        "QSplitter::handle { background: %3; }"
        "QStatusBar { background: %2; border-top: 1px solid %3; }"
        "QStatusBar::item { border: 0; }"

        "QScrollBar:vertical { background: %13; width: 12px; margin: 0; border: 0; }"
        "QScrollBar::handle:vertical { background: %8; min-height: 24px; margin: 0 3px; border-radius: 0; }"
        "QScrollBar::handle:vertical:hover { background: %9; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: %13; height: 12px; margin: 0; border: 0; }"
        "QScrollBar::handle:horizontal { background: %8; min-width: 24px; margin: 3px 0; border-radius: 0; }"
        "QScrollBar::handle:horizontal:hover { background: %9; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; border: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"

        "QMenu { background: %4; border: 1px solid %3; }"
        "QMenu::item:selected { background: %5; }")
        .arg(foreground.name(),
             background.name(),
             cssColor(rule),
             surface.name(),
             hover,
             press,
             cssColor(selectionFill),
             cssColor(QColor(foreground.red(), foreground.green(), foreground.blue(), 128)),
             cssColor(QColor(foreground.red(), foreground.green(), foreground.blue(), 153)),
             fill,
             cssColor(QColor(foreground.red(), foreground.green(), foreground.blue(), 13)));
}

QString Theme::documentStyleSheet() const
{
    const QColor link = foreground;
    return QString(
        "body { color: %1; font-size: %2px; }"
        "p { margin-top: 0px; margin-bottom: 7px; }"
        "h1 { color: %3; font-size: %4px; }"
        "h2 { color: %3; font-size: %5px; }"
        "h3 { color: %3; font-size: %6px; }"
        "h4, h5, h6 { color: %3; font-size: %2px; }"
        "a { color: %7; text-decoration: underline; }"
        "code { font-family: 'JetBrainsMono Nerd Font', 'CaskaydiaMono Nerd Font', monospace;"
        " background-color: %8; color: %9; }"
        "pre { background-color: %8; color: %9;"
        " font-family: 'JetBrainsMono Nerd Font', 'CaskaydiaMono Nerd Font', monospace; }"
        "blockquote { color: %10; margin-left: 12px; }"
        "table { border-collapse: collapse; }"
        "th, td { border: 1px solid %11; padding: 3px 8px; }"
        "th { color: %3; }"
        "hr { color: %11; }")
        .arg(foreground.name())
        .arg(bodySize)
        .arg(brightForeground.name())
        .arg(headingSize)
        .arg(qRound(baseSize * 1.167))
        .arg(qRound(baseSize * 1.083))
        .arg(link.name())
        .arg(codeBackground.name())
        .arg(foreground.lighter(112).name())
        .arg(metadata.name())
        .arg(cssColor(rule));
}
