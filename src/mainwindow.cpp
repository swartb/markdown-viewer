#include "mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSharedPointer>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStorageInfo>
#include <QStyledItemDelegate>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextTable>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int PathRole = Qt::UserRole + 1;
constexpr int KindRole = Qt::UserRole + 2;
constexpr int SizeRole = Qt::UserRole + 3;
constexpr int IconRole = Qt::UserRole + 4;
constexpr int HeaderRole = Qt::UserRole + 5;
constexpr qint64 MaxFileSize = 5 * 1024 * 1024;

QString stripFrontMatter(const QString &markdown)
{
    static const QRegularExpression front(
        QStringLiteral("\\A\\x{FEFF}?---\\r?\\n[\\s\\S]*?\\r?\\n---[ \\t]*(?:\\r?\\n|$)"));
    QString body = markdown;
    body.remove(front);
    return body;
}

QString humanSize(qint64 bytes)
{
    if (bytes < 1000)
        return MainWindow::tr("%1 B").arg(bytes);
    static const char *units[] = {"kB", "MB", "GB", "TB"};
    double value = double(bytes);
    int unit = -1;
    while (value >= 1000.0 && unit < 3) {
        value /= 1000.0;
        ++unit;
    }
    const int decimals = value < 10.0 ? 1 : 0;
    return QStringLiteral("%1 %2").arg(value, 0, 'f', decimals).arg(QLatin1String(units[unit]));
}

QColor blend(const QColor &from, const QColor &to, qreal amount)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}

// Lucide-paden zoals Flea ze gebruikt (24-punts raster, streek 2, vierkante
// uiteinden). Ze worden als vector door QSvgRenderer getekend.
const QHash<QString, QString> &lucidePaths()
{
    static const QHash<QString, QString> paths = {
        {QStringLiteral("house"), QStringLiteral("M3 21V10l9-7 9 7v11h-6v-8H9v8H3z")},
        {QStringLiteral("download"), QStringLiteral("M12 15V3 M7 10l5 5 5-5 M3 15v6h18v-6")},
        {QStringLiteral("file-text"), QStringLiteral("M4 22V2h10l6 6v14H4z M14 2v6h6 M8 9h2 M8 13h8 M8 17h8")},
        {QStringLiteral("image"), QStringLiteral("M3 3h18v18H3z M7 9A2 2 0 1 0 11 9A2 2 0 1 0 7 9Z M21 15l-3-3L6 21")},
        {QStringLiteral("film"), QStringLiteral("M3 3h18v18H3z M7 3v18 M3 7.5h4 M3 12h18 M3 16.5h4 M17 3v18 M17 7.5h4 M17 16.5h4")},
        {QStringLiteral("music"), QStringLiteral("M9 18V5l12-2v13 M3 15h6v6H3z M15 13h6v6h-6z")},
        {QStringLiteral("folder"), QStringLiteral("M2 20V3h6l2 3h12v14H2z")},
        {QStringLiteral("folder-open"), QStringLiteral("M2 20V3h6l2 3h12v3 M22 11l-2.5 9H2l2.5-9z")},
        {QStringLiteral("folder-git-2"), QStringLiteral("M18 19a5 5 0 0 1-5-5v8 M9 20H2V3h6l2 2h12v5 M11 12A2 2 0 1 0 15 12A2 2 0 1 0 11 12Z M18 19A2 2 0 1 0 22 19A2 2 0 1 0 18 19Z")},
        {QStringLiteral("markdown"), QStringLiteral("M2 5h20v14H2z M5.5 15V9l3 3.5 3-3.5v6 M17 9v6 M14.5 12.5 17 15l2.5-2.5")},
        {QStringLiteral("search"), QStringLiteral("M16.7 16.7 21 21 M3 11A8 8 0 1 0 19 11A8 8 0 1 0 3 11Z")},
        {QStringLiteral("arrow-left"), QStringLiteral("M19 12H5 M12 19l-7-7 7-7")},
        {QStringLiteral("arrow-up"), QStringLiteral("M12 19V5 M5 12l7-7 7 7")},
    };
    return paths;
}

QSvgRenderer *lucideRenderer(const QString &name, const QColor &color)
{
    static QHash<QString, QSharedPointer<QSvgRenderer>> cache;
    const QString key = name + QLatin1Char('|') + color.name();
    auto it = cache.find(key);
    if (it != cache.end())
        return it.value().data();

    const QString path = lucidePaths().value(name, lucidePaths().value(QStringLiteral("file")));
    const QString svg = QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none'"
        " stroke='%1' stroke-width='2' stroke-linecap='square' stroke-linejoin='miter'>"
        "<path d='%2'/></svg>")
        .arg(color.name(), path);
    auto renderer = QSharedPointer<QSvgRenderer>::create(svg.toUtf8());
    cache.insert(key, renderer);
    return renderer.data();
}

void paintLucide(QPainter *painter, const QString &name, const QColor &color, const QRectF &box)
{
    QSvgRenderer *renderer = lucideRenderer(name, color);
    if (renderer)
        renderer->render(painter, box);
}

QPixmap lucidePixmap(const QString &name, const QColor &color, int pixels)
{
    const qreal ratio = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(qRound(pixels * ratio), qRound(pixels * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paintLucide(&painter, name, color, QRectF(0, 0, pixels, pixels));
    return pixmap;
}

QIcon lucideIcon(const QString &name, const QColor &color, int pixels)
{
    return QIcon(lucidePixmap(name, color, pixels));
}

} // namespace

// Padtekst die links afkapt, zoals Flea's chrome dat doet.
class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(40);
    }

    void setTextColor(const QColor &color)
    {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(m_color);
        const QString line = fontMetrics().elidedText(text(), Qt::ElideLeft, width());
        painter.drawText(rect(), Qt::AlignVCenter | Qt::AlignLeft, line);
    }

