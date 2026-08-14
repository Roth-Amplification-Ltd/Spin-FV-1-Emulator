#pragma once

#include <QMainWindow>

#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QTableWidget;
class QTimer;

namespace fv1::gui {

class InstrumentPlot;
class SessionController;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void build_menus();
    void build_toolbar();
    void build_left_dock();
    void build_center();
    void build_right_dock();
    void refresh_audio_devices();
    void choose_program();
    void choose_audio_file();
    void inspect_program();
    void start_session();
    void stop_session();
    void update_telemetry();
    void log(const QString& text);
    void set_theme(const QString& theme_name);
    void set_accent(const QString& accent_name);

    QString theme_name_{QStringLiteral("Dark")};
    QString accent_name_{QStringLiteral("Cyan")};
    QString program_path_;
    QString audio_file_path_;

    std::unique_ptr<SessionController> session_;
    QTimer* telemetry_timer_{};

    QComboBox* source_combo_{};
    QComboBox* playback_combo_{};
    QComboBox* capture_combo_{};
    QComboBox* host_rate_combo_{};
    QComboBox* buffer_combo_{};
    QComboBox* clock_combo_{};
    QComboBox* generator_combo_{};
    QDoubleSpinBox* generator_frequency_{};
    QSlider* pot0_{};
    QSlider* pot1_{};
    QSlider* pot2_{};
    QLabel* program_label_{};
    QLabel* file_label_{};
    QLabel* runtime_status_{};
    QPlainTextEdit* console_{};
    QTableWidget* debugger_table_{};
    QProgressBar* program_usage_{};
    QProgressBar* delay_usage_{};
    InstrumentPlot* scope_plot_{};
    InstrumentPlot* spectrum_plot_{};
    InstrumentPlot* spectrogram_plot_{};
    InstrumentPlot* levels_plot_{};
};

} // namespace fv1::gui
