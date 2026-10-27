#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QFont>
#include <QString>

// Kleuren en maten van Omarchy, zoals Flea ze leest: colors.toml voor de
// kleuren, shell.toml voor de tekstmaat. Alles is monospace en vierkant.
class Theme
{
public:
    QColor background;
    QColor darkBackground;
    QColor darkerBackground;
    QColor lighterBackground;
    QColor foreground;
    QColor brightForeground;
    QColor muted;
    QColor accent;
    QColor selection;
    QColor green;
    QColor red;

    // Afgeleide inkten, net als ThemeColors.qml van Flea.
    QColor metadata;      // foreground donkerder gemaakt (1.4x)
    QColor rule;          // foreground op 12% voor scheidingslijnen
    QColor normalFill;    // foreground op 4%
    QColor hoverFill;     // foreground op 8%
    QColor cursorFill;    // accent op 18%
    QColor selectionFill; // foreground op 35%
    QColor codeBackground;
    QColor surface;

    // Tekstschaal, afgeleid van shell.toml [font] base-size.
    int baseSize = 12;
    int bodySize = 12;
    int bodySmallSize = 11;
    int captionSize = 10;
    int titleSize = 14;
    int headingSize = 16;

    // Vaste maten uit Flea's Theme.qml.
    int chromeHeight = 23;
    int rowHeight = 32;
    int railRowHeight = 25;
    int railWidth = 192;
    int hitMin = 24;
    int rowPaddingX = 12;
    int gap = 8;
    int controlPaddingY = 6;

    bool dark = true;

    static Theme load();
    static QString colorsPath();
    static QString shellPath();

    QFont font(int pixels, bool bold = false) const;
    QString appStyleSheet() const;
    QString documentStyleSheet() const;
    static QString cssColor(const QColor &color);
};

#endif // THEME_H