private:
    QColor m_color;
};

// Eén lijstrij: 32 px hoog, icoon in het tekstvak, naam en grootte zoals Flea.
class FileRowDelegate : public QStyledItemDelegate
{
public:
    explicit FileRowDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void setTheme(const Theme &theme) { m_theme = theme; }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(0, m_theme.rowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const QRect rect = option.rect;
        // De view tekent zelf geen achtergrond per rij; dat doen we hier.
        painter->fillRect(rect, m_theme.background);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (selected) {
            painter->fillRect(rect, m_theme.cursorFill);
            painter->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), m_theme.accent);
        } else if (hovered) {
            painter->fillRect(rect, m_theme.hoverFill);
        }

        const bool isDir = index.data(KindRole).toInt() == 1;
        const int iconPx = m_theme.rowHeight - 2 * m_theme.controlPaddingY - 1; // 19 bij basis 12
        const QRectF iconBox(rect.left() + m_theme.rowPaddingX,
                            rect.top() + (rect.height() - iconPx) / 2.0, iconPx, iconPx);
        paintLucide(painter, isDir ? QStringLiteral("folder") : QStringLiteral("file-text"),
                    m_theme.foreground, iconBox);

        const QString sizeText = index.data(SizeRole).toString();
        const QFont sizeFont = m_theme.font(m_theme.bodySmallSize);
        const QFontMetrics sizeMetrics(sizeFont);
        const int sizeWidth = sizeText.isEmpty() ? 0 : sizeMetrics.horizontalAdvance(sizeText);
        const int rightPad = m_theme.rowPaddingX;
        const int sizeLeft = rect.right() - rightPad - sizeWidth;
        const int nameLeft = int(iconBox.right()) + m_theme.gap;
        const int nameWidth = qMax(24, sizeLeft - nameLeft - m_theme.gap);

        painter->setFont(m_theme.font(m_theme.bodySize));
        painter->setPen(m_theme.foreground);
        const QFontMetrics nameMetrics(m_theme.font(m_theme.bodySize));
        painter->drawText(QRect(nameLeft, rect.top(), nameWidth, rect.height()),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          nameMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                 Qt::ElideMiddle, nameWidth));

        if (!sizeText.isEmpty()) {
            painter->setFont(sizeFont);
            painter->setPen(m_theme.metadata);
            painter->drawText(QRect(sizeLeft, rect.top(), sizeWidth, rect.height()),
                              Qt::AlignVCenter | Qt::AlignRight, sizeText);
        }
        painter->restore();
    }

private:
    Theme m_theme;
};

// Zijbalkrij: kop (PLEKKEN) en 25 px hoge plekken, met accentrand op de huidige.
class PlaceRowDelegate : public QStyledItemDelegate
{
public:
    explicit PlaceRowDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void setTheme(const Theme &theme) { m_theme = theme; }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        return QSize(0, index.data(HeaderRole).toBool() ? m_theme.chromeHeight : m_theme.railRowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect rect = option.rect;
        painter->fillRect(rect, m_theme.background);

        if (index.data(HeaderRole).toBool()) {
            QFont font = m_theme.font(m_theme.captionSize);
            font.setCapitalization(QFont::AllUppercase);
            font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            painter->setFont(font);
            painter->setPen(m_theme.muted);
            const QRect textRect(rect.left() + m_theme.rowPaddingX, rect.top() + 2,
                                 rect.width() - 2 * m_theme.rowPaddingX,
                                 rect.height() - 2 - m_theme.gap);
            painter->drawText(textRect, Qt::AlignBottom | Qt::AlignLeft,
                              index.data(Qt::DisplayRole).toString());
            painter->restore();
            return;
        }

        const bool current = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (current) {
            painter->fillRect(rect, m_theme.normalFill);
            painter->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), m_theme.accent);
        } else if (hovered) {
            painter->fillRect(rect, m_theme.hoverFill);
        }

        const int iconPx = m_theme.railRowHeight - 2 * m_theme.controlPaddingY + 1; // 13 bij basis 12
        const QRectF iconBox(rect.left() + m_theme.rowPaddingX,
                            rect.top() + (rect.height() - iconPx) / 2.0, iconPx, iconPx);
        paintLucide(painter, index.data(IconRole).toString(), m_theme.foreground, iconBox);

        painter->setFont(m_theme.font(m_theme.bodySize));
        painter->setPen(m_theme.foreground);
        const int labelLeft = int(iconBox.right()) + m_theme.gap;
        const int labelWidth = rect.right() - m_theme.rowPaddingX - labelLeft;
        const QFontMetrics metrics(m_theme.font(m_theme.bodySize));
        painter->drawText(QRect(labelLeft, rect.top(), labelWidth, rect.height()),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          metrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                             Qt::ElideRight, labelWidth));
        painter->restore();
    }

private:
    Theme m_theme;
};

