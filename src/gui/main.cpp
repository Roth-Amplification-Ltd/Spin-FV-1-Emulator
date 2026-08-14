#include <fv1/gui/main_window.hpp>
#include <fv1/gui/theme_manager.hpp>

#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QStringList>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Roth Amplification Ltd"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("roth-amplification.com"));
    QCoreApplication::setApplicationName(QStringLiteral("Spin FV-1 Emulator"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.4.0"));

    QSettings settings;
    fv1::gui::ThemeManager::apply(app,
        settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString(),
        settings.value(QStringLiteral("ui/accent"), QStringLiteral("Cyan")).toString());

    fv1::gui::MainWindow window;
    if (QCoreApplication::arguments().contains(QStringLiteral("--smoke"))) {
        app.processEvents();
        return 0;
    }
    window.show();
    return app.exec();
}
