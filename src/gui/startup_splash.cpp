#include <fv1/gui/startup_splash.hpp>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QScreen>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace fv1::gui {
namespace {

constexpr auto kSplashBackgroundFile = "FV1LabSplashImagebase.png";
constexpr qreal kSplashDesignWidth = 960.0;
constexpr qreal kSplashDesignHeight = 540.0;

QImage load_default_splash_background() {
    const QString file = QString::fromLatin1(kSplashBackgroundFile);
    const QStringList candidates{
        QDir::current().filePath(QStringLiteral("assets/splash/") + file),
        QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("../share/spin-fv1-emulator/splash/") + file),
        QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("../assets/splash/") + file)
    };

    for (const QString& candidate : candidates) {
        const QString clean = QDir::cleanPath(candidate);
        if (!QFileInfo(clean).isFile()) continue;
        QImage image(clean);
        if (!image.isNull()) return image;
    }
    return {};
}

QColor accent_color(const QString& name) {
    if (name.compare(QStringLiteral("Blue"), Qt::CaseInsensitive) == 0) return QColor(70, 145, 255);
    if (name.compare(QStringLiteral("Green"), Qt::CaseInsensitive) == 0) return QColor(80, 220, 150);
    if (name.compare(QStringLiteral("Amber"), Qt::CaseInsensitive) == 0) return QColor(255, 168, 40);
    if (name.compare(QStringLiteral("Orange"), Qt::CaseInsensitive) == 0) return QColor(255, 120, 45);
    if (name.compare(QStringLiteral("Red"), Qt::CaseInsensitive) == 0) return QColor(245, 75, 75);
    if (name.compare(QStringLiteral("Purple"), Qt::CaseInsensitive) == 0) return QColor(165, 105, 255);
    if (name.compare(QStringLiteral("Magenta"), Qt::CaseInsensitive) == 0) return QColor(235, 80, 205);
    return QColor(35, 220, 245); // Cyan/default.
}

void draw_centered_text(QPainter& p, const QRectF& rect, const QString& text,
                        qreal point_size, const QColor& color, int weight = QFont::Normal) {
    QFont f = p.font();
    f.setPointSizeF(point_size);
    f.setWeight(static_cast<QFont::Weight>(weight));
    p.setFont(f);
    p.setPen(color);
    p.drawText(rect, Qt::AlignCenter, text);
}

void draw_chip(QPainter& p, const QRectF& body, const QColor& accent) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // A restrained, standalone DIP package for the splash composition. This is
    // intentionally not the application-icon rendering; it is just one of the
    // technical foreground elements that can later sit over a photo collage.
    QLinearGradient chip_gradient(body.topLeft(), body.bottomLeft());
    chip_gradient.setColorAt(0.0, QColor(70, 74, 79));
    chip_gradient.setColorAt(0.32, QColor(30, 33, 36));
    chip_gradient.setColorAt(1.0, QColor(8, 10, 12));
    p.setBrush(chip_gradient);
    p.setPen(QPen(accent.darker(165), 1.2));
    p.drawRoundedRect(body, 8.0, 8.0);

    const QPointF dimple(body.left() + body.height() * 0.18, body.center().y());
    p.setBrush(QColor(6, 8, 10));
    p.setPen(QPen(QColor(125, 130, 135), 0.8));
    p.drawEllipse(dimple, body.height() * 0.10, body.height() * 0.10);

    constexpr int pins_per_side = 7;
    const qreal first = body.left() + body.width() * 0.16;
    const qreal last = body.right() - body.width() * 0.08;
    const qreal step = (last - first) / static_cast<qreal>(pins_per_side - 1);
    QLinearGradient metal(0, body.bottom(), 0, body.bottom() + body.height() * 0.55);
    metal.setColorAt(0.0, QColor(235, 238, 240));
    metal.setColorAt(0.45, QColor(132, 139, 145));
    metal.setColorAt(1.0, QColor(62, 68, 74));
    p.setBrush(metal);
    p.setPen(QPen(QColor(215, 220, 224), 0.6));
    for (int i = 0; i < pins_per_side; ++i) {
        const qreal x = first + step * static_cast<qreal>(i);
        QPainterPath pin;
        pin.moveTo(x - 4.4, body.bottom() - 1);
        pin.lineTo(x + 4.4, body.bottom() - 1);
        pin.lineTo(x + 3.2, body.bottom() + 10);
        pin.lineTo(x + 1.8, body.bottom() + 18);
        pin.lineTo(x - 1.8, body.bottom() + 18);
        pin.lineTo(x - 3.2, body.bottom() + 10);
        pin.closeSubpath();
        p.drawPath(pin);
    }
    p.restore();
}

