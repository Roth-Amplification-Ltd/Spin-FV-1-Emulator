#pragma once

#include <QImage>
#include <QWidget>

class QString;

namespace fv1::gui {

// Software-rendered startup splash. Foreground geometry, typography, waveform,
// DIP artwork and progress UI are painted with Qt so the splash remains crisp
// at any DPI. The background is intentionally blank/dark unless an optional
// image is supplied. That hook is reserved for a future monochrome collage,
// which the renderer tints to the active application accent.
class StartupSplash final : public QWidget {
public:
    explicit StartupSplash(const QString& accent_name, QWidget* parent = nullptr);

    void set_progress(int percent, const QString& status);
    void set_background_image(const QImage& image);
    void clear_background_image();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int progress_{5};
    QString status_;
    QString accent_name_;
    QImage background_image_;
};

} // namespace fv1::gui
