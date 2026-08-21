#include <fv1/gui/main_window.hpp>
#include <fv1/gui/startup_splash.hpp>
#include <fv1/gui/theme_manager.hpp>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSettings>
#include <QScreen>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

#include <algorithm>

#ifndef FV1_PRODUCT_VERSION_STRING
#define FV1_PRODUCT_VERSION_STRING "1.0.0-rc1"
#endif

int main(int argc, char** argv) {
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Roth Amplification Ltd"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("roth-amplification.com"));
    QCoreApplication::setApplicationName(QStringLiteral("Spin FV-1 Emulator"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(FV1_PRODUCT_VERSION_STRING));
    QGuiApplication::setDesktopFileName(QStringLiteral("roth-fv1-emulator"));

    const QStringList args = QCoreApplication::arguments();
    const bool smoke = args.contains(QStringLiteral("--smoke"));
    const bool splash_smoke = args.contains(QStringLiteral("--smoke-splash"));
    const bool about_smoke = args.contains(QStringLiteral("--smoke-about"));
    const bool desktop_smoke = args.contains(QStringLiteral("--smoke-desktop"));
    const bool no_splash = args.contains(QStringLiteral("--no-splash"));
    const int smoke_open_index = args.indexOf(QStringLiteral("--smoke-open"));
    QSettings settings;
    const QString theme = settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString();
    const QString accent = settings.value(QStringLiteral("ui/accent"), QStringLiteral("Cyan")).toString();
    fv1::gui::ThemeManager::apply(app, theme, accent);

    if (smoke) {
        fv1::gui::MainWindow window;
        app.processEvents();
        if (!window.findChild<QAction*>(QStringLiteral("aboutFv1LabAction"))) return 4;
        return 0;
    }
    if (desktop_smoke) {
        fv1::gui::MainWindow window;
        window.show();
        app.processEvents();

        QScreen* screen = window.screen();
        if (!screen) {
            QTextStream(stderr) << "FV1Lab desktop smoke: no screen available\n";
            return 7;
        }

        const QRect available = screen->availableGeometry();
        const QRect geometry = window.geometry();
        if (geometry.isEmpty() || !available.intersects(geometry)) {
            QTextStream(stderr)
                << "FV1Lab desktop smoke: main window is not visible on its screen\n";
            return 8;
        }

        fv1::gui::StartupSplash splash(accent);
        if (splash.width() <= 0 || splash.height() <= 0) return 9;

        const bool offscreen =
            QGuiApplication::platformName().compare(
                QStringLiteral("offscreen"),
                Qt::CaseInsensitive) == 0;
        if (!offscreen
            && (splash.width() > available.width()
                || splash.height() > available.height())) {
            QTextStream(stderr)
                << "FV1Lab desktop smoke: splash exceeds available screen geometry\n";
            return 10;
        }

        QTextStream out(stdout);
        out << "FV1LAB DESKTOP SMOKE PASSED\n"
            << "platform=" << QGuiApplication::platformName() << '\n'
            << "screen=" << screen->name() << '\n'
            << "logical-dpi=" << screen->logicalDotsPerInch() << '\n'
            << "device-pixel-ratio=" << screen->devicePixelRatio() << '\n'
            << "available=" << available.width() << 'x' << available.height() << '\n'
            << "window=" << geometry.width() << 'x' << geometry.height() << '\n'
            << "splash=" << splash.width() << 'x' << splash.height() << '\n';
        return 0;
    }
    if (smoke_open_index >= 0) {
        if (smoke_open_index + 1 >= args.size()) return 6;
        app.setProperty("fv1SmokeOpen", true);
        fv1::gui::MainWindow window;
        return window.open_external_path(
            args.at(smoke_open_index + 1))
            ? 0
            : 6;
    }
    if (splash_smoke) {
        fv1::gui::StartupSplash splash(accent);
        if (!splash.has_background_image()) return 3;
        splash.show();
        splash.set_progress(73, QStringLiteral("Splash background smoke test"));
        app.processEvents();
        splash.close();
        return 0;
    }
    if (about_smoke) {
        fv1::gui::MainWindow window;
        fv1::gui::StartupSplash about(accent, &window, fv1::gui::StartupSplash::Mode::About);
        if (!about.has_background_image()) return 5;
        about.show();
        app.processEvents();
        about.close();
        return 0;
    }
    QString startup_path;
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg.startsWith(QLatin1Char('-'))) continue;
        const QFileInfo info(arg);
        if (info.exists() && info.isFile()) {
            startup_path = info.absoluteFilePath();
            break;
        }
    }

    if (no_splash) {
        fv1::gui::MainWindow window;
        if (!startup_path.isEmpty()) window.open_external_path(startup_path);
        window.show();
        return app.exec();
    }

    // Keep the approved splash visible long enough to be perceived on fast
    // machines, while all percentages still correspond to actual startup
    // milestones reported by MainWindow construction.
    constexpr qint64 minimum_splash_ms = 1750;
    QElapsedTimer splash_timer;
    splash_timer.start();
    fv1::gui::StartupSplash splash(accent);
    splash.show();
    splash.set_progress(8, QStringLiteral("Initializing Spin FV-1 Emulator…"));
    splash.set_progress(16, QStringLiteral("Loading theme, icon and application settings…"));

    fv1::gui::MainWindow window(nullptr, [&splash](int percent, const QString& status) {
        splash.set_progress(percent, status);
    });

    if (!startup_path.isEmpty()) {
        splash.set_progress(92, QStringLiteral("Opening startup file…"));
        window.open_external_path(startup_path);
    }

    splash.set_progress(98, QStringLiteral("FV-1 Lab ready — preparing workspace…"));
    const qint64 remaining = std::max<qint64>(0, minimum_splash_ms - splash_timer.elapsed());
    if (remaining > 0) {
        QEventLoop wait;
        QTimer::singleShot(static_cast<int>(remaining), &wait, &QEventLoop::quit);
        wait.exec();
    }
    splash.set_progress(100, QStringLiteral("Ready"));

    // Briefly hold the completed state without blocking event processing.
    QEventLoop complete_hold;
    QTimer::singleShot(180, &complete_hold, &QEventLoop::quit);
    complete_hold.exec();

    window.show();
    splash.close();
    return app.exec();
}