void draw_scope(QPainter& p, const QRectF& scope, const QColor& accent) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // Engineering grid. It remains subtle enough to work over the future
    // monochrome-photo background without becoming a second focal point.
    p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 42), 0.7, Qt::DashLine));
    for (int i = 1; i < 9; ++i) {
        const qreal x = scope.left() + scope.width() * static_cast<qreal>(i) / 9.0;
        p.drawLine(QPointF(x, scope.top()), QPointF(x, scope.bottom()));
    }
    for (int i = 1; i < 4; ++i) {
        const qreal y = scope.top() + scope.height() * static_cast<qreal>(i) / 4.0;
        p.drawLine(QPointF(scope.left(), y), QPointF(scope.right(), y));
    }

    QPainterPath wave;
    constexpr int points = 420;
    for (int i = 0; i < points; ++i) {
        const qreal u = static_cast<qreal>(i) / static_cast<qreal>(points - 1);
        const qreal x = scope.left() + u * scope.width();
        const double envelope = 0.38 + 0.62 * std::sin(3.14159265358979323846 * static_cast<double>(u));
        const double v = std::sin(7.0 * 3.14159265358979323846 * static_cast<double>(u))
                       + 0.43 * std::sin(17.0 * 3.14159265358979323846 * static_cast<double>(u) + 0.65)
                       + 0.20 * std::sin(37.0 * 3.14159265358979323846 * static_cast<double>(u));
        const qreal y = scope.center().y() - static_cast<qreal>(v * envelope) * scope.height() * 0.31;
        if (i == 0) wave.moveTo(x, y); else wave.lineTo(x, y);
    }

    p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 34), 9.0,
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(wave);
    p.setPen(QPen(accent, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(wave);
    p.restore();
}

QImage tinted_monochrome_background(const QImage& source, const QColor& accent) {
    if (source.isNull()) return {};

    // Preserve the photographic luminance instead of replacing it with a flat
    // accent silhouette. A low-alpha SourceAtop wash lets each application
    // accent gently color the black-and-white base photograph while keeping
    // pedals, keys, cables and guitar detail recognizable.
    const QImage gray8 = source.convertToFormat(QImage::Format_Grayscale8);
    QImage tinted = gray8.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter tp(&tinted);
    tp.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    QColor tint = accent;
    tint.setAlpha(62);
    tp.fillRect(tinted.rect(), tint);
    tp.end();
    return tinted;
}

} // namespace

StartupSplash::StartupSplash(const QString& accent_name, QWidget* parent, Mode mode)
    : QWidget(parent),
      status_(QStringLiteral("Initializing application…")),
      accent_name_(accent_name),
      background_image_(load_default_splash_background()),
      mode_(mode) {
    if (mode_ == Mode::Startup) {
        setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
    } else {
        // About is a conventional owned dialog so it behaves like a native
        // desktop About window while reusing the exact branded splash artwork.
        setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
        setWindowModality(Qt::WindowModal);
        setWindowTitle(QStringLiteral("About FV-1 Lab"));
    }
    fit_to_available_screen();
}

void StartupSplash::fit_to_available_screen() {
    QScreen* screen = nullptr;

    if (parentWidget())
        screen = parentWidget()->screen();

    if (!screen)
        screen = QApplication::primaryScreen();

    if (!screen) {
        setFixedSize(
            static_cast<int>(kSplashDesignWidth),
            static_cast<int>(kSplashDesignHeight));
        return;
    }

    const QRect area = screen->availableGeometry();
    const qreal width_scale =
        static_cast<qreal>(area.width()) * 0.92
        / kSplashDesignWidth;
    const qreal height_scale =
        static_cast<qreal>(area.height()) * 0.88
        / kSplashDesignHeight;
    const qreal scale = std::clamp(
        std::min(width_scale, height_scale),
        0.50,
        1.0);

    const QSize target(
        std::max(
            1,
            static_cast<int>(
                std::lround(kSplashDesignWidth * scale))),
        std::max(
            1,
            static_cast<int>(
                std::lround(kSplashDesignHeight * scale))));

    setFixedSize(target);
    move(
        area.center()
        - QPoint(target.width() / 2, target.height() / 2));
}

