#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

class QApplication;

namespace fv1::gui {

struct ThemeSpec {
    QString name;
    QColor window;
    QColor panel;
    QColor raised;
    QColor border;
    QColor text;
    QColor muted;
    QColor accent;
    QColor success;
    QColor warning;
    QColor error;
    QColor grid_major;
    QColor grid_minor;
};

class ThemeManager {
public:
    static QStringList theme_names();
    static QStringList accent_names();
    static ThemeSpec theme(const QString& name, const QString& accent_name = QStringLiteral("Cyan"));
    static QColor accent(const QString& name);
    static void apply(QApplication& app, const QString& theme_name, const QString& accent_name);
};

} // namespace fv1::gui
