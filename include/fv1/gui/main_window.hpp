#pragma once

#include <QMainWindow>
#include <QByteArray>

#include <functional>

#include <fv1/debugger.hpp>

#include <cstddef>
#include <memory>

class QAction;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QTableWidget;
class QTimer;

namespace fv1::gui {

class InstrumentPlot;
class ValidationPanel;
class SessionController;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr,
                        std::function<void(int, const QString&)> startup_progress = {});
    ~MainWindow() override;

    // Shared desktop workflow entry point used by command-line/Open-With and
    // drag/drop. Loading never starts realtime audio.
    bool open_external_path(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void build_menus();
    void build_toolbar();
    void build_left_dock();
    void build_center();
    void build_right_dock();
    void build_status_footer();

    void refresh_audio_devices();
    void show_audio_settings();
    void show_generator_settings();
    void show_loop_region_settings();
    void show_about();
    void choose_program();
    bool load_program_path(const QString& path);
    void paste_spinasm();
    bool install_program_image(const QByteArray& bytes, const QString& display_name, const QString& source_path = {});
    void choose_audio_file();
    bool load_audio_path(const QString& path);
    void remember_recent_path(const QString& settings_key, const QString& path);
    void rebuild_recent_menus();
    QString dialog_directory(const QString& settings_key, const QString& fallback = {}) const;
    void remember_directory(const QString& settings_key, const QString& selected_path);
    void restore_workspace_state();
    void save_workspace_state();
    void reset_workspace_layout();
    void inspect_program();
    void start_session();
    void stop_session();
    void start_recording();
    void stop_recording();
    void update_telemetry();
    void update_file_transport_ui();
    void log(const QString& text);
    void set_theme(const QString& theme_name);
    void set_accent(const QString& accent_name);
    void set_app_icon(const QString& icon_name);
    void set_dsp_enabled(bool enabled);
    void set_compare_enabled(bool enabled);
    void update_signal_monitor_labels();
    void install_plot_context_menu(InstrumentPlot* plot);

    void debugger_load_program();
    void debugger_reset();
    void debugger_step_instruction();
    void debugger_step_sample();
    void debugger_continue_sample();
    void debugger_refresh();

    QString theme_name_{QStringLiteral("Dark")};
    QString accent_name_{QStringLiteral("Cyan")};
    QString icon_name_{QStringLiteral("Silver")};
    QString program_path_;
    QString program_display_name_;
    QString pasted_spinasm_source_;
    QByteArray program_image_;
    QString audio_file_path_;
    int resampler_quality_{7};
    std::size_t analyzer_fft_size_{4096};
    bool dsp_enabled_{true};
    bool compare_raw_processed_{true};

    // Test stimulus settings live outside the realtime callback and are copied
    // into TestSignalConfig when a session starts.
    double generator_amplitude_{0.25};
    double generator_sweep_end_hz_{12000.0};
    double generator_sweep_seconds_{5.0};
    double generator_impulse_period_seconds_{1.0};

    // File-loop settings are expressed in seconds for UI persistence. The
    // FileLoopSource translates these to source-file frames.
    double file_duration_seconds_{};
    double loop_begin_seconds_{};
    double loop_end_seconds_{};
    bool file_loop_enabled_{true};
    double loop_crossfade_ms_{5.0};

    std::unique_ptr<SessionController> session_;
    std::unique_ptr<fv1::Debugger> debugger_;
    QTimer* telemetry_timer_{};
    QAction* dsp_action_{};
    QAction* compare_action_{};
    QAction* record_action_{};
    QMenu* recent_programs_menu_{};
    QMenu* recent_audio_menu_{};
    QByteArray default_window_state_;

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
    QLabel* file_position_label_{};
    QSlider* file_position_slider_{};
    QPushButton* file_play_button_{};
    QPushButton* file_pause_button_{};
    QPushButton* file_stop_button_{};
    QPushButton* file_loop_button_{};
    QLabel* runtime_status_{};
    QPlainTextEdit* console_{};
    QTableWidget* debugger_table_{};
    QTableWidget* register_table_{};
    QPlainTextEdit* delay_view_{};
    QDoubleSpinBox* debug_input_left_{};
    QDoubleSpinBox* debug_input_right_{};
    QProgressBar* program_usage_{};
    QProgressBar* delay_usage_{};
    QProgressBar* register_usage_{};
    QProgressBar* sin_lfo_usage_{};
    QProgressBar* ramp_lfo_usage_{};
    QLabel* resource_details_{};
    QLabel* copyright_label_{};
    InstrumentPlot* scope_plot_{};
    InstrumentPlot* spectrum_plot_{};
    InstrumentPlot* spectrogram_plot_{};
    InstrumentPlot* levels_plot_{};
    ValidationPanel* validation_panel_{};
};

} // namespace fv1::gui