void StartupSplash::set_progress(int percent, const QString& status) {
    progress_ = std::clamp(percent, 0, 100);
    status_ = status;
    update();
    QApplication::processEvents();
}

void StartupSplash::set_background_image(const QImage& image) {
    background_image_ = image;
    update();
}

void StartupSplash::clear_background_image() {
    background_image_ = QImage{};
    update();
}

void StartupSplash::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    if (mode_ == Mode::About) {
        // Normal decorated dialogs are opaque; explicitly paint the content
        // background outside the rounded artwork panel as well.
        p.fillRect(rect(), QColor(4, 7, 9));
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal scale = std::min(
        static_cast<qreal>(width()) / kSplashDesignWidth,
        static_cast<qreal>(height()) / kSplashDesignHeight);
    const qreal x_offset =
        (static_cast<qreal>(width())
         - kSplashDesignWidth * scale)
        * 0.5;
    const qreal y_offset =
        (static_cast<qreal>(height())
         - kSplashDesignHeight * scale)
        * 0.5;

    p.save();
    p.translate(x_offset, y_offset);
    p.scale(scale, scale);

    const QColor accent = accent_color(accent_name_);
    const QRectF panel(
        16.0,
        16.0,
        kSplashDesignWidth - 32.0,
        kSplashDesignHeight - 32.0);

    for (int i = 18; i >= 1; --i) {
        QColor shadow(0, 0, 0, std::max(1, 22 - i));
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawRoundedRect(panel.adjusted(-i * 0.35, -i * 0.25, i * 0.35, i * 0.5),
                          20 + i * 0.2, 20 + i * 0.2);
    }

    // The gradient is both the fallback when the packaged photo is unavailable
    // and the dark foundation beneath the normal photographic background.
    QLinearGradient bg(panel.topLeft(), panel.bottomRight());
    bg.setColorAt(0.0, QColor(24, 30, 35));
    bg.setColorAt(0.50, QColor(9, 15, 20));
    bg.setColorAt(1.0, QColor(5, 9, 12));
    p.setBrush(bg);
    p.setPen(QPen(QColor(135, 145, 152), 1.2));
    p.drawRoundedRect(panel, 18.0, 18.0);

    p.save();
    QPainterPath clip;
    clip.addRoundedRect(panel.adjusted(2, 2, -2, -2), 16.0, 16.0);
    p.setClipPath(clip);

    if (!background_image_.isNull()) {
        const QImage tinted = tinted_monochrome_background(background_image_, accent);
        const QSize target_size = panel.size().toSize();
        const QImage scaled = tinted.scaled(target_size, Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation);
        const QPointF origin(panel.center().x() - scaled.width() * 0.5,
                             panel.center().y() - scaled.height() * 0.5);
        p.setOpacity(0.68);
        p.drawImage(origin, scaled);
        p.setOpacity(1.0);
        p.fillRect(panel, QColor(2, 6, 9, 112));
    }

    // Subtle brushed/engineering texture remains software-drawn and works on
    // both the fallback gradient and the photographic background layer.
    for (int y = 28; y < static_cast<int>(kSplashDesignHeight) - 28; y += 3) {
        const int mod = (y * 37) % 23;
        p.setPen(QColor(255, 255, 255, 3 + mod / 6));
        p.drawLine(QPointF(24, y), QPointF(kSplashDesignWidth - 24, y));
    }
    p.restore();

    // Dedicated splash composition: standalone typography, scope waveform and
    // DIP artwork. No application-icon badge is reused or imitated here.
    draw_centered_text(p, QRectF(92, 58, 300, 76), QStringLiteral("FV-1"), 52.0,
                       QColor(222, 226, 230), QFont::Bold);

    const QRectF scope(88, 132, 784, 118);
    draw_scope(p, scope, accent);

    // Place the package as an independent technical element over the lower
    // right of the waveform, leaving clear space for all product typography.
    draw_chip(p, QRectF(570, 166, 245, 48), accent);

    draw_centered_text(p, QRectF(155, 266, 650, 64), QStringLiteral("Spin FV-1 Emulator"), 34.0,
                       QColor(205, 211, 216), QFont::Light);

    p.setPen(QPen(accent, 1.1));
    p.drawLine(QPointF(285, 337), QPointF(400, 337));
    p.drawLine(QPointF(560, 337), QPointF(675, 337));
    draw_centered_text(p, QRectF(400, 320, 160, 36), QStringLiteral("FV-1 Lab"), 16.0,
                       accent, QFont::Medium);
    draw_centered_text(p, QRectF(285, 352, 390, 34),
                       QStringLiteral("Virtual DSP Testbench for Spin FV-1"),
                       12.5, QColor(170, 178, 185), QFont::Normal);

    const QString app_version = QCoreApplication::applicationVersion().isEmpty()
        ? QStringLiteral("development build")
        : QCoreApplication::applicationVersion();

    if (mode_ == Mode::Startup) {
        // Progress UI reports actual startup milestones.
        const QRectF track(66, 414, kSplashDesignWidth - 132.0, 12);
        p.setBrush(QColor(3, 7, 9, 210));
        p.setPen(QPen(QColor(75, 85, 92), 1.0));
        p.drawRoundedRect(track, 6, 6);
        const QRectF fill(track.left() + 2, track.top() + 2,
                          std::max<qreal>(0.0, (track.width() - 4) * static_cast<qreal>(progress_) / 100.0),
                          track.height() - 4);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(fill, 4, 4);

        QFont small = p.font();
        small.setPointSizeF(9.5);
        p.setFont(small);
        p.setPen(QColor(175, 183, 190));
        p.drawText(QRectF(66, 433, 720, 24), Qt::AlignLeft | Qt::AlignVCenter, status_);
        QFont pct = p.font();
        pct.setPointSizeF(12.0);
        pct.setWeight(QFont::Medium);
        p.setFont(pct);
        p.setPen(accent);
        p.drawText(QRectF(kSplashDesignWidth - 150, 430, 84, 28), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1%").arg(progress_));

        draw_centered_text(p, QRectF(250, 462, 460, 22),
                           QStringLiteral("Created & engineered by Adam Vadala-Roth"),
                           9.5, QColor(158, 166, 173), QFont::Medium);
        p.setPen(QPen(QColor(90, 100, 108), 0.8));
        p.drawLine(QPointF(55, 508), QPointF(285, 508));
        p.drawLine(QPointF(675, 508), QPointF(kSplashDesignWidth - 55, 508));
        draw_centered_text(p, QRectF(285, 486, 390, 34),
                           QStringLiteral("© 2026 Roth Amplification LTD  •  MPL-2.0  •  %1").arg(app_version),
                           8.8, QColor(115, 124, 132), QFont::Normal);
    } else {
        // The on-demand About presentation replaces startup progress with the
        // credits/version block while preserving the same branded composition.
        draw_centered_text(p, QRectF(175, 404, 610, 26),
                           QStringLiteral("Created & engineered by Adam Vadala-Roth"),
                           11.5, QColor(205, 211, 216), QFont::Medium);
        draw_centered_text(p, QRectF(175, 432, 610, 24),
                           QStringLiteral("Roth Amplification LTD"),
                           10.5, accent, QFont::Medium);
        draw_centered_text(p, QRectF(145, 458, 670, 22),
                           QStringLiteral("Spin FV-1 Emulator / FV-1 Lab  •  Version %1").arg(app_version),
                           9.5, QColor(166, 174, 181), QFont::Normal);
        draw_centered_text(p, QRectF(145, 481, 670, 22),
                           QStringLiteral("Open-source software licensed under the Mozilla Public License 2.0"),
                           8.8, QColor(132, 141, 149), QFont::Normal);
        p.setPen(QPen(QColor(90, 100, 108), 0.8));
        p.drawLine(QPointF(55, 510), QPointF(330, 510));
        p.drawLine(QPointF(630, 510), QPointF(kSplashDesignWidth - 55, 510));
        draw_centered_text(p, QRectF(330, 497, 300, 26),
                           QStringLiteral("© 2026 Roth Amplification LTD"),
                           8.8, QColor(110, 119, 127), QFont::Normal);
    }

    p.restore();
}

} // namespace fv1::gui