MainWindow::MainWindow(const Theme &theme, bool persist, QWidget *parent)
    : QMainWindow(parent)
    , m_theme(theme)
    , m_persist(persist)
{
    qApp->setFont(m_theme.font(m_theme.bodySize));
    buildUi();
    watchTheme();
    applyTheme(m_theme);

    resize(1200, 800);
    setMinimumSize(860, 480);
    setWindowTitle(tr("Markdown viewer"));

    if (m_persist) {
        QSettings settings;
        const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
        const QByteArray splitter = settings.value(QStringLiteral("splitter")).toByteArray();
        if (!splitter.isEmpty())
            m_split->restoreState(splitter);
    }

    QString start = QDir::homePath();
    QString startFile;
    if (m_persist) {
        QSettings settings;
        const QString lastDir = settings.value(QStringLiteral("lastDir")).toString();
        if (!lastDir.isEmpty() && QFileInfo(lastDir).isDir())
            start = lastDir;
        startFile = settings.value(QStringLiteral("lastFile")).toString();
    }
    navigateTo(start, false);
    if (!startFile.isEmpty() && QFileInfo(startFile).isFile()
        && startFile.startsWith(m_dir + QLatin1Char('/')))
        openFile(startFile);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setCentralWidget(central);

    // Bovenbalk: terug, omhoog, pad, zoeken, map kiezen.
    auto *toolbar = new QWidget(central);
    toolbar->setObjectName(QStringLiteral("toolbar"));
    toolbar->setFixedHeight(m_theme.chromeHeight);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(m_theme.rowPaddingX, 0, m_theme.rowPaddingX, 0);
    toolbarLayout->setSpacing(m_theme.gap);

    const auto makeChromeButton = [this, toolbar](const QString &glyph, const QString &tip) {
        auto *button = new QToolButton(toolbar);
        button->setAutoRaise(true);
        button->setFixedSize(m_theme.hitMin, m_theme.chromeHeight);
        button->setIconSize(QSize(14, 14));
        button->setToolTip(tip);
        button->setProperty("glyph", glyph);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };

    m_back = makeChromeButton(QStringLiteral("arrow-left"), tr("Terug (Alt+←)"));
    m_up = makeChromeButton(QStringLiteral("arrow-up"), tr("Eén map omhoog (Alt+↑)"));

    m_crumb = new ElidedLabel(toolbar);
    m_crumb->setFont(m_theme.font(m_theme.captionSize));
    m_crumb->setMinimumWidth(60);

    m_searchButton = makeChromeButton(QStringLiteral("search"), tr("Zoeken (Ctrl+F)"));
    m_chooseButton = makeChromeButton(QStringLiteral("folder-open"), tr("Map kiezen… (Ctrl+O)"));

    toolbarLayout->addWidget(m_back);
    toolbarLayout->addWidget(m_up);
    toolbarLayout->addWidget(m_crumb, 1);
    toolbarLayout->addWidget(m_searchButton);
    toolbarLayout->addWidget(m_chooseButton);
    layout->addWidget(toolbar);

    // Zijbalk met plekken.
    auto *placesPane = new QWidget(central);
    placesPane->setObjectName(QStringLiteral("placesPane"));
    placesPane->setFixedWidth(m_theme.railWidth);
    auto *placesLayout = new QVBoxLayout(placesPane);
    placesLayout->setContentsMargins(0, m_theme.rowPaddingX, 0, m_theme.rowPaddingX);
    placesLayout->setSpacing(0);

    m_places = new QListWidget(placesPane);
    m_places->setObjectName(QStringLiteral("placesList"));
    m_places->setFrameShape(QFrame::NoFrame);
    m_places->setFocusPolicy(Qt::NoFocus);
    m_places->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_places->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_places->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_places->setMouseTracking(true);
    m_places->viewport()->setAttribute(Qt::WA_Hover, true);
    m_placesDelegate = new PlaceRowDelegate(m_places);
    m_places->setItemDelegate(m_placesDelegate);
    placesLayout->addWidget(m_places, 1);
    populatePlaces();

    // Lijst met mappen en markdown-bestanden, met een kop die bij zoeken een veld wordt.
    auto *listPane = new QWidget(central);
    listPane->setObjectName(QStringLiteral("listPane"));
    auto *listLayout = new QVBoxLayout(listPane);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    m_header = new QStackedWidget(listPane);
    m_header->setFixedHeight(m_theme.chromeHeight);
    m_header->setObjectName(QStringLiteral("listHeader"));

    m_headerLabels = new QWidget(m_header);
    auto *labelsLayout = new QHBoxLayout(m_headerLabels);
    labelsLayout->setContentsMargins(m_theme.rowPaddingX, 0, m_theme.rowPaddingX, 0);
    labelsLayout->setSpacing(0);
    m_nameHeader = new QLabel(tr("Naam"), m_headerLabels);
    m_sizeHeader = new QLabel(tr("Grootte"), m_headerLabels);
    m_nameHeader->setFont(m_theme.font(m_theme.captionSize));
    m_sizeHeader->setFont(m_theme.font(m_theme.captionSize));
    labelsLayout->addWidget(m_nameHeader, 1);
    labelsLayout->addWidget(m_sizeHeader);

    m_headerSearch = new QWidget(m_header);
    auto *searchLayout = new QHBoxLayout(m_headerSearch);
    searchLayout->setContentsMargins(m_theme.rowPaddingX, 0, m_theme.rowPaddingX, 0);
    searchLayout->setSpacing(m_theme.gap);
    auto *searchMark = new QLabel(m_headerSearch);
    searchMark->setPixmap(lucidePixmap(QStringLiteral("search"), m_theme.muted, 13));
    searchMark->setFixedSize(13, 13);
    m_search = new QLineEdit(m_headerSearch);
    m_search->setFont(m_theme.font(m_theme.bodySize));
    m_search->setPlaceholderText(tr("Zoeken…"));
    m_search->setFrame(false);
    auto *escHint = new QLabel(QStringLiteral("esc"), m_headerSearch);
    escHint->setFont(m_theme.font(m_theme.captionSize));
    escHint->setPalette([this]() {
        QPalette palette = qApp->palette();
        palette.setColor(QPalette::WindowText, m_theme.muted);
        return palette;
    }());
    searchLayout->addWidget(searchMark);
    searchLayout->addWidget(m_search, 1);
    searchLayout->addWidget(escHint);

    m_header->addWidget(m_headerLabels);
    m_header->addWidget(m_headerSearch);
    m_header->setCurrentIndex(0);
    listLayout->addWidget(m_header);

    m_list = new QListWidget(listPane);
    m_list->setObjectName(QStringLiteral("fileList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->viewport()->setAttribute(Qt::WA_Hover, true);
    m_delegate = new FileRowDelegate(m_list);
    m_list->setItemDelegate(m_delegate);
    listLayout->addWidget(m_list, 1);

    // Documentkolom.
    auto *docPane = new QWidget(central);
    docPane->setObjectName(QStringLiteral("docPane"));
    auto *docLayout = new QVBoxLayout(docPane);
    docLayout->setContentsMargins(0, 0, 0, 0);
    docLayout->setSpacing(0);

    m_docHeader = new QWidget(docPane);
    m_docHeader->setObjectName(QStringLiteral("docHeader"));
    m_docHeader->setFixedHeight(m_theme.chromeHeight);
    auto *docHeaderLayout = new QHBoxLayout(m_docHeader);
    docHeaderLayout->setContentsMargins(m_theme.rowPaddingX, 0, m_theme.rowPaddingX, 0);
    docHeaderLayout->setSpacing(m_theme.gap);
    m_docName = new QLabel(m_docHeader);
    m_docName->setFont(m_theme.font(m_theme.bodySmallSize));
    m_docMeta = new QLabel(m_docHeader);
    m_docMeta->setFont(m_theme.font(m_theme.captionSize));
    docHeaderLayout->addWidget(m_docName);
    docHeaderLayout->addWidget(m_docMeta, 1, Qt::AlignRight);
    m_docHeader->hide();
    docLayout->addWidget(m_docHeader);

    m_doc = new QTextBrowser(docPane);
    m_doc->setFrameShape(QFrame::NoFrame);
    m_doc->setOpenLinks(false);
    m_doc->setOpenExternalLinks(false);
    m_doc->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_doc->setFont(m_theme.font(m_theme.bodySize));
    m_doc->document()->setDocumentMargin(m_theme.rowPaddingX + 2);
    docLayout->addWidget(m_doc, 1);

    m_split = new QSplitter(Qt::Horizontal, central);
    m_split->addWidget(listPane);
    m_split->addWidget(docPane);
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(1);
    m_split->setStretchFactor(0, 0);
    m_split->setStretchFactor(1, 1);
    m_split->setSizes({430, 570});
    listPane->setMinimumWidth(240);
    docPane->setMinimumWidth(280);

    auto *body = new QWidget(central);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(placesPane);
    bodyLayout->addWidget(m_split, 1);
    layout->addWidget(body, 1);

    // Statusbalk.
    m_statusLeft = new QLabel(this);
    m_statusRight = new QLabel(this);
    m_statusLeft->setFont(m_theme.font(m_theme.captionSize));
    m_statusRight->setFont(m_theme.font(m_theme.captionSize));
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setFixedHeight(m_theme.chromeHeight);
    statusBar()->setContentsMargins(m_theme.rowPaddingX, 0, m_theme.rowPaddingX, 0);
    statusBar()->addWidget(m_statusLeft);
    statusBar()->addPermanentWidget(m_statusRight);

    connect(m_back, &QToolButton::clicked, this, &MainWindow::goBack);
    connect(m_up, &QToolButton::clicked, this, &MainWindow::goUp);
    connect(m_chooseButton, &QToolButton::clicked, this, &MainWindow::chooseDir);
    connect(m_searchButton, &QToolButton::clicked, this, [this]() {
        setSearchVisible(!m_searchVisible);
    });
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::filterList);
    connect(m_search, &QLineEdit::returnPressed, this, [this]() {
        for (int row = 0; row < m_list->count(); ++row) {
            if (!m_list->isRowHidden(row)) {
                m_list->setCurrentRow(row);
                m_list->setFocus(Qt::ShortcutFocusReason);
                break;
            }
        }
    });
    connect(m_doc, &QTextBrowser::anchorClicked, this, &MainWindow::handleAnchor);

    connect(m_places, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item->data(HeaderRole).toBool())
            return;
        navigateTo(item->data(PathRole).toString());
    });

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        if (m_syncingSelection || !current)
            return;
        if (current->data(KindRole).toInt() == 1)
            return;
        openFile(current->data(PathRole).toString());
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item->data(KindRole).toInt() == 1)
            navigateTo(item->data(PathRole).toString());
    });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item->data(KindRole).toInt() == 1)
            navigateTo(item->data(PathRole).toString());
        else
            openFile(item->data(PathRole).toString());
    });

    auto addShortcut = [this](const QKeySequence &sequence, auto slot) {
        auto *shortcut = new QShortcut(sequence, this);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    addShortcut(QKeySequence(QStringLiteral("Alt+Left")), &MainWindow::goBack);
    addShortcut(QKeySequence(QStringLiteral("Alt+Up")), &MainWindow::goUp);
    addShortcut(QKeySequence(QStringLiteral("Ctrl+O")), &MainWindow::chooseDir);
    addShortcut(QKeySequence::Find, [this]() { setSearchVisible(true); });
    addShortcut(QKeySequence::Quit, &QWidget::close);
    addShortcut(QKeySequence(Qt::Key_Escape), [this]() {
        if (m_searchVisible)
            setSearchVisible(false);
        else
            m_list->setFocus(Qt::ShortcutFocusReason);
    });

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(220);
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() {
        if (m_hasImages && !m_file.isEmpty()) {
            const int scroll = m_doc->verticalScrollBar()->value();
            renderDocument();
            m_doc->verticalScrollBar()->setValue(scroll);
        }
    });

    m_list->setFocus();
}

