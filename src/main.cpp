#include <QApplication>
#include <QFontDatabase>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // برای رندر شارپ در ویندوز
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Veyo Updater"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("VeyoUpdater"));
    app.setApplicationDisplayName(QStringLiteral("Veyo Updater"));

    // فونت مدرن: اگر Vazirmatn یا Inter نصب باشد استفاده می‌شود، در غیر این صورت Segoe UI
    QFont baseFont(QStringLiteral("Segoe UI"), 10);
    baseFont.setStyleHint(QFont::SansSerif);
    app.setFont(baseFont);

    MainWindow w;
    w.show();

    return app.exec();
}
