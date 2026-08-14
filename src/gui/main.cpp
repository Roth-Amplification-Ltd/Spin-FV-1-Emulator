#include <fv1/gui/main_window.hpp>
#include <fv1/gui/startup_splash.hpp>
#include <fv1/gui/theme_manager.hpp>

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QSettings>
#include <QStringList>
#include <QTimer>

#include <algorithm>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Roth Amplification Ltd"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("roth-amplification.com"));
    QCoreApplication::setApplicationName(QStringLiteral("Spin FV-1 Emulator"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.6.0"));
    QGuiApplication::setDesktopFileName(QStringLiteral("roth-fv1-emulator"));

    const bool smoke = QCoreApplication::arguments().contains(QStringLiteral("--smoke"));
    const bool no_splash = QCoreApplication::arguments().contains(QStringLiteral("--no-splash"));
    QSettings settings;
    const QString theme = settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString();
    const QString accent = settings.value(QStringLiteral("ui/accent"), QStringLiteral("Cyan")).toString();
    fv1::gui::ThemeManager::apply(app, theme, accent);

    if (smoke) {
        fv1::gui::MainWindow window;
        app.processEvents();
        return 0;
    }
    if (no_splash) {
        fv1::gui::MainWindow window;
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