void MainWindow::applyTheme(const Theme &theme)
{
    m_theme = theme;
    qApp->setStyleSheet(theme.appStyleSheet());
    qApp->setFont(theme.font(theme.bodySize));

    m_delegate->setTheme(theme);
    m_placesDelegate->setTheme(theme);
    m_places->setFixedWidth(theme.railWidth);
    if (auto *pane = m_places->parentWidget())
        pane->setFixedWidth(theme.railWidth);

    m_crumb->setFont(theme.font(theme.captionSize));
    m_crumb->setTextColor(theme.foreground);

    const auto chromeButtonIcon = [&theme](QToolButton *button) {
        button->setIcon(lucideIcon(button->property("glyph").toString(), theme.foreground, 14));
        button->setIconSize(QSize(14, 14));
        button->setFixedSize(theme.hitMin, theme.chromeHeight);
    };
    chromeButtonIcon(m_back);
    chromeButtonIcon(m_up);
    chromeButtonIcon(m_chooseButton);
    m_searchButton->setIcon(lucideIcon(QStringLiteral("search"),
                                      m_searchVisible ? theme.accent : theme.foreground, 14));
    m_searchButton->setFixedSize(theme.hitMin, theme.chromeHeight);

    m_nameHeader->setFont(theme.font(theme.captionSize));
    m_sizeHeader->setFont(theme.font(theme.captionSize));
    m_docName->setFont(theme.font(theme.bodySmallSize));
    m_docMeta->setFont(theme.font(theme.captionSize));
    m_statusLeft->setFont(theme.font(theme.captionSize));
    m_statusRight->setFont(theme.font(theme.captionSize));
    m_search->setFont(theme.font(theme.bodySize));

    const auto setInk = [](QLabel *label, const QColor &color) {
        label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(color.name()));
    };
    setInk(m_nameHeader, theme.muted);
    setInk(m_sizeHeader, theme.muted);
    setInk(m_docMeta, theme.muted);
    setInk(m_statusRight, theme.muted);
    setInk(m_statusLeft, theme.foreground);
    setInk(m_docName, theme.foreground);

    QPalette searchPalette = m_search->palette();
    searchPalette.setColor(QPalette::PlaceholderText, theme.muted);
    searchPalette.setColor(QPalette::Text, theme.foreground);
    m_search->setPalette(searchPalette);

    if (auto *toolbar = findChild<QWidget *>(QStringLiteral("toolbar")))
        toolbar->setFixedHeight(theme.chromeHeight);
    if (auto *placesPane = findChild<QWidget *>(QStringLiteral("placesPane")))
        placesPane->setFixedWidth(theme.railWidth);
    m_header->setFixedHeight(theme.chromeHeight);
    m_docHeader->setFixedHeight(theme.chromeHeight);
    statusBar()->setFixedHeight(theme.chromeHeight);
    m_search->setFont(theme.font(theme.bodySize));
    m_doc->setFont(theme.font(theme.bodySize));

    QPalette palette = m_doc->palette();
    palette.setColor(QPalette::Base, theme.background);
    palette.setColor(QPalette::Text, theme.foreground);
    palette.setColor(QPalette::Highlight, theme.selection);
    palette.setColor(QPalette::HighlightedText, theme.brightForeground);
    m_doc->setPalette(palette);
    m_doc->viewport()->setPalette(palette);
    m_list->viewport()->setPalette(palette);
    m_places->viewport()->setPalette(palette);

    if (!m_file.isEmpty())
        renderDocument();

    updateBreadcrumb();
    updateStatus();
    updatePlacesSelection();
}

