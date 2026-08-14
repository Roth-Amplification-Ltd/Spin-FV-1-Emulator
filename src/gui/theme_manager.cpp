#include <fv1/gui/theme_manager.hpp>

#include <QApplication>
#include <QPalette>

#include <algorithm>

namespace fv1::gui {
namespace {

QColor mix(const QColor& a, const QColor& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(
        a.redF()   * (1.0 - t) + b.redF()   * t,
        a.greenF() * (1.0 - t) + b.greenF() * t,
        a.blueF()  * (1.0 - t) + b.blueF()  * t,
        1.0);
}

ThemeSpec dark_base(const QString& name, const QColor& window, const QColor& panel,
                    const QColor& text, const QColor& accent) {
    ThemeSpec t;
    t.name = name;
    t.window = window;
    t.panel = panel;
    t.raised = mix(panel, QColor(255,255,255), 0.07);
    t.border = mix(panel, QColor(255,255,255), 0.18);
    t.text = text;
    t.muted = mix(text, panel, 0.43);
    t.accent = accent;
    t.success = QColor(88, 211, 122);
    t.warning = QColor(245, 184, 76);
    t.error = QColor(242, 92, 92);
    t.grid_major = mix(panel, text, 0.20);
    t.grid_minor = mix(panel, text, 0.10);
    return t;
}

} // namespace

QStringList ThemeManager::theme_names() {
    return {QStringLiteral("Dark"), QStringLiteral("Light"), QStringLiteral("Midnight"),
            QStringLiteral("Amber CRT"), QStringLiteral("Green Phosphor"),
            QStringLiteral("Slate"), QStringLiteral("High Contrast")};
}

QStringList ThemeManager::accent_names() {
    return {QStringLiteral("Cyan"), QStringLiteral("Blue"), QStringLiteral("Green"),
            QStringLiteral("Amber"), QStringLiteral("Orange"), QStringLiteral("Red"),
            QStringLiteral("Purple"), QStringLiteral("Magenta")};
}

QColor ThemeManager::accent(const QString& name) {
    if (name == QStringLiteral("Blue")) return QColor(75, 138, 255);
    if (name == QStringLiteral("Green")) return QColor(76, 214, 132);
    if (name == QStringLiteral("Amber")) return QColor(244, 182, 63);
    if (name == QStringLiteral("Orange")) return QColor(245, 132, 55);
    if (name == QStringLiteral("Red")) return QColor(238, 82, 82);
    if (name == QStringLiteral("Purple")) return QColor(156, 112, 255);
    if (name == QStringLiteral("Magenta")) return QColor(232, 93, 205);
    return QColor(66, 208, 232); // Cyan
}

ThemeSpec ThemeManager::theme(const QString& name, const QString& accent_name) {
    const QColor selected_accent = accent(accent_name);
    if (name == QStringLiteral("Light")) {
        ThemeSpec t;
        t.name = name;
        t.window = QColor(237, 240, 244);
        t.panel = QColor(250, 251, 252);
        t.raised = QColor(255, 255, 255);
        t.border = QColor(190, 198, 208);
        t.text = QColor(28, 34, 42);
        t.muted = QColor(94, 105, 118);
        t.accent = selected_accent;
        t.success = QColor(35, 142, 79);
        t.warning = QColor(180, 116, 20);
        t.error = QColor(190, 53, 53);
        t.grid_major = QColor(185, 193, 204);
        t.grid_minor = QColor(216, 222, 229);
        return t;
    }
    if (name == QStringLiteral("Midnight"))
        return dark_base(name, QColor(8, 13, 24), QColor(15, 23, 38), QColor(225, 234, 248), selected_accent);
    if (name == QStringLiteral("Amber CRT"))
        return dark_base(name, QColor(12, 9, 5), QColor(22, 16, 8), QColor(245, 194, 95), QColor(255, 184, 58));
    if (name == QStringLiteral("Green Phosphor"))
        return dark_base(name, QColor(4, 11, 7), QColor(7, 22, 13), QColor(133, 245, 151), QColor(79, 255, 113));
    if (name == QStringLiteral("Slate"))
        return dark_base(name, QColor(31, 35, 40), QColor(43, 48, 55), QColor(229, 233, 238), selected_accent);
    if (name == QStringLiteral("High Contrast")) {
        auto t = dark_base(name, QColor(0,0,0), QColor(0,0,0), QColor(255,255,255), QColor(255,230,0));
        t.border = QColor(255,255,255);
        t.muted = QColor(215,215,215);
        t.grid_major = QColor(150,150,150);
        t.grid_minor = QColor(80,80,80);
        return t;
    }
    return dark_base(QStringLiteral("Dark"), QColor(18, 20, 24), QColor(27, 30, 35),
                     QColor(228, 233, 239), selected_accent);
}

void ThemeManager::apply(QApplication& app, const QString& theme_name, const QString& accent_name) {
    const ThemeSpec t = theme(theme_name, accent_name);
    QPalette p;
    p.setColor(QPalette::Window, t.window);
    p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.panel);
    p.setColor(QPalette::AlternateBase, t.raised);
    p.setColor(QPalette::ToolTipBase, t.raised);
    p.setColor(QPalette::ToolTipText, t.text);
    p.setColor(QPalette::Text, t.text);
    p.setColor(QPalette::Button, t.raised);
    p.setColor(QPalette::ButtonText, t.text);
    p.setColor(QPalette::BrightText, t.error);
    p.setColor(QPalette::Highlight, t.accent);
    p.setColor(QPalette::HighlightedText, t.window);
    p.setColor(QPalette::PlaceholderText, t.muted);
    app.setPalette(p);

    app.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background-color: %1; color: %2; }
        QDockWidget { color: %2; }
        QDockWidget::title { background: %3; padding: 6px; border-bottom: 1px solid %4; }
        QGroupBox { border: 1px solid %4; border-radius: 5px; margin-top: 11px; padding-top: 8px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QPushButton, QToolButton, QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
            background: %3; border: 1px solid %4; border-radius: 4px; padding: 5px;
        }
        QPushButton:hover, QToolButton:hover { border-color: %5; }
        QPushButton:checked, QToolButton:checked { background: %5; color: %1; }
        QTabWidget::pane { border: 1px solid %4; }
        QTabBar::tab { background: %3; border: 1px solid %4; padding: 7px 12px; }
        QTabBar::tab:selected { border-bottom: 2px solid %5; }
        QPlainTextEdit, QTextEdit, QTableWidget, QListWidget, QTreeWidget {
            background: %6; border: 1px solid %4; selection-background-color: %5;
        }
        QSlider::groove:horizontal { height: 4px; background: %4; }
        QSlider::handle:horizontal { width: 14px; margin: -5px 0; border-radius: 7px; background: %5; }
        QProgressBar { border: 1px solid %4; border-radius: 3px; text-align: center; background: %6; }
        QProgressBar::chunk { background: %5; }
        QStatusBar { border-top: 1px solid %4; }
    )")
        .arg(t.window.name())
        .arg(t.text.name())
        .arg(t.raised.name())
        .arg(t.border.name())
        .arg(t.accent.name())
        .arg(t.panel.name()));

    app.setProperty("fv1ThemeName", t.name);
    app.setProperty("fv1ThemeAccent", t.accent);
    app.setProperty("fv1ThemeGridMajor", t.grid_major);
    app.setProperty("fv1ThemeGridMinor", t.grid_minor);
    app.setProperty("fv1ThemeMuted", t.muted);
}

} // namespace fv1::gui
