#pragma once

#include <QImage>
#include <QWidget>

class QString;

namespace fv1::gui {

// Software-rendered startup splash. Foreground geometry, typography, waveform,
// DIP artwork and progress UI are painted with Qt so the splash remains crisp
// at any DPI. The repository-provided black-and-white FV1LabSplashImagebase.png
// is loaded as the default full-bleed background, accent-tinted in software,
// and darkened behind the existing foreground composition.
class StartupSplash final : public QWidget {
public:
    enum class Mode {
        Startup,
        About,
    };

    explicit StartupSplash(const QString& accent_name, QWidget* parent = nullptr,
                           Mode mode = Mode::Startup);

    void set_progress(int percent, const QString& status);
    void set_background_image(const QImage& image);
    void clear_background_image();
    [[nodiscard]] bool has_background_image() const noexcept { return !background_image_.isNull(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void fit_to_available_screen();

    int progress_{5};
    QString status_;
    QString accent_name_;
    QImage background_image_;
    Mode mode_{Mode::Startup};
};

} // namespace fv1::gui