void MainWindow::watchTheme()
{
    if (!m_themeWatcher) {
        m_themeWatcher = new QFileSystemWatcher(this);
        connect(m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
            QTimer::singleShot(120, this, &MainWindow::reloadTheme);
        });
    }
    for (const QString &path : {Theme::colorsPath(), Theme::shellPath()}) {
        if (!m_themeWatcher->files().contains(path) && QFile::exists(path))
            m_themeWatcher->addPath(path);
    }
}

void MainWindow::reloadTheme()
{
    applyTheme(Theme::load());
    watchTheme();
}

void MainWindow::setInitialPath(const QString &path)
{
    if (path.isEmpty())
        return;
    const QFileInfo info(path);
    if (info.isDir()) {
        navigateTo(info.absoluteFilePath());
    } else if (info.isFile()) {
        navigateTo(info.absolutePath());
        openFile(info.absoluteFilePath());
    }
}

bool MainWindow::openDir(const QString &dir)
{
    return navigateTo(dir);
}

bool MainWindow::openFile(const QString &file)
{
    const QFileInfo info(file);
    if (!info.isFile() || !isMarkdown(file)) {
        showPlaceholder(tr("Dit is geen leesbaar markdown-bestand."));
        return false;
    }
    if (info.size() > MaxFileSize) {
        showPlaceholder(tr("Dit bestand is te groot om te tonen (max 5 MB)."));
        return false;
    }
    QFile handle(info.absoluteFilePath());
    if (!handle.open(QIODevice::ReadOnly)) {
        showPlaceholder(tr("Kan dit bestand niet lezen: %1").arg(handle.errorString()));
        return false;
    }
    const QString markdown = QString::fromUtf8(handle.readAll());
    handle.close();

    const QString absolute = info.absoluteFilePath();
    if (info.absolutePath() != m_dir)
        navigateTo(info.absolutePath());

    m_file = absolute;
    m_body = stripFrontMatter(markdown);
    renderDocument();
    updateDocumentHeader();
    selectFileInList(absolute);
    updateStatus();

    if (m_persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("lastFile"), absolute);
    }
    return true;
}

