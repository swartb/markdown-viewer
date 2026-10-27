#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextStream>
#include <QUrl>

#include <cstdio>

#include "../src/mainwindow.h"
#include "../src/theme.h"

static int failures = 0;

#define CHECK(condition, message)                                                   \
    do {                                                                            \
        if (!(condition)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n", message, __FILE__, __LINE__); \
            ++failures;                                                             \
        }                                                                           \
    } while (0)

static void writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::fprintf(stderr, "kan %s niet schrijven\n", qPrintable(path));
        ++failures;
        return;
    }
    QTextStream(&file) << content;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QTemporaryDir temp;
    CHECK(temp.isValid(), "tijdelijke map");
    const QString dir = temp.path();
    QDir(dir).mkpath(QStringLiteral("notities"));
    writeFile(dir + "/welkom.md", "# Welkom\n\nDit is **vet** en `code`.\n");
    writeFile(dir + "/tweede.md", "---\ntitle: Verborgen\n---\n\n# Tweede\n\n- een\n- twee\n");
    writeFile(dir + "/geheim.txt", "geen markdown\n");
    writeFile(dir + "/.verborgen.md", "# Verborgen bestand\n");
    writeFile(dir + "/notities/genest.md", "# Genest\n");

    MainWindow window(Theme::load(), false);
    window.resize(1100, 780);

    // Map openen: alleen mappen en markdown-bestanden, verborgen bestanden weg.
    CHECK(window.openDir(dir), "map openen");
    CHECK(window.currentDir() == dir, "huidige map");
    CHECK(window.visibleItemCount() == 3, "2 bestanden + 1 submap zichtbaar");

    auto *list = window.findChild<QListWidget *>(QStringLiteral("fileList"));
    CHECK(list != nullptr, "lijst aanwezig");
    if (list) {
        CHECK(list->count() == 3, "lijst bevat 3 items");
        CHECK(list->item(0)->text() == QStringLiteral("notities"), "mappen eerst");
        CHECK(list->item(1)->text() == QStringLiteral("tweede.md"), "daarna bestanden op naam");
        CHECK(list->item(0)->data(Qt::UserRole + 3).toString().isEmpty(), "mappen hebben geen grootte");
    }

    auto *places = window.findChild<QListWidget *>(QStringLiteral("placesList"));
    CHECK(places != nullptr, "zijbalk met plekken");
    if (places) {
        CHECK(places->count() > 1, "plekken gevuld");
        CHECK(places->item(0)->data(Qt::UserRole + 5).toBool(), "eerste rij is een kop");
    }

    // Bestand openen en renderen.
    CHECK(window.openFile(dir + "/welkom.md"), "markdown openen");
    CHECK(window.currentFile() == dir + "/welkom.md", "huidig bestand");
    CHECK(window.isDocumentHeaderVisible(), "documentkop zichtbaar");
    CHECK(window.documentPlainText().contains(QStringLiteral("Welkom")), "kop gerenderd");
    CHECK(window.documentPlainText().contains(QStringLiteral("vet")), "vette tekst gerenderd");

    // Een relatieve link naar een ander markdown-bestand opent dat bestand.
    auto *browser = window.findChild<QTextBrowser *>();
    CHECK(browser != nullptr, "documentweergave aanwezig");
    if (browser) {
        const QString target = dir + "/notities/genest.md";
        QMetaObject::invokeMethod(browser, "anchorClicked", Q_ARG(QUrl, QUrl::fromLocalFile(target)));
        CHECK(window.currentFile() == target, "link naar ander bestand");
        CHECK(window.currentDir() == dir + "/notities", "link opent juiste map");
    }
    window.openDir(dir);
    window.openFile(dir + "/welkom.md");

    // Frontmatter wordt niet als tekst getoond.
    CHECK(window.openFile(dir + "/tweede.md"), "tweede bestand openen");
    CHECK(!window.documentPlainText().contains(QStringLiteral("title:")), "frontmatter verborgen");
    CHECK(window.documentPlainText().contains(QStringLiteral("Tweede")), "inhoud na frontmatter");

    // Niet-markdown wordt geweigerd.
    CHECK(!window.openFile(dir + "/geheim.txt"), "txt-bestand geweigerd");
    CHECK(!window.openFile(dir + "/bestaat-niet.md"), "ontbrekend bestand geweigerd");

    // Genest bestand opent de bijbehorende map.
    CHECK(window.openFile(dir + "/notities/genest.md"), "genest bestand openen");
    CHECK(window.currentDir() == dir + "/notities", "map van genest bestand");

    // Zoekfilter.
    window.openDir(dir);
    auto *search = window.findChild<QLineEdit *>();
    CHECK(search != nullptr, "zoekveld aanwezig");
    if (search) {
        search->setText(QStringLiteral("welkom"));
        CHECK(window.visibleItemCount() == 1, "filter toont één item");
        search->clear();
        CHECK(window.visibleItemCount() == 3, "filter wissen toont alles");
    }

    // Een open document blijft staan als de bovenliggende map wordt geopend.
    CHECK(window.documentPlainText().contains(QStringLiteral("Genest")),
          "document blijft bij mappenwissel");

    // Thema levert geldige kleuren.
    const Theme theme = Theme::load();
    CHECK(theme.background.isValid() && theme.accent.isValid(), "thema-kleuren");
    CHECK(!theme.appStyleSheet().isEmpty(), "stylesheet");

    if (failures == 0)
        std::printf("Alle tests geslaagd.\n");
    else
        std::fprintf(stderr, "%d test(s) mislukt.\n", failures);
    return failures == 0 ? 0 : 1;
}