void MainWindow::showPlaceholder(const QString &text)
{
    QTextDocument *doc = m_doc->document();
    doc->setDefaultStyleSheet(m_theme.documentStyleSheet());
    doc->setDocumentMargin(m_theme.rowPaddingX + 2);
    m_doc->setHtml(QStringLiteral("<p style=\"color:%1\">%2</p>")
                       .arg(m_theme.metadata.name(), text.toHtmlEscaped()));
    m_hasImages = false;
}

void MainWindow::renderDocument()
{
    if (m_file.isEmpty()) {
        showPlaceholder(tr("Kies een markdown-bestand uit de lijst."));
        return;
    }
    QTextDocument *doc = m_doc->document();
    doc->setDefaultFont(m_theme.font(m_theme.bodySize));
    doc->setDefaultStyleSheet(m_theme.documentStyleSheet());
    doc->setBaseUrl(QUrl::fromLocalFile(QFileInfo(m_file).absolutePath() + QLatin1Char('/')));
    doc->setMarkdown(m_body, QTextDocument::MarkdownDialectGitHub);
    doc->setDocumentMargin(m_theme.rowPaddingX + 2);
    m_hasImages = false;
    scaleImages();
    styleDocument();
    m_doc->verticalScrollBar()->setValue(0);
}

void MainWindow::scaleImages()
{
    QTextDocument *doc = m_doc->document();
    const int available = m_doc->viewport()->width() - int(doc->documentMargin() * 2) - 8;
    if (available < 160)
        return;

    struct Replacement {
        int position;
        int length;
        QTextImageFormat format;
    };
    QVector<Replacement> replacements;

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            const QTextCharFormat charFormat = fragment.charFormat();
            if (!charFormat.isImageFormat())
                continue;

            m_hasImages = true;
            QTextImageFormat image = charFormat.toImageFormat();
            QUrl source(image.name());
            if (source.isRelative())
                source = doc->baseUrl().resolved(source);

            QImageReader reader(source.toLocalFile());
            const QSize natural = reader.size();
            if (!natural.isValid() || natural.width() <= available)
                continue;

            const qreal factor = qreal(available) / qreal(natural.width());
            image.setWidth(available);
            image.setHeight(qMax(1, int(natural.height() * factor)));
            replacements.append({fragment.position(), fragment.length(), image});
        }
    }

    for (int i = replacements.size() - 1; i >= 0; --i) {
        const Replacement &replacement = replacements.at(i);
        QTextCursor cursor(doc);
        cursor.setPosition(replacement.position);
        cursor.setPosition(replacement.position + replacement.length, QTextCursor::KeepAnchor);
        cursor.insertImage(replacement.format);
    }
}

void MainWindow::styleDocument()
{
    QTextDocument *doc = m_doc->document();
    const QColor border = blend(m_theme.background, m_theme.foreground, 0.12);

    // Koppen: Qt's markdown-import zet zelf geen buitenmarges.
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level <= 0)
            continue;
        QTextCursor cursor(block);
        QTextBlockFormat format = block.blockFormat();
        format.setTopMargin(level == 1 ? 10 : (level == 2 ? 16 : 12));
        format.setBottomMargin(level == 1 ? 8 : 5);
        cursor.setBlockFormat(format);
    }

    QVector<QTextBlock> codeBlocks;
    struct InlineRange {
        int position;
        int length;
        QTextCharFormat format;
    };
    QVector<InlineRange> inlineRanges;

    const auto isFixedPitch = [this](const QTextCharFormat &format) {
        // De markdown-import zet code op een vaste letterbreedte; het thema kan
        // de familie hebben vervangen, dus ook op familienaam letten.
        return format.fontFixedPitch()
            || format.fontFamilies().toStringList().join(QLatin1Char(',')).contains(
                QLatin1String("mono"), Qt::CaseInsensitive);
    };

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        const QString text = block.text();
        if (text.trimmed().isEmpty())
            continue;

        bool allFixedPitch = true;
        bool anyFixedPitch = false;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.text().trimmed().isEmpty())
                continue;
            const bool fixedPitch = isFixedPitch(fragment.charFormat());
            anyFixedPitch = anyFixedPitch || fixedPitch;
            if (!fixedPitch)
                allFixedPitch = false;
        }
        if (allFixedPitch && anyFixedPitch) {
            codeBlocks.append(block);
        } else if (anyFixedPitch) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                if (!fragment.isValid())
                    continue;
                QTextCharFormat format = fragment.charFormat();
                if (!isFixedPitch(format))
                    continue;
                format.setBackground(m_theme.codeBackground);
                inlineRanges.append({fragment.position(), fragment.length(), format});
            }
        }
    }

    // Aaneengesloten regels van één codeblok delen de achtergrond.
    for (int i = 0; i < codeBlocks.size(); ++i) {
        const QTextBlock block = codeBlocks.at(i);
        const bool firstOfRun = i == 0 || codeBlocks.at(i - 1).blockNumber() != block.blockNumber() - 1;
        const bool lastOfRun = i == codeBlocks.size() - 1
            || codeBlocks.at(i + 1).blockNumber() != block.blockNumber() + 1;
        QTextCursor cursor(block);
        QTextBlockFormat format = block.blockFormat();
        format.setBackground(m_theme.codeBackground);
        format.setLeftMargin(10);
        format.setRightMargin(10);
        format.setTopMargin(firstOfRun ? 8 : 0);
        format.setBottomMargin(lastOfRun ? 8 : 0);
        cursor.setBlockFormat(format);
    }

    for (int i = inlineRanges.size() - 1; i >= 0; --i) {
        const InlineRange &range = inlineRanges.at(i);
        QTextCursor cursor(doc);
        cursor.setPosition(range.position);
        cursor.setPosition(range.position + range.length, QTextCursor::KeepAnchor);
        cursor.setCharFormat(range.format);
    }

    // Tabellen: dunne lijnen, weinig padding, kopregel gelicht.
    for (QTextFrame *frame : doc->rootFrame()->childFrames()) {
        auto *table = qobject_cast<QTextTable *>(frame);
        if (!table)
            continue;
        QTextTableFormat format = table->format();
        format.setBorder(1);
        format.setBorderBrush(QBrush(border));
        format.setBorderCollapse(true);
        format.setCellPadding(3);
        format.setCellSpacing(0);
        table->setFormat(format);
        if (table->rows() > 0) {
            for (int column = 0; column < table->columns(); ++column) {
                QTextTableCell cell = table->cellAt(0, column);
                QTextCharFormat cellFormat = cell.format();
                cellFormat.setBackground(m_theme.codeBackground);
                cellFormat.setFontWeight(QFont::DemiBold);
                cellFormat.setForeground(QBrush(m_theme.brightForeground));
                cell.setFormat(cellFormat);
            }
        }
    }
}

void MainWindow::handleAnchor(const QUrl &url)
{
    if (url.scheme() == QLatin1String("file")) {
        const QString local = url.toLocalFile();
        if (!url.fragment().isEmpty() && local == m_file) {
            m_doc->scrollToAnchor(url.fragment());
            return;
        }
        if (isMarkdown(local)) {
            openFile(local);
            return;
        }
        QDesktopServices::openUrl(url);
        return;
    }
    if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")
        || url.scheme() == QLatin1String("mailto")) {
        QDesktopServices::openUrl(url);
    }
}

bool MainWindow::navigateTo(const QString &dir, bool remember)
{
    const QFileInfo info(dir);
    if (!info.isDir())
        return false;

    const QString absolute = info.absoluteFilePath();
    if (remember && !m_dir.isEmpty() && absolute != m_dir) {
        m_history.append(m_dir);
        if (m_history.size() > 50)
            m_history.removeFirst();
    }

    m_dir = absolute;
    if (m_persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("lastDir"), absolute);
    }

    if (m_file.isEmpty() || !m_file.startsWith(m_dir + QLatin1Char('/'))) {
        m_file.clear();
        m_body.clear();
        m_docHeader->hide();
    }

    refreshList();
    updateBreadcrumb();
    updatePlacesSelection();
    updateStatus();
    if (m_file.isEmpty())
        showPlaceholder(tr("Kies een markdown-bestand uit de lijst."));

    if (m_persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("lastFile"), m_file);
    }
    return true;
}

void MainWindow::goUp()
{
    const QFileInfo info(m_dir);
    const QString parent = info.absolutePath();
    if (!parent.isEmpty() && parent != m_dir)
        navigateTo(parent);
}

void MainWindow::goBack()
{
    if (m_history.isEmpty())
        return;
    const QString previous = m_history.takeLast();
    if (QFileInfo(previous).isDir())
        navigateTo(previous, false);
    updateBreadcrumb();
    updateStatus();
}

void MainWindow::chooseDir()
{
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Map kiezen"), m_dir, QFileDialog::ShowDirsOnly);
    if (!chosen.isEmpty())
        navigateTo(chosen);
}

void MainWindow::refreshList()
{
    m_syncingSelection = true;
    m_list->clear();

    QDir dir(m_dir);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::NoSort);

    QFileInfoList dirs;
    QFileInfoList files;
    for (const QFileInfo &entry : entries) {
        if (entry.fileName().startsWith(QLatin1Char('.')))
            continue;
        if (entry.isDir())
            dirs.append(entry);
        else if (entry.isFile() && isMarkdown(entry.fileName()))
            files.append(entry);
    }
    const auto byName = [](const QFileInfo &a, const QFileInfo &b) {
        return QString::localeAwareCompare(a.fileName(), b.fileName()) < 0;
    };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    const auto addItem = [this](const QFileInfo &info, bool isDir) {
        auto *item = new QListWidgetItem(info.fileName());
        item->setData(PathRole, info.absoluteFilePath());
        item->setData(KindRole, isDir ? 1 : 0);
        item->setData(SizeRole, isDir ? QString() : humanSize(info.size()));
        item->setToolTip(info.absoluteFilePath());
        m_list->addItem(item);
        return item;
    };
    for (const QFileInfo &entry : dirs)
        addItem(entry, true);
    for (const QFileInfo &entry : files)
        addItem(entry, false);

    if (!m_file.isEmpty()) {
        for (int row = 0; row < m_list->count(); ++row) {
            QListWidgetItem *item = m_list->item(row);
            if (item->data(PathRole).toString() == m_file) {
                m_list->setCurrentItem(item);
                m_list->scrollToItem(item, QAbstractItemView::EnsureVisible);
                break;
            }
        }
    }
    m_syncingSelection = false;
    filterList(m_search->text());
}

void MainWindow::populatePlaces()
{
    const QString home = QDir::homePath();
    struct Place {
        QString name;
        QString path;
        QString icon;
    };
    const QVector<Place> places = {
        {tr("Home"), home, QStringLiteral("house")},
        {tr("Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), QStringLiteral("download")},
        {tr("Documenten"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), QStringLiteral("file-text")},
        {tr("Muziek"), QStandardPaths::writableLocation(QStandardPaths::MusicLocation), QStringLiteral("music")},
        {tr("Afbeeldingen"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), QStringLiteral("image")},
        {tr("Video's"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), QStringLiteral("film")},
        {tr("Projecten"), home + QStringLiteral("/Projects"), QStringLiteral("folder-git-2")},
    };

    m_places->clear();
    auto *header = new QListWidgetItem(tr("Plekken"), m_places);
    header->setData(HeaderRole, true);
    header->setFlags(Qt::NoItemFlags);

    for (const Place &place : places) {
        if (place.path.isEmpty() || !QFileInfo(place.path).isDir())
            continue;
        auto *item = new QListWidgetItem(place.name, m_places);
        item->setData(PathRole, place.path);
        item->setData(IconRole, place.icon);
        item->setToolTip(prettyPath(place.path));
    }
}

void MainWindow::updatePlacesSelection()
{
    QString bestPath;
    QListWidgetItem *best = nullptr;
    for (int row = 0; row < m_places->count(); ++row) {
        QListWidgetItem *item = m_places->item(row);
        if (item->data(HeaderRole).toBool())
            continue;
        const QString path = item->data(PathRole).toString();
        if ((m_dir == path || m_dir.startsWith(path + QLatin1Char('/'))) && path.size() > bestPath.size()) {
            bestPath = path;
            best = item;
        }
    }
    m_places->setCurrentItem(best);
}

void MainWindow::filterList(const QString &text)
{
    const QString needle = text.trimmed();
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const bool match = needle.isEmpty()
            || item->data(Qt::DisplayRole).toString().contains(needle, Qt::CaseInsensitive);
        m_list->setRowHidden(row, !match);
    }
    updateStatus();
}

void MainWindow::selectFileInList(const QString &path)
{
    m_syncingSelection = true;
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        if (item->data(PathRole).toString() == path) {
            m_list->setCurrentItem(item);
            m_list->scrollToItem(item, QAbstractItemView::EnsureVisible);
            break;
        }
    }
    m_syncingSelection = false;
}

void MainWindow::setSearchVisible(bool visible)
{
    m_searchVisible = visible;
    m_header->setCurrentIndex(visible ? 1 : 0);
    m_searchButton->setIcon(lucideIcon(QStringLiteral("search"),
                                      visible ? m_theme.accent : m_theme.foreground, 14));
    if (visible) {
        m_search->setFocus(Qt::ShortcutFocusReason);
        m_search->selectAll();
    } else {
        m_search->clear();
        m_list->setFocus(Qt::ShortcutFocusReason);
    }
}

void MainWindow::updateBreadcrumb()
{
    m_crumb->setText(prettyPath(m_dir));
    m_crumb->setToolTip(m_dir);
}

QString MainWindow::prettyPath(const QString &path) const
{
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QStringLiteral("~") + path.mid(home.size());
    return path;
}

void MainWindow::updateStatus()
{
    int visible = 0;
    for (int row = 0; row < m_list->count(); ++row) {
        if (!m_list->isRowHidden(row))
            ++visible;
    }

    if (!m_file.isEmpty()) {
        const QFileInfo info(m_file);
        m_statusLeft->setText(tr("1 van %1 geselecteerd · %2").arg(visible).arg(humanSize(info.size())));
    } else {
        m_statusLeft->setText(visible == 1 ? tr("1 item") : tr("%1 items").arg(visible));
    }

    const QStorageInfo storage(m_dir);
    if (storage.isValid() && storage.isReady()) {
        m_statusRight->setText(QStringLiteral("%1 · %2 %3")
                                   .arg(storage.fileSystemType(),
                                        humanSize(storage.bytesAvailable()),
                                        tr("vrij")));
    } else {
        m_statusRight->setText(prettyPath(m_dir));
    }

    m_back->setEnabled(!m_history.isEmpty());
    const QFileInfo info(m_dir);
    m_up->setEnabled(!m_dir.isEmpty() && info.absolutePath() != m_dir);
}

void MainWindow::updateDocumentHeader()
{
    if (m_file.isEmpty()) {
        m_docHeader->hide();
        return;
    }
    const QFileInfo info(m_file);
    m_docName->setText(info.fileName());
    m_docMeta->setText(QStringLiteral("%1 · %2").arg(prettyPath(info.absolutePath()), humanSize(info.size())));
    m_docMeta->setToolTip(m_file);
    m_docHeader->show();
}

int MainWindow::visibleItemCount() const
{
    int visible = 0;
    for (int row = 0; row < m_list->count(); ++row) {
        if (!m_list->isRowHidden(row))
            ++visible;
    }
    return visible;
}

QString MainWindow::documentPlainText() const
{
    return m_doc->toPlainText();
}

bool MainWindow::isDocumentHeaderVisible() const
{
    return !m_file.isEmpty();
}

bool MainWindow::isMarkdown(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("md") || suffix == QLatin1String("markdown")
        || suffix == QLatin1String("mdown") || suffix == QLatin1String("mkd");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("geometry"), saveGeometry());
        settings.setValue(QStringLiteral("splitter"), m_split->saveState());
        settings.setValue(QStringLiteral("lastDir"), m_dir);
        settings.setValue(QStringLiteral("lastFile"), m_file);
    }
    QMainWindow::closeEvent(event);
}
