#include <fv1/gui/main_window.hpp>

#include <fv1/analysis.hpp>
#include <fv1/debugger.hpp>
#include <fv1/audio_host.hpp>
#include <fv1/audio_recorder.hpp>
#include <fv1/audio_source.hpp>
#include <fv1/fv1.h>
#include <fv1/gui/instrument_plot.hpp>
#include <fv1/gui/startup_splash.hpp>
#include <fv1/gui/theme_manager.hpp>
#include <fv1/gui/validation_panel.hpp>
#include <fv1/runtime.hpp>
#include <fv1/spinasm.hpp>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace fv1::gui {
namespace {

QSlider* make_parameter_slider(int value, QWidget* parent) {
    auto* slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(0, 1000);
    slider->setValue(value);
    return slider;
}

QString format_time(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    const int minutes = static_cast<int>(seconds / 60.0);
    const double remainder = seconds - static_cast<double>(minutes) * 60.0;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(remainder, 6, 'f', 3, QChar('0'));
}


QString find_icon_asset(const QString& icon_name) {
    QString slug = icon_name.toLower();
    if (slug == QStringLiteral("dark cyan")) slug = QStringLiteral("dark-cyan");
    const QString file = QStringLiteral("fv1-emulator-%1.png").arg(slug);
    const QStringList candidates{
        QDir::current().filePath(QStringLiteral("assets/icons/") + file),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../share/spin-fv1-emulator/icons/") + file),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../assets/icons/") + file)
    };
    for (const QString& path : candidates) {
        const QString clean = QDir::cleanPath(path);
        if (QFileInfo::exists(clean)) return clean;
    }
    return {};
}

bool load_program_image(const QString& path, QByteArray& bytes, QString& error) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("spn")) {
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) {
            error = QStringLiteral("Cannot open %1").arg(path);
            return false;
        }
        const QByteArray source_bytes = source.readAll();
        try {
            const auto compiled = fv1::spinasm::compile(std::string_view(
                source_bytes.constData(), static_cast<std::size_t>(source_bytes.size())));
            bytes = QByteArray(reinterpret_cast<const char*>(compiled.image.data()),
                               static_cast<qsizetype>(compiled.image.size()));
            return true;
        } catch (const fv1::spinasm::CompileError& compile_error) {
            error = QString::fromStdString(compile_error.what());
            return false;
        }
    }
    if (suffix != QStringLiteral("bin")) {
        error = QStringLiteral("The GUI opens .spn and 512-byte .bin programs. HEX/bank selection remains available in fv1-cli/fv1-live.");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error = QStringLiteral("Cannot open %1").arg(path); return false; }
    bytes = file.readAll();
    if (bytes.size() != static_cast<qsizetype>(FV1_PROGRAM_BYTES)) {
        error = QStringLiteral("Program image is %1 bytes; expected exactly %2.").arg(bytes.size()).arg(FV1_PROGRAM_BYTES);
        return false;
    }
    return true;
}

} // namespace

class SessionController {
public:
    ~SessionController() { stop(); }

    bool start(const QByteArray& program,
               const QString& source_mode,
               const QString& audio_file,
               const QString& generator_kind,
               double generator_frequency,
               double generator_amplitude,
               double sweep_end_hz,
               double sweep_seconds,
               double impulse_period_seconds,
               bool file_loop_enabled,
               double loop_begin_seconds,
               double loop_end_seconds,
               double loop_crossfade_ms,
               std::uint32_t host_rate,
               std::uint32_t period_frames,
               double fv1_rate,
               std::size_t analyzer_fft_size,
               int playback_device,
               int capture_device,
               int resampler_quality,
               bool dsp_enabled,
               float pot0, float pot1, float pot2,
               QString& error) {
        stop();
        file_source_ = nullptr;

        if (source_mode == QStringLiteral("Audio File Loop")) {
            if (audio_file.isEmpty()) {
                error = QStringLiteral("Choose an audio file before starting File Loop mode.");
                return false;
            }
            auto file = std::make_unique<fv1::FileLoopSource>();
            std::string source_error;
            if (!file->load(std::filesystem::path(audio_file.toStdString()), &source_error)) {
                error = QString::fromStdString(source_error);
                return false;
            }
            file->set_looping(file_loop_enabled);
            file->set_loop_crossfade_ms(loop_crossfade_ms);
            if (loop_end_seconds > loop_begin_seconds && file->file_sample_rate() != 0) {
                const auto rate = static_cast<double>(file->file_sample_rate());
                const auto begin = static_cast<std::uint64_t>(std::llround(loop_begin_seconds * rate));
                const auto end = static_cast<std::uint64_t>(std::llround(loop_end_seconds * rate));
                if (!file->set_loop_region(begin, end)) {
                    error = QStringLiteral("The selected loop region is outside the audio file.");
                    return false;
                }
            }
            file->play();
            file_source_ = file.get();
            source_ = std::move(file);
            needs_capture_ = false;
        } else if (source_mode == QStringLiteral("Test Generator")) {
            fv1::TestSignalConfig cfg;
            cfg.frequency_hz = generator_frequency;
            cfg.amplitude = generator_amplitude;
            cfg.sweep_end_hz = sweep_end_hz;
            cfg.sweep_seconds = sweep_seconds;
            cfg.impulse_period_seconds = impulse_period_seconds;
            if (generator_kind == QStringLiteral("Sweep")) cfg.kind = fv1::TestSignalKind::Sweep;
            else if (generator_kind == QStringLiteral("White Noise")) cfg.kind = fv1::TestSignalKind::WhiteNoise;
            else if (generator_kind == QStringLiteral("Pink Noise")) cfg.kind = fv1::TestSignalKind::PinkNoise;
            else if (generator_kind == QStringLiteral("Impulse")) cfg.kind = fv1::TestSignalKind::Impulse;
            else cfg.kind = fv1::TestSignalKind::Sine;
            source_ = std::make_unique<fv1::TestSignalSource>(cfg);
            needs_capture_ = false;
        } else {
            source_ = std::make_unique<fv1::LiveInputSource>();
            needs_capture_ = true;
        }

        fv1::RuntimeConfig rc;
        rc.host_sample_rate = host_rate;
        rc.fv1_sample_rate = fv1_rate;
        rc.max_host_block_frames = std::max<std::size_t>(4096u, static_cast<std::size_t>(period_frames) * 4u);
        rc.resampler_quality = std::clamp(resampler_quality, 0, 10);
        if (!runtime_.prepare(rc)) {
            error = QStringLiteral("FV-1 runtime prepare failed.");
            stop();
            return false;
        }
        if (!runtime_.load_program_bytes(reinterpret_cast<const std::uint8_t*>(program.constData()),
                                         static_cast<std::size_t>(program.size()))) {
            error = QStringLiteral("FV-1 program load failed.");
            stop();
            return false;
        }
        runtime_.set_pots(pot0, pot1, pot2);

        const std::size_t queue_frames = std::max<std::size_t>(65536u, analyzer_fft_size * 16u);
        if (!analyzer_.prepare(host_rate, analyzer_fft_size, queue_frames) ||
            !raw_analyzer_.prepare(host_rate, analyzer_fft_size, queue_frames)) {
            error = QStringLiteral("Analyzer prepare failed.");
            stop();
            return false;
        }
        analyzer_.start();
        raw_analyzer_.start();

        fv1::AudioHostConfig hc;
        hc.host_sample_rate = host_rate;
        hc.period_frames = period_frames;
        hc.needs_capture = needs_capture_;
        hc.playback_device = playback_device;
        hc.capture_device = capture_device;
        std::string host_error;
        if (!host_.open(hc, *source_, runtime_, &analyzer_, &raw_analyzer_, &host_error)) {
            error = QString::fromStdString(host_error);
            stop();
            return false;
        }
        host_.set_dsp_enabled(dsp_enabled);
        if (!host_.start(&host_error)) {
            error = QString::fromStdString(host_error);
            stop();
            return false;
        }
        running_ = true;
        return true;
    }

    void stop() noexcept {
        host_.stop();
        host_.set_recorder(nullptr);
        recorder_.stop();
        host_.close();
        analyzer_.stop();
        raw_analyzer_.stop();
        file_source_ = nullptr;
        source_.reset();
        running_ = false;
    }

    bool running() const noexcept { return running_; }
    void set_pots(float a, float b, float c) noexcept {
        if (running_) runtime_.set_pots(a, b, c);
    }
    fv1::AnalysisSnapshot analysis() const { return analyzer_.latest(); }
    fv1::AnalysisSnapshot raw_analysis() const { return raw_analyzer_.latest(); }
    fv1::AudioHostStats host_stats() const noexcept { return host_.stats(); }
    fv1::RuntimeStats runtime_stats() const noexcept { return runtime_.stats(); }
    std::uint64_t analyzer_drops() const noexcept {
        return analyzer_.dropped_frames() + raw_analyzer_.dropped_frames();
    }
    bool using_speex() const noexcept { return runtime_.using_speexdsp(); }
    void set_dsp_enabled(bool enabled) noexcept { host_.set_dsp_enabled(enabled); }
    bool dsp_enabled() const noexcept { return host_.dsp_enabled(); }

    bool file_active() const noexcept { return file_source_ != nullptr; }
    void file_play() noexcept { if (file_source_) file_source_->play(); }
    void file_pause() noexcept { if (file_source_) file_source_->pause(); }
    void file_stop() noexcept { if (file_source_) file_source_->stop(); }
    void file_set_looping(bool enabled) noexcept { if (file_source_) file_source_->set_looping(enabled); }
    bool file_set_loop_region_seconds(double begin_seconds, double end_seconds) noexcept {
        if (!file_source_ || file_source_->file_sample_rate() == 0 || !(end_seconds > begin_seconds)) return false;
        const double rate = static_cast<double>(file_source_->file_sample_rate());
        const auto begin = static_cast<std::uint64_t>(std::llround(begin_seconds * rate));
        const auto end = static_cast<std::uint64_t>(std::llround(end_seconds * rate));
        return file_source_->set_loop_region(begin, end);
    }
    void file_set_crossfade_ms(double milliseconds) noexcept {
        if (file_source_) file_source_->set_loop_crossfade_ms(milliseconds);
    }
    bool file_seek_seconds(double seconds) noexcept {
        return file_source_ ? file_source_->seek_seconds(seconds) : false;
    }
    double file_position_seconds() const noexcept {
        return file_source_ ? file_source_->position_seconds() : 0.0;
    }
    double file_duration_seconds() const noexcept {
        return file_source_ ? file_source_->duration_seconds() : 0.0;
    }
    fv1::TransportState file_state() const noexcept {
        return file_source_ ? file_source_->state() : fv1::TransportState::Stopped;
    }

    bool start_recording(const std::filesystem::path& path, fv1::AudioRecordMode mode, QString& error) {
        if (!running_) {
            error = QStringLiteral("Start an audio session before recording.");
            return false;
        }
        stop_recording();
        std::string recorder_error;
        const auto rate = static_cast<std::uint32_t>(std::llround(runtime_.config().host_sample_rate));
        if (!recorder_.prepare(path, rate, mode, 262144, &recorder_error) ||
            !recorder_.start(&recorder_error)) {
            error = QString::fromStdString(recorder_error);
            recorder_.stop();
            return false;
        }
        host_.set_recorder(&recorder_);
        return true;
    }

    void stop_recording() noexcept {
        host_.set_recorder(nullptr);
        recorder_.stop();
    }

    bool recording() const noexcept { return recorder_.recording(); }
    fv1::AudioRecorderStats recorder_stats() const noexcept { return recorder_.stats(); }
    std::filesystem::path raw_record_path() const { return recorder_.raw_path(); }
    std::filesystem::path processed_record_path() const { return recorder_.processed_path(); }

private:
    fv1::Runtime runtime_;
    fv1::AnalyzerWorker analyzer_;
    fv1::AnalyzerWorker raw_analyzer_;
    fv1::AudioHost host_;
    fv1::AudioRecorder recorder_;
    std::unique_ptr<fv1::AudioSource> source_;
    fv1::FileLoopSource* file_source_{}; // non-owning, valid while source_ owns it
    bool needs_capture_{};
    bool running_{};
};

MainWindow::MainWindow(QWidget* parent, std::function<void(int, const QString&)> startup_progress)
    : QMainWindow(parent),
      session_(std::make_unique<SessionController>()),
      debugger_(std::make_unique<fv1::Debugger>()) {
    setWindowTitle(QStringLiteral("Spin FV-1 Emulator — FV-1 Lab"));
    resize(1680, 980);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks |
                   QMainWindow::AllowTabbedDocks | QMainWindow::GroupedDragging);

    auto startup = [&startup_progress](int percent, const QString& text) {
        if (startup_progress) startup_progress(percent, text);
    };
    startup(26, QStringLiteral("Loading persistent preferences…"));

    QSettings settings;
    theme_name_ = settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString();
    accent_name_ = settings.value(QStringLiteral("ui/accent"), QStringLiteral("Cyan")).toString();
    icon_name_ = settings.value(QStringLiteral("ui/appIcon"), QStringLiteral("Silver")).toString();
    resampler_quality_ = settings.value(QStringLiteral("audio/srcQuality"), 7).toInt();
    analyzer_fft_size_ = static_cast<std::size_t>(
        settings.value(QStringLiteral("analysis/fftSize"), 4096).toUInt());
    if (analyzer_fft_size_ != 1024 && analyzer_fft_size_ != 2048 &&
        analyzer_fft_size_ != 4096 && analyzer_fft_size_ != 8192)
        analyzer_fft_size_ = 4096;
    dsp_enabled_ = settings.value(QStringLiteral("audio/dspEnabled"), true).toBool();
    compare_raw_processed_ = settings.value(QStringLiteral("analysis/rawProcessedOverlay"), true).toBool();

    generator_amplitude_ = settings.value(QStringLiteral("generator/amplitude"), 0.25).toDouble();
    generator_sweep_end_hz_ = settings.value(QStringLiteral("generator/sweepEndHz"), 12000.0).toDouble();
    generator_sweep_seconds_ = settings.value(QStringLiteral("generator/sweepSeconds"), 5.0).toDouble();
    generator_impulse_period_seconds_ = settings.value(QStringLiteral("generator/impulsePeriodSeconds"), 1.0).toDouble();
    file_loop_enabled_ = settings.value(QStringLiteral("fileLoop/enabled"), true).toBool();
    loop_crossfade_ms_ = settings.value(QStringLiteral("fileLoop/crossfadeMs"), 5.0).toDouble();

    ThemeManager::apply(*qApp, theme_name_, accent_name_);
    startup(34, QStringLiteral("Applying theme and application identity…"));

    set_app_icon(icon_name_);
    build_menus();
    build_toolbar();
    startup(46, QStringLiteral("Building program and transport controls…"));
    build_left_dock();
    startup(58, QStringLiteral("Building analyzer and validation workspace…"));
    build_center();
    startup(70, QStringLiteral("Building virtual-chip inspector…"));
    build_right_dock();
    build_status_footer();
    startup(80, QStringLiteral("Enumerating audio devices…"));
    refresh_audio_devices();

    host_rate_combo_->setCurrentText(settings.value(QStringLiteral("audio/hostRate"), QStringLiteral("48000")).toString());
    buffer_combo_->setCurrentText(settings.value(QStringLiteral("audio/buffer"), QStringLiteral("256")).toString());
    clock_combo_->setCurrentText(settings.value(QStringLiteral("audio/fv1Clock"), QStringLiteral("32768")).toString());
    const QString saved_playback = settings.value(QStringLiteral("audio/playbackName")).toString();
    const QString saved_capture = settings.value(QStringLiteral("audio/captureName")).toString();
    if (!saved_playback.isEmpty()) {
        const int i = playback_combo_->findText(saved_playback);
        if (i >= 0) playback_combo_->setCurrentIndex(i);
    }
    if (!saved_capture.isEmpty()) {
        const int i = capture_combo_->findText(saved_capture);
        if (i >= 0) capture_combo_->setCurrentIndex(i);
    }
    set_dsp_enabled(dsp_enabled_);
    set_compare_enabled(compare_raw_processed_);
    startup(90, QStringLiteral("Restoring FV-1 Lab session preferences…"));

    telemetry_timer_ = new QTimer(this);
    telemetry_timer_->setInterval(50);
    connect(telemetry_timer_, &QTimer::timeout, this, [this]{ update_telemetry(); });

    startup(96, QStringLiteral("Finalizing Phase 5B validation testbench…"));
    statusBar()->showMessage(QStringLiteral("Phase 5B hardware validation — ready"));
    log(QStringLiteral("FV-1 Lab GUI initialized."));
    log(QStringLiteral("Runtime connected: live input, file loop, test generator, virtual-clock SRC and dual raw/processed analyzers."));
    log(QStringLiteral("Phase 5B focus: reproducible physical-FV-1 capture packs, comparison and accuracy refinement."));
    log(QStringLiteral("External capture-interface acceptance remains deferred; playback path accepted on Cortana."));
}

MainWindow::~MainWindow() { stop_session(); }

void MainWindow::build_menus() {
    auto* file = menuBar()->addMenu(QStringLiteral("&File"));
    auto* open_program = file->addAction(QStringLiteral("Open FV-1 Program…"));
    connect(open_program, &QAction::triggered, this, [this]{ choose_program(); });
    auto* paste_program = file->addAction(QStringLiteral("Paste SpinASM…"));
    connect(paste_program, &QAction::triggered, this, [this]{ paste_spinasm(); });
    auto* open_audio = file->addAction(QStringLiteral("Open Audio Loop…"));
    connect(open_audio, &QAction::triggered, this, [this]{ choose_audio_file(); });
    file->addSeparator();
    auto* quit = file->addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    auto* audio = menuBar()->addMenu(QStringLiteral("&Audio"));
    auto* audio_settings = audio->addAction(QStringLiteral("Audio Settings…"));
    connect(audio_settings, &QAction::triggered, this, [this]{ show_audio_settings(); });
    auto* refresh_devices = audio->addAction(QStringLiteral("Refresh Audio Devices"));
    connect(refresh_devices, &QAction::triggered, this, [this]{ refresh_audio_devices(); });
    audio->addSeparator();
    auto* generator_settings = audio->addAction(QStringLiteral("Test Generator Settings…"));
    connect(generator_settings, &QAction::triggered, this, [this]{ show_generator_settings(); });
    auto* loop_settings = audio->addAction(QStringLiteral("Audio Loop Region…"));
    connect(loop_settings, &QAction::triggered, this, [this]{ show_loop_region_settings(); });
    audio->addSeparator();
    auto* record = audio->addAction(QStringLiteral("Record Raw / Processed Audio…"));
    connect(record, &QAction::triggered, this, [this]{ start_recording(); });
    auto* stop_record = audio->addAction(QStringLiteral("Stop Recording"));
    connect(stop_record, &QAction::triggered, this, [this]{ stop_recording(); });
    audio->addSeparator();
    auto* bypass = audio->addAction(QStringLiteral("Toggle DSP/FX Bypass"));
    connect(bypass, &QAction::triggered, this, [this]{ set_dsp_enabled(!dsp_enabled_); });

    auto* analysis = menuBar()->addMenu(QStringLiteral("&Analysis"));
    compare_action_ = analysis->addAction(QStringLiteral("Show Raw + Processed Overlay"));
    compare_action_->setCheckable(true);
    compare_action_->setChecked(compare_raw_processed_);
    connect(compare_action_, &QAction::toggled, this, [this](bool enabled){ set_compare_enabled(enabled); });
    analysis->addSeparator();
    auto* freeze_all = analysis->addAction(QStringLiteral("Freeze / Unfreeze All Plots"));
    connect(freeze_all, &QAction::triggered, this, [this] {
        const bool freeze = !(scope_plot_ && scope_plot_->frozen());
        if (scope_plot_) scope_plot_->set_frozen(freeze);
        if (spectrum_plot_) spectrum_plot_->set_frozen(freeze);
        if (spectrogram_plot_) spectrogram_plot_->set_frozen(freeze);
        if (levels_plot_) levels_plot_->set_frozen(freeze);
        statusBar()->showMessage(freeze ? QStringLiteral("Analyzer displays frozen")
                                        : QStringLiteral("Analyzer displays live"), 2500);
    });
    auto* clear_all = analysis->addAction(QStringLiteral("Clear Analyzer Displays"));
    connect(clear_all, &QAction::triggered, this, [this] {
        if (scope_plot_) scope_plot_->clear_display();
        if (spectrum_plot_) spectrum_plot_->clear_display();
        if (spectrogram_plot_) spectrogram_plot_->clear_display();
        if (levels_plot_) levels_plot_->clear_display();
    });

    auto* fft_menu = analysis->addMenu(QStringLiteral("FFT Size (next session)"));
    auto* fft_group = new QActionGroup(fft_menu);
    fft_group->setExclusive(true);
    for (const std::size_t size : {std::size_t{1024}, std::size_t{2048}, std::size_t{4096}, std::size_t{8192}}) {
        auto* action = fft_menu->addAction(QString::number(size));
        action->setCheckable(true);
        action->setChecked(size == analyzer_fft_size_);
        fft_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, size] {
            analyzer_fft_size_ = size;
            QSettings().setValue(QStringLiteral("analysis/fftSize"), static_cast<qulonglong>(size));
            log(QStringLiteral("Analyzer FFT size set to %1 for the next session.").arg(size));
            if (session_ && session_->running())
                statusBar()->showMessage(QStringLiteral("FFT size saved — restart session to apply"), 3500);
        });
    }

    auto* view = menuBar()->addMenu(QStringLiteral("&View"));
    auto* theme_menu = view->addMenu(QStringLiteral("Theme"));
    auto* theme_group = new QActionGroup(theme_menu);
    theme_group->setExclusive(true);
    for (const QString& name : ThemeManager::theme_names()) {
        auto* action = theme_menu->addAction(name);
        action->setCheckable(true);
        action->setChecked(name == theme_name_);
        theme_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, name]{ set_theme(name); });
    }
    auto* accent_menu = view->addMenu(QStringLiteral("Accent Color"));
    auto* accent_group = new QActionGroup(accent_menu);
    accent_group->setExclusive(true);
    for (const QString& name : ThemeManager::accent_names()) {
        auto* action = accent_menu->addAction(name);
        action->setCheckable(true);
        action->setChecked(name == accent_name_);
        accent_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, name]{ set_accent(name); });
    }
    auto* icon_menu = view->addMenu(QStringLiteral("Application Icon"));
    auto* icon_group = new QActionGroup(icon_menu);
    icon_group->setExclusive(true);
    for (const QString& name : {QStringLiteral("Silver"), QStringLiteral("Dark Cyan"),
                                QStringLiteral("Blue"), QStringLiteral("Amber")}) {
        auto* action = icon_menu->addAction(name);
        action->setCheckable(true);
        action->setChecked(name == icon_name_);
        icon_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, name]{ set_app_icon(name); });
    }

    auto* help = menuBar()->addMenu(QStringLiteral("&Help"));
    auto* about = help->addAction(QStringLiteral("About FV-1 Lab…"));
    about->setObjectName(QStringLiteral("aboutFv1LabAction"));
    connect(about, &QAction::triggered, this, [this]{ show_about(); });
}

void MainWindow::show_about() {
    if (auto* existing = findChild<StartupSplash*>(QStringLiteral("fv1AboutWindow"))) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    auto* about = new StartupSplash(accent_name_, this, StartupSplash::Mode::About);
    about->setObjectName(QStringLiteral("fv1AboutWindow"));
    about->setAttribute(Qt::WA_DeleteOnClose, true);
    about->show();
    about->raise();
    about->activateWindow();
}

void MainWindow::build_toolbar() {
    auto* bar = addToolBar(QStringLiteral("Transport"));
    bar->setMovable(false);
    auto* start = bar->addAction(QStringLiteral("▶ Start"));
    auto* stop = bar->addAction(QStringLiteral("■ Stop"));
    record_action_ = bar->addAction(QStringLiteral("● Record"));
    record_action_->setCheckable(true);
    connect(record_action_, &QAction::toggled, this, [this](bool enabled) {
        if (enabled) start_recording(); else stop_recording();
    });
    bar->addSeparator();
    dsp_action_ = bar->addAction(QStringLiteral("DSP/FX ON — PROCESSED"));
    dsp_action_->setCheckable(true);
    dsp_action_->setChecked(dsp_enabled_);
    connect(dsp_action_, &QAction::toggled, this, [this](bool enabled){ set_dsp_enabled(enabled); });
    if (compare_action_) {
        compare_action_->setText(QStringLiteral("RAW + FX OVERLAY"));
        bar->addAction(compare_action_);
    }
    bar->addSeparator();
    auto* open = bar->addAction(QStringLiteral("Open Program"));
    auto* audio_settings = bar->addAction(QStringLiteral("Audio Settings…"));
    connect(open, &QAction::triggered, this, [this]{ choose_program(); });
    connect(audio_settings, &QAction::triggered, this, [this]{ show_audio_settings(); });
    connect(start, &QAction::triggered, this, [this]{ start_session(); });
    connect(stop, &QAction::triggered, this, [this]{ stop_session(); });
}

void MainWindow::build_left_dock() {
    auto* dock = new QDockWidget(QStringLiteral("PROGRAM / SOURCE / CONTROLS"), this);
    dock->setObjectName(QStringLiteral("programDock"));
    dock->setMinimumWidth(320);
    auto* body = new QWidget(dock);
    auto* layout = new QVBoxLayout(body);

    auto* program = new QGroupBox(QStringLiteral("PROGRAM"), body);
    auto* program_layout = new QVBoxLayout(program);
    program_label_ = new QLabel(QStringLiteral("No program loaded"), program);
    program_label_->setWordWrap(true);
    program_label_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(program_label_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(QStringLiteral("Open FV-1 Program…"), this, [this]{ choose_program(); });
        menu.addAction(QStringLiteral("Paste SpinASM…"), this, [this]{ paste_spinasm(); });
        menu.addSeparator();
        auto* inspect = menu.addAction(QStringLiteral("Re-run Resource Analysis"), this, [this]{ inspect_program(); });
        inspect->setEnabled(!program_image_.isEmpty());
        menu.addSeparator();
        auto* reset_debug = menu.addAction(QStringLiteral("Load / Reset Offline Chip Inspector"), this, [this]{ debugger_load_program(); });
        reset_debug->setEnabled(!program_image_.isEmpty());
        menu.exec(program_label_->mapToGlobal(pos));
    });
    auto* program_button = new QPushButton(QStringLiteral("Open .spn / .bin"), program);
    connect(program_button, &QPushButton::clicked, this, [this]{ choose_program(); });
    program_layout->addWidget(program_label_);
    program_layout->addWidget(program_button);
    layout->addWidget(program);

    auto* source = new QGroupBox(QStringLiteral("INPUT SOURCE"), body);
    auto* source_layout = new QVBoxLayout(source);
    source_combo_ = new QComboBox(source);
    source_combo_->addItems({QStringLiteral("Audio Interface"), QStringLiteral("Audio File Loop"), QStringLiteral("Test Generator")});
    source_combo_->setCurrentText(QStringLiteral("Test Generator"));
    source_layout->addWidget(source_combo_);

    file_label_ = new QLabel(QStringLiteral("No audio file selected"), source);
    file_label_->setWordWrap(true);
    source_layout->addWidget(file_label_);

    auto* file_row = new QHBoxLayout;
    auto* browse = new QPushButton(QStringLiteral("Browse…"), source);
    auto* loop_region = new QPushButton(QStringLiteral("Loop Region…"), source);
    connect(browse, &QPushButton::clicked, this, [this]{ choose_audio_file(); });
    connect(loop_region, &QPushButton::clicked, this, [this]{ show_loop_region_settings(); });
    file_row->addWidget(browse);
    file_row->addWidget(loop_region);
    source_layout->addLayout(file_row);

    auto* transport_row = new QHBoxLayout;
    file_play_button_ = new QPushButton(QStringLiteral("▶"), source);
    file_pause_button_ = new QPushButton(QStringLiteral("Ⅱ"), source);
    file_stop_button_ = new QPushButton(QStringLiteral("■"), source);
    file_loop_button_ = new QPushButton(QStringLiteral("↻ LOOP"), source);
    file_loop_button_->setCheckable(true);
    file_loop_button_->setChecked(file_loop_enabled_);
    file_play_button_->setToolTip(QStringLiteral("Play/resume the loaded audio file."));
    file_pause_button_->setToolTip(QStringLiteral("Pause file playback without resetting position."));
    file_stop_button_->setToolTip(QStringLiteral("Stop and return to the loop start."));
    connect(file_play_button_, &QPushButton::clicked, this, [this]{ if (session_) session_->file_play(); });
    connect(file_pause_button_, &QPushButton::clicked, this, [this]{ if (session_) session_->file_pause(); });
    connect(file_stop_button_, &QPushButton::clicked, this, [this]{ if (session_) session_->file_stop(); });
    connect(file_loop_button_, &QPushButton::toggled, this, [this](bool enabled) {
        file_loop_enabled_ = enabled;
        QSettings().setValue(QStringLiteral("fileLoop/enabled"), enabled);
        if (session_) session_->file_set_looping(enabled);
    });
    transport_row->addWidget(file_play_button_);
    transport_row->addWidget(file_pause_button_);
    transport_row->addWidget(file_stop_button_);
    transport_row->addWidget(file_loop_button_, 1);
    source_layout->addLayout(transport_row);

    file_position_slider_ = new QSlider(Qt::Horizontal, source);
    file_position_slider_->setRange(0, 10000);
    file_position_slider_->setValue(0);
    file_position_label_ = new QLabel(QStringLiteral("00:00.000 / 00:00.000"), source);
    file_position_label_->setAlignment(Qt::AlignCenter);
    connect(file_position_slider_, &QSlider::sliderReleased, this, [this] {
        if (!session_ || !session_->file_active() || file_duration_seconds_ <= 0.0) return;
        const double position = file_duration_seconds_ *
            static_cast<double>(file_position_slider_->value()) / 10000.0;
        session_->file_seek_seconds(position);
    });
    source_layout->addWidget(file_position_slider_);
    source_layout->addWidget(file_position_label_);

    auto* generator_row = new QHBoxLayout;
    generator_combo_ = new QComboBox(source);
    generator_combo_->addItems({QStringLiteral("Sine"), QStringLiteral("Sweep"), QStringLiteral("White Noise"),
                                QStringLiteral("Pink Noise"), QStringLiteral("Impulse")});
    generator_frequency_ = new QDoubleSpinBox(source);
    generator_frequency_->setRange(1.0, 20000.0);
    generator_frequency_->setValue(440.0);
    generator_frequency_->setSuffix(QStringLiteral(" Hz"));
    generator_row->addWidget(generator_combo_);
    generator_row->addWidget(generator_frequency_);
    source_layout->addLayout(generator_row);
    auto* generator_settings = new QPushButton(QStringLiteral("Test Generator Settings…"), source);
    connect(generator_settings, &QPushButton::clicked, this, [this]{ show_generator_settings(); });
    source_layout->addWidget(generator_settings);

    source->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(source, &QWidget::customContextMenuRequested, this, [this, source](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(QStringLiteral("Open Audio Loop…"), this, [this]{ choose_audio_file(); });
        menu.addAction(QStringLiteral("Loop Region…"), this, [this]{ show_loop_region_settings(); });
        menu.addAction(QStringLiteral("Test Generator Settings…"), this, [this]{ show_generator_settings(); });
        menu.exec(source->mapToGlobal(pos));
    });
    layout->addWidget(source);

    auto* controls = new QGroupBox(QStringLiteral("FV-1 PARAMETERS"), body);
    auto* form = new QFormLayout(controls);
    pot0_ = make_parameter_slider(600, controls);
    pot1_ = make_parameter_slider(500, controls);
    pot2_ = make_parameter_slider(700, controls);
    form->addRow(QStringLiteral("POT0"), pot0_);
    form->addRow(QStringLiteral("POT1"), pot1_);
    form->addRow(QStringLiteral("POT2"), pot2_);
    form->addRow(QStringLiteral("Dry / Wet"), make_parameter_slider(1000, controls));
    form->addRow(QStringLiteral("Output"), make_parameter_slider(800, controls));
    const auto pot_update = [this] {
        if (session_) session_->set_pots(static_cast<float>(pot0_->value()) / 1000.0f,
                                         static_cast<float>(pot1_->value()) / 1000.0f,
                                         static_cast<float>(pot2_->value()) / 1000.0f);
    };
    connect(pot0_, &QSlider::valueChanged, this, [pot_update](int){ pot_update(); });
    connect(pot1_, &QSlider::valueChanged, this, [pot_update](int){ pot_update(); });
    connect(pot2_, &QSlider::valueChanged, this, [pot_update](int){ pot_update(); });
    layout->addWidget(controls);

    auto* audio = new QGroupBox(QStringLiteral("AUDIO / VIRTUAL CLOCK"), body);
    auto* audio_form = new QFormLayout(audio);
    playback_combo_ = new QComboBox(audio);
    capture_combo_ = new QComboBox(audio);
    host_rate_combo_ = new QComboBox(audio);
    host_rate_combo_->addItems({QStringLiteral("44100"), QStringLiteral("48000"), QStringLiteral("96000"), QStringLiteral("192000")});
    host_rate_combo_->setCurrentText(QStringLiteral("48000"));
    buffer_combo_ = new QComboBox(audio);
    buffer_combo_->addItems({QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("256"), QStringLiteral("512"), QStringLiteral("1024")});
    buffer_combo_->setCurrentText(QStringLiteral("256"));
    clock_combo_ = new QComboBox(audio);
    clock_combo_->addItems({QStringLiteral("32768"), QStringLiteral("46608.4")});
    audio_form->addRow(QStringLiteral("Playback"), playback_combo_);
    audio_form->addRow(QStringLiteral("Capture"), capture_combo_);
    audio_form->addRow(QStringLiteral("Host rate"), host_rate_combo_);
    audio_form->addRow(QStringLiteral("Buffer"), buffer_combo_);
    audio_form->addRow(QStringLiteral("FV-1 clock"), clock_combo_);
    auto* audio_settings_button = new QPushButton(QStringLiteral("Audio Settings…"), audio);
    connect(audio_settings_button, &QPushButton::clicked, this, [this]{ show_audio_settings(); });
    audio_form->addRow(audio_settings_button);
    const auto install_audio_context = [this](QWidget* widget) {
        widget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(widget, &QWidget::customContextMenuRequested, this, [this, widget](const QPoint& pos) {
            QMenu menu(this);
            menu.addAction(QStringLiteral("Audio Settings…"), this, [this]{ show_audio_settings(); });
            menu.addAction(QStringLiteral("Refresh Audio Devices"), this, [this]{ refresh_audio_devices(); });
            menu.exec(widget->mapToGlobal(pos));
        });
    };
    install_audio_context(playback_combo_);
    install_audio_context(capture_combo_);
    install_audio_context(host_rate_combo_);
    install_audio_context(buffer_combo_);
    install_audio_context(clock_combo_);
    layout->addWidget(audio);
    layout->addStretch(1);

    dock->setWidget(body);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::build_center() {
    auto* center = new QWidget(this);
    auto* main = new QVBoxLayout(center);
    main->setContentsMargins(4, 4, 4, 4);

    auto* tabs = new QTabWidget(center);
    scope_plot_ = new InstrumentPlot(PlotKind::Oscilloscope, tabs);
    spectrum_plot_ = new InstrumentPlot(PlotKind::Spectrum, tabs);
    spectrogram_plot_ = new InstrumentPlot(PlotKind::Spectrogram, tabs);
    levels_plot_ = new InstrumentPlot(PlotKind::Levels, tabs);
    spectrum_plot_->set_peak_hold(false);
    spectrum_plot_->set_log_frequency(true);
    spectrogram_plot_->set_db_range(-100.0, 0.0);
    tabs->addTab(scope_plot_, QStringLiteral("OSCILLOSCOPE"));
    tabs->addTab(spectrum_plot_, QStringLiteral("SPECTRUM"));
    tabs->addTab(spectrogram_plot_, QStringLiteral("SPECTROGRAM"));
    tabs->addTab(levels_plot_, QStringLiteral("LEVELS"));
    validation_panel_ = new ValidationPanel(tabs);
    validation_panel_->set_log_callback([this](const QString& message){ log(message); });
    tabs->addTab(validation_panel_, QStringLiteral("VALIDATION"));
    install_plot_context_menu(scope_plot_);
    install_plot_context_menu(spectrum_plot_);
    install_plot_context_menu(spectrogram_plot_);
    install_plot_context_menu(levels_plot_);
    main->addWidget(tabs, 1);

    auto* bottom = new QSplitter(Qt::Horizontal, center);
    bottom->setChildrenCollapsible(false);

    auto* delay_group = new QGroupBox(QStringLiteral("DELAY RAM VIEWER"), bottom);
    auto* delay_layout = new QVBoxLayout(delay_group);
    delay_view_ = new QPlainTextEdit(delay_group);
    delay_view_->setReadOnly(true);
    delay_view_->setMaximumBlockCount(256);
    delay_view_->setPlainText(QStringLiteral(
        "Offline chip inspector ready.\n"
        "Step the virtual FV-1 to view a live window around its physical delay pointer."));
    delay_layout->addWidget(delay_view_);
    bottom->addWidget(delay_group);

    auto* resources = new QGroupBox(QStringLiteral("VIRTUAL DSP RESOURCE USAGE"), bottom);
    auto* resource_layout = new QVBoxLayout(resources);
    auto* resource_form = new QFormLayout;
    program_usage_ = new QProgressBar(resources);
    program_usage_->setRange(0, 128);
    program_usage_->setValue(0);
    program_usage_->setFormat(QStringLiteral("0 / 128 instructions"));
    delay_usage_ = new QProgressBar(resources);
    delay_usage_->setRange(0, 32768);
    delay_usage_->setValue(0);
    delay_usage_->setFormat(QStringLiteral("0 / 32768 words"));
    register_usage_ = new QProgressBar(resources);
    register_usage_->setRange(0, 32);
    register_usage_->setValue(0);
    register_usage_->setFormat(QStringLiteral("0 / 32 registers"));
    sin_lfo_usage_ = new QProgressBar(resources);
    sin_lfo_usage_->setRange(0, 2);
    sin_lfo_usage_->setValue(0);
    sin_lfo_usage_->setFormat(QStringLiteral("0 / 2 SIN LFO"));
    ramp_lfo_usage_ = new QProgressBar(resources);
    ramp_lfo_usage_->setRange(0, 2);
    ramp_lfo_usage_->setValue(0);
    ramp_lfo_usage_->setFormat(QStringLiteral("0 / 2 RAMP LFO"));
    resource_form->addRow(QStringLiteral("Program"), program_usage_);
    resource_form->addRow(QStringLiteral("Delay RAM"), delay_usage_);
    resource_form->addRow(QStringLiteral("Registers"), register_usage_);
    resource_form->addRow(QStringLiteral("SIN LFO"), sin_lfo_usage_);
    resource_form->addRow(QStringLiteral("RAMP LFO"), ramp_lfo_usage_);
    resource_layout->addLayout(resource_form);
    resource_details_ = new QLabel(QStringLiteral(
        "Reads/sample —\nWrites/sample —\nDynamic reads —\nWorst path —\nSKP targets —"), resources);
    resource_details_->setWordWrap(true);
    resource_layout->addWidget(resource_details_);
    bottom->addWidget(resources);

    auto* status = new QGroupBox(QStringLiteral("DSP STATUS"), bottom);
    auto* status_layout = new QVBoxLayout(status);
    runtime_status_ = new QLabel(QStringLiteral("Stopped\nHost/FV-1 runtime ready."), status);
    runtime_status_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    status_layout->addWidget(runtime_status_);
    bottom->addWidget(status);
    bottom->setSizes({360, 430, 310});
    main->addWidget(bottom, 0);
    setCentralWidget(center);
}

void MainWindow::build_right_dock() {
    auto* dock = new QDockWidget(QStringLiteral("CONSOLE / CHIP INSPECTOR"), this);
    dock->setObjectName(QStringLiteral("debugDock"));
    dock->setMinimumWidth(430);
    auto* split = new QSplitter(Qt::Vertical, dock);
    split->setChildrenCollapsible(false);

    auto* console_group = new QGroupBox(QStringLiteral("CONSOLE / LOG"), split);
    auto* console_layout = new QVBoxLayout(console_group);
    console_ = new QPlainTextEdit(console_group);
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(4000);
    console_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(console_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        std::unique_ptr<QMenu> menu(console_->createStandardContextMenu());
        menu->addSeparator();
        menu->addAction(QStringLiteral("Clear Log"), console_, [this]{ console_->clear(); });
        menu->addAction(QStringLiteral("Copy Entire Log"), console_, [this]{
            console_->selectAll();
            console_->copy();
        });
        menu->exec(console_->mapToGlobal(pos));
    });
    console_layout->addWidget(console_);
    split->addWidget(console_group);

    auto* debug_group = new QGroupBox(QStringLiteral("OFFLINE FV-1 CHIP INSPECTOR"), split);
    auto* debug_layout = new QVBoxLayout(debug_group);
    auto* note = new QLabel(QStringLiteral(
        "A private virtual FV-1 is stepped independently of realtime audio. "
        "This is an emulator inspection tool, not a source-code IDE."), debug_group);
    note->setWordWrap(true);
    debug_layout->addWidget(note);

    auto* input_form = new QFormLayout;
    debug_input_left_ = new QDoubleSpinBox(debug_group);
    debug_input_right_ = new QDoubleSpinBox(debug_group);
    for (auto* spin : {debug_input_left_, debug_input_right_}) {
        spin->setRange(-1.0, 1.0);
        spin->setDecimals(4);
        spin->setSingleStep(0.05);
    }
    debug_input_left_->setValue(0.25);
    debug_input_right_->setValue(0.25);
    input_form->addRow(QStringLiteral("Debug input L"), debug_input_left_);
    input_form->addRow(QStringLiteral("Debug input R"), debug_input_right_);
    debug_layout->addLayout(input_form);

    auto* controls = new QHBoxLayout;
    auto* reset = new QPushButton(QStringLiteral("Reset"), debug_group);
    auto* step_instruction = new QPushButton(QStringLiteral("Step Instruction"), debug_group);
    auto* step_sample = new QPushButton(QStringLiteral("Step Sample"), debug_group);
    auto* cont = new QPushButton(QStringLiteral("Continue Sample"), debug_group);
    connect(reset, &QPushButton::clicked, this, [this]{ debugger_reset(); });
    connect(step_instruction, &QPushButton::clicked, this, [this]{ debugger_step_instruction(); });
    connect(step_sample, &QPushButton::clicked, this, [this]{ debugger_step_sample(); });
    connect(cont, &QPushButton::clicked, this, [this]{ debugger_continue_sample(); });
    controls->addWidget(reset);
    controls->addWidget(step_instruction);
    controls->addWidget(step_sample);
    controls->addWidget(cont);
    debug_layout->addLayout(controls);

    debugger_table_ = new QTableWidget(11, 2, debug_group);
    debugger_table_->setHorizontalHeaderLabels({QStringLiteral("State"), QStringLiteral("Value")});
    debugger_table_->horizontalHeader()->setStretchLastSection(true);
    debugger_table_->verticalHeader()->setVisible(false);
    const QStringList names{
        QStringLiteral("PC"), QStringLiteral("Instruction"), QStringLiteral("Opcode"),
        QStringLiteral("ACC"), QStringLiteral("PACC"), QStringLiteral("LR"),
        QStringLiteral("ADDR_PTR"), QStringLiteral("SIN0"), QStringLiteral("SIN1"),
        QStringLiteral("RMP0"), QStringLiteral("RMP1")};
    for (qsizetype i = 0; i < names.size(); ++i) {
        debugger_table_->setItem(static_cast<int>(i), 0, new QTableWidgetItem(names[i]));
        debugger_table_->setItem(static_cast<int>(i), 1, new QTableWidgetItem(QStringLiteral("—")));
    }
    debug_layout->addWidget(debugger_table_);
    split->addWidget(debug_group);

    auto* registers = new QGroupBox(QStringLiteral("REGISTERS / MEMORY"), split);
    auto* register_layout = new QVBoxLayout(registers);
    register_table_ = new QTableWidget(32, 2, registers);
    register_table_->setHorizontalHeaderLabels({QStringLiteral("Register"), QStringLiteral("Q1.23 / Hex")});
    register_table_->horizontalHeader()->setStretchLastSection(true);
    register_table_->verticalHeader()->setVisible(false);
    for (int i = 0; i < 32; ++i) {
        register_table_->setItem(i, 0, new QTableWidgetItem(QStringLiteral("REG%1").arg(i)));
        register_table_->setItem(i, 1, new QTableWidgetItem(QStringLiteral("0x000000")));
    }
    register_layout->addWidget(register_table_);
    split->addWidget(registers);

    split->setSizes({390, 430, 260});
    dock->setWidget(split);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::build_status_footer() {
    copyright_label_ = new QLabel(QStringLiteral("© 2026 Roth Amplification LTD"), this);
    copyright_label_->setToolTip(QStringLiteral("Spin FV-1 Emulator / FV-1 Lab"));
    copyright_label_->setContentsMargins(10, 0, 8, 0);
    statusBar()->addPermanentWidget(copyright_label_);
}

void MainWindow::install_plot_context_menu(InstrumentPlot* plot) {
    if (!plot) return;
    plot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(plot, &QWidget::customContextMenuRequested, this, [this, plot](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(dsp_enabled_ ? QStringLiteral("Bypass DSP/FX — View Raw Signal")
                                    : QStringLiteral("Enable DSP/FX — View Processed Signal"),
                       this, [this]{ set_dsp_enabled(!dsp_enabled_); });

        if (plot->kind() == PlotKind::Oscilloscope || plot->kind() == PlotKind::Spectrum) {
            auto* compare = menu.addAction(QStringLiteral("Raw + Processed Overlay"));
            compare->setCheckable(true);
            compare->setChecked(compare_raw_processed_);
            connect(compare, &QAction::toggled, this, [this](bool enabled){ set_compare_enabled(enabled); });
        }

        menu.addSeparator();
        auto* freeze = menu.addAction(QStringLiteral("Freeze Display"));
        freeze->setCheckable(true);
        freeze->setChecked(plot->frozen());
        connect(freeze, &QAction::toggled, plot, [plot](bool enabled){ plot->set_frozen(enabled); });
        menu.addAction(QStringLiteral("Clear Display"), plot, [plot]{ plot->clear_display(); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Copy Plot Image"), this, [plot] {
            QApplication::clipboard()->setPixmap(plot->grab());
        });
        menu.addAction(QStringLiteral("Save Plot Image…"), this, [this, plot] {
            const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Analyzer Plot"), {},
                QStringLiteral("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
            if (!path.isEmpty() && !plot->grab().save(path))
                QMessageBox::warning(this, QStringLiteral("Save Plot"), QStringLiteral("Could not write the selected image file."));
        });
        menu.addAction(QStringLiteral("Export Current Data CSV…"), this, [this, plot] {
            if (!session_) return;
            const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Analyzer Data"), {},
                QStringLiteral("CSV data (*.csv)"));
            if (path.isEmpty()) return;
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, QStringLiteral("Export Analyzer Data"), QStringLiteral("Could not create the selected CSV file."));
                return;
            }
            QTextStream out(&file);
            const auto processed = session_->analysis();
            const auto raw = session_->raw_analysis();
            if (plot->kind() == PlotKind::Oscilloscope) {
                out << "index,processed_left,processed_right,raw_left,raw_right\n";
                const std::size_t n = std::max(processed.scope_frames.size(), raw.scope_frames.size());
                for (std::size_t i = 0; i < n; ++i) {
                    out << static_cast<qulonglong>(i) << ',';
                    if (i < processed.scope_frames.size())
                        out << processed.scope_frames[i].left << ',' << processed.scope_frames[i].right;
                    else out << ',';
                    out << ',';
                    if (i < raw.scope_frames.size())
                        out << raw.scope_frames[i].left << ',' << raw.scope_frames[i].right;
                    else out << ',';
                    out << '\n';
                }
            } else if (plot->kind() == PlotKind::Spectrum || plot->kind() == PlotKind::Spectrogram) {
                out << "frequency_hz,processed_db,raw_db\n";
                const std::size_t n = std::max(processed.spectrum_db.size(), raw.spectrum_db.size());
                const double rate = processed.sample_rate > 0.0 ? processed.sample_rate : raw.sample_rate;
                for (std::size_t i = 0; i < n; ++i) {
                    const double hz = n > 1 ? static_cast<double>(i) * rate * 0.5 / static_cast<double>(n - 1) : 0.0;
                    out << hz << ',';
                    if (i < processed.spectrum_db.size()) out << processed.spectrum_db[i];
                    out << ',';
                    if (i < raw.spectrum_db.size()) out << raw.spectrum_db[i];
                    out << '\n';
                }
            } else {
                out << "stream,peak_left,peak_right,rms_left,rms_right,correlation,dominant_hz,dominant_db\n";
                const auto write_levels = [&out](const char* name, const fv1::AnalysisSnapshot& a) {
                    out << name << ',' << a.peak_left << ',' << a.peak_right << ','
                        << a.rms_left << ',' << a.rms_right << ',' << a.correlation << ','
                        << a.dominant_frequency_hz << ',' << a.dominant_level_db << '\n';
                };
                write_levels("processed", processed);
                write_levels("raw", raw);
            }
            log(QStringLiteral("Analyzer data exported: ") + path);
        });

        if (plot->kind() == PlotKind::Oscilloscope) {
            auto* time = menu.addMenu(QStringLiteral("Time Zoom"));
            for (const double zoom : {1.0, 2.0, 4.0, 8.0, 16.0}) {
                auto* action = time->addAction(QStringLiteral("%1x").arg(zoom, 0, 'g', 3));
                action->setCheckable(true);
                action->setChecked(std::abs(plot->time_zoom() - zoom) < 0.001);
                connect(action, &QAction::triggered, plot, [plot, zoom]{ plot->set_time_zoom(zoom); });
            }
            auto* gain = menu.addMenu(QStringLiteral("Vertical Gain"));
            for (const double value : {0.5, 1.0, 2.0, 4.0, 8.0}) {
                auto* action = gain->addAction(QStringLiteral("%1x").arg(value, 0, 'g', 3));
                action->setCheckable(true);
                action->setChecked(std::abs(plot->vertical_gain() - value) < 0.001);
                connect(action, &QAction::triggered, plot, [plot, value]{ plot->set_vertical_gain(value); });
            }

            auto* trigger = menu.addMenu(QStringLiteral("Trigger"));
            auto* mode_menu = trigger->addMenu(QStringLiteral("Mode"));
            struct TriggerModeItem { const char* label; TriggerMode mode; };
            const TriggerModeItem modes[]{
                {"Off", TriggerMode::Off}, {"Auto", TriggerMode::Auto},
                {"Normal", TriggerMode::Normal}, {"Single", TriggerMode::Single}};
            for (const auto& item : modes) {
                auto* action = mode_menu->addAction(QString::fromLatin1(item.label));
                action->setCheckable(true);
                action->setChecked(plot->trigger_mode() == item.mode);
                connect(action, &QAction::triggered, plot, [plot, mode = item.mode]{ plot->set_trigger_mode(mode); });
            }
            auto* channel = trigger->addMenu(QStringLiteral("Source"));
            auto* left = channel->addAction(QStringLiteral("Left"));
            auto* right = channel->addAction(QStringLiteral("Right"));
            left->setCheckable(true); right->setCheckable(true);
            left->setChecked(plot->trigger_channel() == TriggerChannel::Left);
            right->setChecked(plot->trigger_channel() == TriggerChannel::Right);
            connect(left, &QAction::triggered, plot, [plot]{ plot->set_trigger_channel(TriggerChannel::Left); });
            connect(right, &QAction::triggered, plot, [plot]{ plot->set_trigger_channel(TriggerChannel::Right); });
            auto* slope = trigger->addMenu(QStringLiteral("Slope"));
            auto* rising = slope->addAction(QStringLiteral("Rising ↗"));
            auto* falling = slope->addAction(QStringLiteral("Falling ↘"));
            rising->setCheckable(true); falling->setCheckable(true);
            rising->setChecked(plot->trigger_slope() == TriggerSlope::Rising);
            falling->setChecked(plot->trigger_slope() == TriggerSlope::Falling);
            connect(rising, &QAction::triggered, plot, [plot]{ plot->set_trigger_slope(TriggerSlope::Rising); });
            connect(falling, &QAction::triggered, plot, [plot]{ plot->set_trigger_slope(TriggerSlope::Falling); });
            trigger->addAction(QStringLiteral("Set Trigger Level…"), this, [this, plot] {
                bool ok = false;
                const double value = QInputDialog::getDouble(this, QStringLiteral("Oscilloscope Trigger"),
                    QStringLiteral("Trigger level (-1.0 … +1.0)"), plot->trigger_level(), -1.0, 1.0, 3, &ok);
                if (ok) plot->set_trigger_level(value);
            });
            auto* rearm = trigger->addAction(QStringLiteral("Re-arm Single Trigger"), plot, [plot]{ plot->rearm_single_trigger(); });
            rearm->setEnabled(plot->trigger_mode() == TriggerMode::Single);
        } else if (plot->kind() == PlotKind::Spectrum) {
            auto* log_axis = menu.addAction(QStringLiteral("Logarithmic Frequency Axis"));
            log_axis->setCheckable(true);
            log_axis->setChecked(plot->log_frequency());
            connect(log_axis, &QAction::toggled, plot, [plot](bool enabled){ plot->set_log_frequency(enabled); });
            auto* peak_hold = menu.addAction(QStringLiteral("Peak Hold"));
            peak_hold->setCheckable(true);
            peak_hold->setChecked(plot->peak_hold());
            connect(peak_hold, &QAction::toggled, plot, [plot](bool enabled){ plot->set_peak_hold(enabled); });
            auto* db = menu.addMenu(QStringLiteral("dB Range"));
            for (const int floor : {-60, -80, -100, -120}) {
                auto* action = db->addAction(QStringLiteral("%1..0 dB").arg(floor));
                connect(action, &QAction::triggered, plot, [plot, floor]{ plot->set_db_range(floor, 0.0); });
            }
        } else if (plot->kind() == PlotKind::Spectrogram) {
            auto* history = menu.addMenu(QStringLiteral("History Width"));
            for (const std::size_t columns : {std::size_t{80}, std::size_t{160}, std::size_t{320}, std::size_t{640}}) {
                auto* action = history->addAction(QStringLiteral("%1 columns").arg(columns));
                connect(action, &QAction::triggered, plot, [plot, columns]{ plot->set_spectrogram_history_columns(columns); });
            }
            auto* db = menu.addMenu(QStringLiteral("Dynamic Range"));
            for (const int floor : {-60, -80, -100, -120}) {
                auto* action = db->addAction(QStringLiteral("%1..0 dB").arg(floor));
                connect(action, &QAction::triggered, plot, [plot, floor]{ plot->set_db_range(floor, 0.0); });
            }
        }

        menu.exec(plot->mapToGlobal(pos));
    });
}


void MainWindow::refresh_audio_devices() {
    if (!playback_combo_ || !capture_combo_) return;
    playback_combo_->clear(); capture_combo_->clear();
    playback_combo_->addItem(QStringLiteral("OS Default"), -1);
    capture_combo_->addItem(QStringLiteral("OS Default"), -1);
    std::string error;
    const auto devices = fv1::AudioHost::enumerate(&error);
    for (const auto& d : devices) {
        const QString label = QString::fromStdString(d.name) + (d.is_default ? QStringLiteral("  (default)") : QString());
        if (d.direction == fv1::AudioDeviceDirection::Playback) playback_combo_->addItem(label, static_cast<int>(d.index));
        else capture_combo_->addItem(label, static_cast<int>(d.index));
    }
    if (!error.empty()) log(QStringLiteral("Audio enumeration: ") + QString::fromStdString(error));
    else log(QStringLiteral("Audio devices enumerated through Phase-2 miniaudio backend."));
}

void MainWindow::show_audio_settings() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Audio Settings"));
    dialog.setMinimumWidth(560);

    auto* outer = new QVBoxLayout(&dialog);
    auto* note = new QLabel(
        session_ && session_->running()
            ? QStringLiteral("The current audio session keeps its existing settings. Changes below apply the next time Start is pressed.")
            : QStringLiteral("Configure the Linux audio host and virtual FV-1 clock. These settings are remembered between launches."),
        &dialog);
    note->setWordWrap(true);
    outer->addWidget(note);

    auto* form = new QFormLayout;
    auto* backend = new QLabel(QStringLiteral("miniaudio / system audio"), &dialog);
    auto* playback = new QComboBox(&dialog);
    auto* capture = new QComboBox(&dialog);
    auto* host_rate = new QComboBox(&dialog);
    auto* buffer = new QComboBox(&dialog);
    auto* clock = new QComboBox(&dialog);
    auto* quality = new QSpinBox(&dialog);
    quality->setRange(0, 10);
    quality->setValue(resampler_quality_);
    quality->setToolTip(QStringLiteral("SpeexDSP resampler quality: 0 = lightest CPU load, 10 = highest quality."));

    const auto clone_combo = [](QComboBox* dst, const QComboBox* src) {
        dst->clear();
        for (int i = 0; i < src->count(); ++i) dst->addItem(src->itemText(i), src->itemData(i));
        dst->setCurrentIndex(std::max(0, src->currentIndex()));
    };
    clone_combo(playback, playback_combo_);
    clone_combo(capture, capture_combo_);
    clone_combo(host_rate, host_rate_combo_);
    clone_combo(buffer, buffer_combo_);
    clone_combo(clock, clock_combo_);

    form->addRow(QStringLiteral("Backend"), backend);
    form->addRow(QStringLiteral("Playback device"), playback);
    form->addRow(QStringLiteral("Capture device"), capture);
    form->addRow(QStringLiteral("Host sample rate"), host_rate);
    form->addRow(QStringLiteral("Buffer / period"), buffer);
    form->addRow(QStringLiteral("Virtual FV-1 clock"), clock);
    form->addRow(QStringLiteral("SRC quality"), quality);
    outer->addLayout(form);

    auto* refresh = new QPushButton(QStringLiteral("Refresh Audio Devices"), &dialog);
    connect(refresh, &QPushButton::clicked, &dialog, [this, playback, capture] {
        const QString playback_name = playback->currentText();
        const QString capture_name = capture->currentText();
        playback->clear(); capture->clear();
        playback->addItem(QStringLiteral("OS Default"), -1);
        capture->addItem(QStringLiteral("OS Default"), -1);
        std::string error;
        const auto devices = fv1::AudioHost::enumerate(&error);
        for (const auto& d : devices) {
            const QString label = QString::fromStdString(d.name) + (d.is_default ? QStringLiteral("  (default)") : QString());
            if (d.direction == fv1::AudioDeviceDirection::Playback) playback->addItem(label, static_cast<int>(d.index));
            else capture->addItem(label, static_cast<int>(d.index));
        }
        const int p = playback->findText(playback_name); if (p >= 0) playback->setCurrentIndex(p);
        const int c = capture->findText(capture_name); if (c >= 0) capture->setCurrentIndex(c);
        if (!error.empty()) log(QStringLiteral("Audio enumeration: ") + QString::fromStdString(error));
        else log(QStringLiteral("Audio devices refreshed from Audio Settings dialog."));
    });
    outer->addWidget(refresh);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) return;

    const auto apply_combo = [](QComboBox* dst, const QComboBox* src) {
        const QVariant device_data = src->currentData();
        const int by_data = dst->findData(device_data);
        if (by_data >= 0) dst->setCurrentIndex(by_data);
        else dst->setCurrentText(src->currentText());
    };
    apply_combo(playback_combo_, playback);
    apply_combo(capture_combo_, capture);
    host_rate_combo_->setCurrentText(host_rate->currentText());
    buffer_combo_->setCurrentText(buffer->currentText());
    clock_combo_->setCurrentText(clock->currentText());
    resampler_quality_ = quality->value();

    QSettings settings;
    settings.setValue(QStringLiteral("audio/playbackName"), playback_combo_->currentText());
    settings.setValue(QStringLiteral("audio/captureName"), capture_combo_->currentText());
    settings.setValue(QStringLiteral("audio/hostRate"), host_rate_combo_->currentText());
    settings.setValue(QStringLiteral("audio/buffer"), buffer_combo_->currentText());
    settings.setValue(QStringLiteral("audio/fv1Clock"), clock_combo_->currentText());
    settings.setValue(QStringLiteral("audio/srcQuality"), resampler_quality_);

    log(QStringLiteral("Audio settings: playback '%1', capture '%2', host %3 Hz, buffer %4, FV-1 %5 Hz, SRC quality %6.")
        .arg(playback_combo_->currentText(), capture_combo_->currentText(), host_rate_combo_->currentText(),
             buffer_combo_->currentText(), clock_combo_->currentText()).arg(resampler_quality_));
    if (session_ && session_->running())
        statusBar()->showMessage(QStringLiteral("Audio settings saved — restart session to apply"), 5000);
}

void MainWindow::show_generator_settings() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Test Generator Settings"));
    dialog.setMinimumWidth(480);
    auto* outer = new QVBoxLayout(&dialog);
    auto* note = new QLabel(QStringLiteral(
        "Configure deterministic lab stimuli. The frequency field in the main panel remains the sine/sweep start frequency."), &dialog);
    note->setWordWrap(true);
    outer->addWidget(note);

    auto* form = new QFormLayout;
    auto* amplitude = new QDoubleSpinBox(&dialog);
    amplitude->setRange(0.0, 1.0);
    amplitude->setDecimals(3);
    amplitude->setSingleStep(0.05);
    amplitude->setValue(generator_amplitude_);
    auto* sweep_end = new QDoubleSpinBox(&dialog);
    sweep_end->setRange(20.0, 24000.0);
    sweep_end->setDecimals(1);
    sweep_end->setSuffix(QStringLiteral(" Hz"));
    sweep_end->setValue(generator_sweep_end_hz_);
    auto* sweep_seconds = new QDoubleSpinBox(&dialog);
    sweep_seconds->setRange(0.1, 60.0);
    sweep_seconds->setDecimals(2);
    sweep_seconds->setSuffix(QStringLiteral(" s"));
    sweep_seconds->setValue(generator_sweep_seconds_);
    auto* impulse_period = new QDoubleSpinBox(&dialog);
    impulse_period->setRange(0.01, 60.0);
    impulse_period->setDecimals(3);
    impulse_period->setSuffix(QStringLiteral(" s"));
    impulse_period->setValue(generator_impulse_period_seconds_);
    form->addRow(QStringLiteral("Amplitude"), amplitude);
    form->addRow(QStringLiteral("Sweep end"), sweep_end);
    form->addRow(QStringLiteral("Sweep duration"), sweep_seconds);
    form->addRow(QStringLiteral("Impulse period"), impulse_period);
    outer->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;

    generator_amplitude_ = amplitude->value();
    generator_sweep_end_hz_ = sweep_end->value();
    generator_sweep_seconds_ = sweep_seconds->value();
    generator_impulse_period_seconds_ = impulse_period->value();
    QSettings settings;
    settings.setValue(QStringLiteral("generator/amplitude"), generator_amplitude_);
    settings.setValue(QStringLiteral("generator/sweepEndHz"), generator_sweep_end_hz_);
    settings.setValue(QStringLiteral("generator/sweepSeconds"), generator_sweep_seconds_);
    settings.setValue(QStringLiteral("generator/impulsePeriodSeconds"), generator_impulse_period_seconds_);
    log(QStringLiteral("Generator settings: amplitude %1, sweep to %2 Hz in %3 s, impulse period %4 s.")
        .arg(generator_amplitude_, 0, 'f', 3)
        .arg(generator_sweep_end_hz_, 0, 'f', 1)
        .arg(generator_sweep_seconds_, 0, 'f', 2)
        .arg(generator_impulse_period_seconds_, 0, 'f', 3));
    if (session_ && session_->running() && source_combo_->currentText() == QStringLiteral("Test Generator"))
        statusBar()->showMessage(QStringLiteral("Generator settings saved — restart session to apply"), 3500);
}

void MainWindow::show_loop_region_settings() {
    if (audio_file_path_.isEmpty() || file_duration_seconds_ <= 0.0) {
        QMessageBox::information(this, QStringLiteral("Loop Region"),
            QStringLiteral("Load an audio file first, then define its loop region."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Audio Loop Region"));
    dialog.setMinimumWidth(480);
    auto* outer = new QVBoxLayout(&dialog);
    auto* file = new QLabel(QFileInfo(audio_file_path_).fileName(), &dialog);
    file->setWordWrap(true);
    outer->addWidget(file);

    auto* form = new QFormLayout;
    auto* begin = new QDoubleSpinBox(&dialog);
    auto* end = new QDoubleSpinBox(&dialog);
    for (auto* spin : {begin, end}) {
        spin->setRange(0.0, file_duration_seconds_);
        spin->setDecimals(3);
        spin->setSingleStep(0.050);
        spin->setSuffix(QStringLiteral(" s"));
    }
    begin->setValue(std::clamp(loop_begin_seconds_, 0.0, file_duration_seconds_));
    end->setValue(std::clamp(loop_end_seconds_ > 0.0 ? loop_end_seconds_ : file_duration_seconds_, 0.0, file_duration_seconds_));
    auto* crossfade = new QDoubleSpinBox(&dialog);
    crossfade->setRange(0.0, 500.0);
    crossfade->setDecimals(1);
    crossfade->setSingleStep(1.0);
    crossfade->setSuffix(QStringLiteral(" ms"));
    crossfade->setValue(loop_crossfade_ms_);
    crossfade->setToolTip(QStringLiteral("Short equal-time overlap at the loop boundary to suppress clicks. 0 ms disables it."));
    form->addRow(QStringLiteral("Loop start"), begin);
    form->addRow(QStringLiteral("Loop end"), end);
    form->addRow(QStringLiteral("Boundary crossfade"), crossfade);
    outer->addLayout(form);

    auto* reset = new QPushButton(QStringLiteral("Use Entire File"), &dialog);
    connect(reset, &QPushButton::clicked, &dialog, [begin, end, this] {
        begin->setValue(0.0);
        end->setValue(file_duration_seconds_);
    });
    outer->addWidget(reset);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    if (!(end->value() > begin->value())) {
        QMessageBox::warning(this, QStringLiteral("Loop Region"), QStringLiteral("Loop end must be after loop start."));
        return;
    }

    loop_begin_seconds_ = begin->value();
    loop_end_seconds_ = end->value();
    loop_crossfade_ms_ = crossfade->value();
    QSettings().setValue(QStringLiteral("fileLoop/crossfadeMs"), loop_crossfade_ms_);
    if (session_ && session_->file_active()) {
        session_->file_set_loop_region_seconds(loop_begin_seconds_, loop_end_seconds_);
        session_->file_set_crossfade_ms(loop_crossfade_ms_);
        session_->file_seek_seconds(loop_begin_seconds_);
    }
    log(QStringLiteral("Loop region: %1 → %2, crossfade %3 ms.")
        .arg(format_time(loop_begin_seconds_), format_time(loop_end_seconds_))
        .arg(loop_crossfade_ms_, 0, 'f', 1));
    update_file_transport_ui();
}


void MainWindow::choose_program() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open FV-1 Program"), {},
        QStringLiteral("FV-1 Programs (*.spn *.bin);;All files (*)"));
    if (path.isEmpty()) return;

    QByteArray bytes;
    QString error;
    if (!load_program_image(path, bytes, error)) {
        QMessageBox::warning(this, QStringLiteral("FV-1 Program"), error);
        log(QStringLiteral("Program load failed: ") + error);
        return;
    }
    install_program_image(bytes, path, path);
}

bool MainWindow::install_program_image(const QByteArray& bytes, const QString& display_name, const QString& source_path) {
    if (bytes.size() != static_cast<qsizetype>(FV1_PROGRAM_BYTES)) {
        log(QStringLiteral("Program rejected: image is %1 bytes; expected %2.").arg(bytes.size()).arg(FV1_PROGRAM_BYTES));
        return false;
    }
    if (session_ && session_->running()) stop_session();
    program_image_ = bytes;
    program_path_ = source_path;
    program_display_name_ = display_name;
    if (program_label_) program_label_->setText(display_name);
    log(QStringLiteral("Program loaded: ") + display_name);
    inspect_program();
    return true;
}

void MainWindow::paste_spinasm() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Paste SpinASM Program"));
    dialog.resize(900, 650);

    auto* outer = new QVBoxLayout(&dialog);
    auto* intro = new QLabel(QStringLiteral(
        "Paste FV-1 / SpinASM assembly below. Compile & Load uses the same project assembler and the same 512-byte program path as opening a .spn file."), &dialog);
    intro->setWordWrap(true);
    outer->addWidget(intro);

    auto* editor = new QPlainTextEdit(&dialog);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setTabStopDistance(editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4.0);
    if (!pasted_spinasm_source_.isEmpty()) {
        editor->setPlainText(pasted_spinasm_source_);
    } else {
        editor->setPlainText(QStringLiteral(
            "; Paste FV-1 SpinASM here\n"
            "; Simple stereo passthrough example:\n"
            "RDAX ADCL, 1.0\n"
            "WRAX DACL, 0\n"
            "RDAX ADCR, 1.0\n"
            "WRAX DACR, 0\n"));
    }
    outer->addWidget(editor, 1);

    auto* status = new QPlainTextEdit(&dialog);
    status->setReadOnly(true);
    status->setMaximumHeight(105);
    status->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    status->setPlainText(QStringLiteral("Ready — paste SpinASM source and choose Compile & Load."));
    outer->addWidget(status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto* compile = buttons->addButton(QStringLiteral("Compile & Load"), QDialogButtonBox::ActionRole);
    compile->setDefault(true);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(compile, &QPushButton::clicked, &dialog, [this, &dialog, editor, status, compile] {
        const QString source_text = editor->toPlainText();
        if (source_text.trimmed().isEmpty()) {
            status->setPlainText(QStringLiteral("ERROR: SpinASM source is empty."));
            return;
        }
        compile->setEnabled(false);
        status->setPlainText(QStringLiteral("Compiling pasted SpinASM with native compiler…"));
        QApplication::processEvents();

        const QByteArray source_bytes = source_text.toUtf8();
        QByteArray bytes;
        std::uint32_t instruction_count = 0;
        std::uint32_t highest_delay = 0;
        try {
            const auto compiled = fv1::spinasm::compile(std::string_view(
                source_bytes.constData(), static_cast<std::size_t>(source_bytes.size())));
            bytes = QByteArray(reinterpret_cast<const char*>(compiled.image.data()),
                               static_cast<qsizetype>(compiled.image.size()));
            instruction_count = compiled.instruction_count;
            highest_delay = compiled.highest_delay_address;
        } catch (const fv1::spinasm::CompileError& compile_error) {
            compile->setEnabled(true);
            const QString detail = QString::fromStdString(compile_error.what());
            status->setPlainText(QStringLiteral("COMPILE ERROR\n") + detail);
            log(QStringLiteral("Pasted SpinASM compilation failed: ") + detail);
            return;
        }
        compile->setEnabled(true);

        const QString summary = QStringLiteral(
            "Compiled successfully — %1 / 128 instructions; highest delay address %2; 512-byte program loaded.")
            .arg(instruction_count).arg(highest_delay);
        status->setPlainText(summary);
        pasted_spinasm_source_ = source_text;
        if (!install_program_image(bytes, QStringLiteral("Pasted SpinASM Program"))) {
            status->appendPlainText(QStringLiteral("ERROR: Emulator rejected the compiled image."));
            return;
        }
        log(summary);
        dialog.accept();
    });
    outer->addWidget(buttons);
    dialog.exec();
}

void MainWindow::choose_audio_file() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Test Audio"), {},
        QStringLiteral("Wave audio (*.wav);;All files (*)"));
    if (path.isEmpty()) return;

    fv1::FileLoopSource probe;
    std::string error;
    if (!probe.load(std::filesystem::path(path.toStdString()), &error)) {
        QMessageBox::warning(this, QStringLiteral("Audio File"), QString::fromStdString(error));
        log(QStringLiteral("Audio loop load failed: ") + QString::fromStdString(error));
        return;
    }

    audio_file_path_ = path;
    file_duration_seconds_ = probe.duration_seconds();
    loop_begin_seconds_ = 0.0;
    loop_end_seconds_ = file_duration_seconds_;
    file_label_->setText(QStringLiteral("%1\n%2 Hz • %3 s")
        .arg(path).arg(probe.file_sample_rate()).arg(file_duration_seconds_, 0, 'f', 3));
    file_position_slider_->setValue(0);
    update_file_transport_ui();
    source_combo_->setCurrentText(QStringLiteral("Audio File Loop"));
    log(QStringLiteral("Loop source selected: %1 (%2 Hz, %3 s).")
        .arg(path).arg(probe.file_sample_rate()).arg(file_duration_seconds_, 0, 'f', 3));
}

void MainWindow::inspect_program() {
    if (program_image_.isEmpty()) return;
    const QByteArray& bytes = program_image_;
    fv1_config cfg{32768.0, FV1_DELAY_REFERENCE_16};
    fv1_engine* engine = fv1_create(&cfg);
    if (!engine) {
        log(QStringLiteral("Program inspection failed: cannot create FV-1 engine."));
        return;
    }
    const auto result = fv1_load_bytes(engine,
        reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size()));
    fv1_resource_report report{};
    if (result == FV1_OK && fv1_analyze_program(engine, &report) == FV1_OK) {
        program_usage_->setValue(static_cast<int>(report.used_instructions));
        program_usage_->setFormat(QStringLiteral("%1 / 128 instructions").arg(report.used_instructions));
        delay_usage_->setValue(static_cast<int>(std::min<std::uint32_t>(report.highest_static_delay_address, FV1_DELAY_WORDS)));
        delay_usage_->setFormat(QStringLiteral("%1 / 32768 highest static address").arg(report.highest_static_delay_address));
        register_usage_->setValue(static_cast<int>(report.general_registers_used));
        register_usage_->setFormat(QStringLiteral("%1 / 32 registers").arg(report.general_registers_used));
        sin_lfo_usage_->setValue(static_cast<int>(report.sine_lfos_used));
        sin_lfo_usage_->setFormat(QStringLiteral("%1 / 2 SIN LFO").arg(report.sine_lfos_used));
        ramp_lfo_usage_->setValue(static_cast<int>(report.ramp_lfos_used));
        ramp_lfo_usage_->setFormat(QStringLiteral("%1 / 2 RAMP LFO").arg(report.ramp_lfos_used));
        resource_details_->setText(QStringLiteral(
            "Static delay reads/sample  %1\n"
            "Static delay writes/sample %2\n"
            "Dynamic delay reads         %3\n"
            "Worst instruction path      %4 / 128\n"
            "SKP instructions            %5\n"
            "POT inputs used              %6 / 3")
            .arg(report.static_delay_reads)
            .arg(report.static_delay_writes)
            .arg(report.dynamic_delay_reads)
            .arg(report.worst_case_path)
            .arg(report.skip_instructions)
            .arg(report.pots_used));
        log(QStringLiteral(
            "Resource analysis: %1 instructions, delay <= %2, %3 registers, %4 POTs, worst path %5.")
            .arg(report.used_instructions)
            .arg(report.highest_static_delay_address)
            .arg(report.general_registers_used)
            .arg(report.pots_used)
            .arg(report.worst_case_path));
    }
    fv1_destroy(engine);

    if (debugger_ && debugger_->load_program(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size())))) {
        debugger_refresh();
        log(QStringLiteral("Offline chip inspector loaded the selected program."));
    }
}

void MainWindow::start_session() {
    if (session_->running()) stop_session();
    if (program_image_.isEmpty()) {
        log(QStringLiteral("Start refused: open or paste an FV-1 program first."));
        statusBar()->showMessage(QStringLiteral("Open or paste an FV-1 program first"), 5000);
        return;
    }
    const QByteArray& bytes = program_image_;
    QString error;

    const auto host_rate = static_cast<std::uint32_t>(host_rate_combo_->currentText().toUInt());
    const auto buffer = static_cast<std::uint32_t>(buffer_combo_->currentText().toUInt());
    const double clock = clock_combo_->currentText().toDouble();
    const int playback = playback_combo_->currentData().toInt();
    const int capture = capture_combo_->currentData().toInt();
    if (!session_->start(bytes,
                         source_combo_->currentText(),
                         audio_file_path_,
                         generator_combo_->currentText(),
                         generator_frequency_->value(),
                         generator_amplitude_,
                         generator_sweep_end_hz_,
                         generator_sweep_seconds_,
                         generator_impulse_period_seconds_,
                         file_loop_enabled_,
                         loop_begin_seconds_,
                         loop_end_seconds_,
                         loop_crossfade_ms_,
                         host_rate,
                         buffer,
                         clock,
                         analyzer_fft_size_,
                         playback,
                         capture,
                         resampler_quality_,
                         dsp_enabled_,
                         static_cast<float>(pot0_->value()) / 1000.0f,
                         static_cast<float>(pot1_->value()) / 1000.0f,
                         static_cast<float>(pot2_->value()) / 1000.0f,
                         error)) {
        log(QStringLiteral("Session start failed: ") + error);
        statusBar()->showMessage(QStringLiteral("Session start failed"), 5000);
        return;
    }
    telemetry_timer_->start();
    log(QStringLiteral(
        "Realtime session started: %1, host %2 Hz, virtual FV-1 %3 Hz, buffer %4, FFT %5, %6.")
        .arg(source_combo_->currentText())
        .arg(host_rate)
        .arg(clock, 0, 'f', 1)
        .arg(buffer)
        .arg(analyzer_fft_size_)
        .arg(dsp_enabled_ ? QStringLiteral("DSP/FX processed")
                          : QStringLiteral("DSP/FX bypassed — raw monitor")));
    if (compare_raw_processed_)
        log(QStringLiteral("Dual analyzer taps active: raw source and processed output are available for overlay."));
    statusBar()->showMessage(QStringLiteral("RUNNING"));
    update_file_transport_ui();
}

void MainWindow::stop_session() {
    if (!session_) return;
    const bool was_running = session_->running();
    if (session_->recording()) stop_recording();
    if (telemetry_timer_) telemetry_timer_->stop();
    session_->stop();
    if (runtime_status_) runtime_status_->setText(QStringLiteral("Stopped\nHost/FV-1 runtime ready."));
    if (was_running) log(QStringLiteral("Realtime session stopped."));
    if (statusBar()) statusBar()->showMessage(QStringLiteral("Stopped"));
}

void MainWindow::start_recording() {
    if (!session_ || !session_->running()) {
        if (record_action_) {
            const bool blocked = record_action_->blockSignals(true);
            record_action_->setChecked(false);
            record_action_->blockSignals(blocked);
        }
        QMessageBox::information(this, QStringLiteral("Record Audio"),
            QStringLiteral("Start a realtime FV-1 session before recording."));
        return;
    }
    if (session_->recording()) return;

    bool ok = false;
    const QStringList choices{
        QStringLiteral("Processed output"),
        QStringLiteral("Raw input"),
        QStringLiteral("Raw + processed (two files)")};
    const QString choice = QInputDialog::getItem(this, QStringLiteral("Record Audio"),
        QStringLiteral("Capture stream"), choices, 2, false, &ok);
    if (!ok) {
        if (record_action_) {
            const bool blocked = record_action_->blockSignals(true);
            record_action_->setChecked(false);
            record_action_->blockSignals(blocked);
        }
        return;
    }

    QString suggested = QDir::home().filePath(QStringLiteral("fv1-capture.wav"));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Record FV-1 Audio"), suggested,
        QStringLiteral("Wave audio (*.wav)"));
    if (path.isEmpty()) {
        if (record_action_) {
            const bool blocked = record_action_->blockSignals(true);
            record_action_->setChecked(false);
            record_action_->blockSignals(blocked);
        }
        return;
    }

    fv1::AudioRecordMode mode = fv1::AudioRecordMode::RawAndProcessed;
    if (choice == choices[0]) mode = fv1::AudioRecordMode::Processed;
    else if (choice == choices[1]) mode = fv1::AudioRecordMode::Raw;

    QString error;
    if (!session_->start_recording(std::filesystem::path(path.toStdString()), mode, error)) {
        QMessageBox::warning(this, QStringLiteral("Record Audio"), error);
        log(QStringLiteral("Recording failed: ") + error);
        if (record_action_) {
            const bool blocked = record_action_->blockSignals(true);
            record_action_->setChecked(false);
            record_action_->blockSignals(blocked);
        }
        return;
    }

    if (record_action_) {
        const bool blocked = record_action_->blockSignals(true);
        record_action_->setChecked(true);
        record_action_->setText(QStringLiteral("● Recording…"));
        record_action_->blockSignals(blocked);
    }
    QStringList paths;
    if (!session_->raw_record_path().empty()) paths << QString::fromStdString(session_->raw_record_path().string());
    if (!session_->processed_record_path().empty()) paths << QString::fromStdString(session_->processed_record_path().string());
    log(QStringLiteral("Audio recording started: %1").arg(paths.join(QStringLiteral(" | "))));
    statusBar()->showMessage(QStringLiteral("RECORDING — realtime-safe WAV capture"), 3000);
}

void MainWindow::stop_recording() {
    if (!session_ || !session_->recording()) {
        if (record_action_) {
            const bool blocked = record_action_->blockSignals(true);
            record_action_->setChecked(false);
            record_action_->setText(QStringLiteral("● Record"));
            record_action_->blockSignals(blocked);
        }
        return;
    }
    const auto raw_path = session_->raw_record_path();
    const auto processed_path = session_->processed_record_path();
    session_->stop_recording();
    const auto stats = session_->recorder_stats();
    if (record_action_) {
        const bool blocked = record_action_->blockSignals(true);
        record_action_->setChecked(false);
        record_action_->setText(QStringLiteral("● Record"));
        record_action_->blockSignals(blocked);
    }
    log(QStringLiteral("Recording stopped: raw %1 frames (%2 dropped), processed %3 frames (%4 dropped).")
        .arg(stats.raw_frames_written).arg(stats.raw_frames_dropped)
        .arg(stats.processed_frames_written).arg(stats.processed_frames_dropped));
    if (!raw_path.empty()) log(QStringLiteral("Raw capture: ") + QString::fromStdString(raw_path.string()));
    if (!processed_path.empty()) log(QStringLiteral("Processed capture: ") + QString::fromStdString(processed_path.string()));
    statusBar()->showMessage(QStringLiteral("Recording finalized"), 2500);
}

void MainWindow::update_telemetry() {
    if (!session_ || !session_->running()) return;
    const auto processed = session_->analysis();
    const auto raw = session_->raw_analysis();

    scope_plot_->set_snapshot(processed);
    spectrum_plot_->set_snapshot(processed);
    spectrogram_plot_->set_snapshot(processed);
    levels_plot_->set_snapshot(processed);
    if (compare_raw_processed_) {
        scope_plot_->set_secondary_snapshot(raw);
        spectrum_plot_->set_secondary_snapshot(raw);
    }

    const auto hs = session_->host_stats();
    const auto rs = session_->runtime_stats();
    runtime_status_->setText(QStringLiteral(
        "RUNNING — %10\n"
        "Host frames      %1\n"
        "FV-1 frames      %2\n"
        "Callback CPU     %3%\n"
        "Underruns        %4\n"
        "Analyzer drops   %5\n"
        "RMS L/R          %6 / %7\n"
        "Dominant         %8 Hz\n"
        "SRC              %9\n"
        "FFT              %11\n"
        "Raw tap RMS      %12 / %13\n"
        "Recording        %14\n"
        "Record drops     %15")
        .arg(hs.source_frames)
        .arg(rs.fv1_frames)
        .arg(hs.callback_cpu_load * 100.0, 0, 'f', 2)
        .arg(rs.output_underrun_frames)
        .arg(session_->analyzer_drops())
        .arg(processed.rms_left, 0, 'f', 3)
        .arg(processed.rms_right, 0, 'f', 3)
        .arg(processed.dominant_frequency_hz, 0, 'f', 1)
        .arg(session_->using_speex() ? QStringLiteral("SpeexDSP") : QStringLiteral("linear fallback"))
        .arg(dsp_enabled_ ? QStringLiteral("DSP/FX ON") : QStringLiteral("BYPASS / RAW"))
        .arg(analyzer_fft_size_)
        .arg(raw.rms_left, 0, 'f', 3)
        .arg(raw.rms_right, 0, 'f', 3)
        .arg(session_->recording() ? QStringLiteral("YES") : QStringLiteral("no"))
        .arg([this] {
            const auto r = session_->recorder_stats();
            return r.raw_frames_dropped + r.processed_frames_dropped;
        }()));

    update_file_transport_ui();
}


void MainWindow::update_file_transport_ui() {
    if (!file_position_slider_ || !file_position_label_) return;

    double duration = file_duration_seconds_;
    double position = 0.0;
    fv1::TransportState state = fv1::TransportState::Stopped;
    bool active = session_ && session_->file_active();

    if (active) {
        duration = session_->file_duration_seconds();
        position = session_->file_position_seconds();
        state = session_->file_state();
        if (duration > 0.0) file_duration_seconds_ = duration;
    }

    if (!std::isfinite(duration) || duration < 0.0) duration = 0.0;
    if (!std::isfinite(position) || position < 0.0) position = 0.0;
    if (duration > 0.0) position = std::min(position, duration);

    if (!file_position_slider_->isSliderDown()) {
        const int slider = duration > 0.0
            ? static_cast<int>(std::llround(std::clamp(position / duration, 0.0, 1.0) * 10000.0))
            : 0;
        file_position_slider_->setValue(slider);
    }

    QString state_text = QStringLiteral("STOPPED");
    if (state == fv1::TransportState::Playing) state_text = QStringLiteral("PLAYING");
    else if (state == fv1::TransportState::Paused) state_text = QStringLiteral("PAUSED");

    file_position_label_->setText(QStringLiteral("%1 / %2   %3")
        .arg(format_time(position), format_time(duration), state_text));

    if (file_play_button_) file_play_button_->setEnabled(active);
    if (file_pause_button_) file_pause_button_->setEnabled(active);
    if (file_stop_button_) file_stop_button_->setEnabled(active);
    if (file_position_slider_) file_position_slider_->setEnabled(active || duration > 0.0);
}

void MainWindow::set_compare_enabled(bool enabled) {
    compare_raw_processed_ = enabled;

    if (scope_plot_) scope_plot_->set_compare_enabled(enabled);
    if (spectrum_plot_) spectrum_plot_->set_compare_enabled(enabled);

    if (!enabled) {
        if (scope_plot_) scope_plot_->set_secondary_snapshot({});
        if (spectrum_plot_) spectrum_plot_->set_secondary_snapshot({});
    } else if (session_ && session_->running()) {
        const auto raw = session_->raw_analysis();
        if (scope_plot_) scope_plot_->set_secondary_snapshot(raw);
        if (spectrum_plot_) spectrum_plot_->set_secondary_snapshot(raw);
    }

    if (compare_action_) {
        const bool blocked = compare_action_->blockSignals(true);
        compare_action_->setChecked(enabled);
        compare_action_->blockSignals(blocked);
    }

    QSettings().setValue(QStringLiteral("analysis/rawProcessedOverlay"), enabled);
    if (statusBar()) {
        statusBar()->showMessage(enabled
            ? QStringLiteral("Raw + processed analyzer overlay enabled")
            : QStringLiteral("Processed analyzer view only"), 2500);
    }
}

void MainWindow::debugger_load_program() {
    if (!debugger_) return;
    if (program_image_.isEmpty()) {
        log(QStringLiteral("Chip inspector: open or paste an FV-1 program first."));
        return;
    }

    const QByteArray& bytes = program_image_;

    if (!debugger_->load_program(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size())))) {
        log(QStringLiteral("Chip inspector load failed: emulator rejected the program image."));
        return;
    }

    debugger_refresh();
    log(QStringLiteral("Offline chip inspector loaded/reset: ") + (program_display_name_.isEmpty() ? QStringLiteral("FV-1 program") : program_display_name_));
}

void MainWindow::debugger_reset() {
    if (!debugger_) return;
    debugger_->reset(true);
    debugger_refresh();
    log(QStringLiteral("Offline chip inspector reset; delay RAM cleared."));
}

void MainWindow::debugger_step_instruction() {
    if (!debugger_) return;
    const float left = debug_input_left_ ? static_cast<float>(debug_input_left_->value()) : 0.0f;
    const float right = debug_input_right_ ? static_cast<float>(debug_input_right_->value()) : 0.0f;
    debugger_->set_input(left, right);
    fv1::DebugStep step{};
    if (!debugger_->step_instruction(step)) {
        log(QStringLiteral("Chip inspector: instruction step failed."));
        return;
    }
    debugger_refresh();
}

void MainWindow::debugger_step_sample() {
    if (!debugger_) return;
    const float left = debug_input_left_ ? static_cast<float>(debug_input_left_->value()) : 0.0f;
    const float right = debug_input_right_ ? static_cast<float>(debug_input_right_->value()) : 0.0f;
    debugger_->set_input(left, right);
    fv1::DebugStep step{};
    if (!debugger_->step_sample(step)) {
        log(QStringLiteral("Chip inspector: sample step failed."));
        return;
    }
    debugger_refresh();
    log(QStringLiteral("Chip inspector completed one virtual sample: DAC %1 / %2.")
        .arg(debugger_->last_output_left(), 0, 'f', 5)
        .arg(debugger_->last_output_right(), 0, 'f', 5));
}

void MainWindow::debugger_continue_sample() {
    if (!debugger_) return;
    const float left = debug_input_left_ ? static_cast<float>(debug_input_left_->value()) : 0.0f;
    const float right = debug_input_right_ ? static_cast<float>(debug_input_right_->value()) : 0.0f;
    debugger_->set_input(left, right);
    fv1::DebugStep step{};
    if (!debugger_->step_sample(step)) {
        log(QStringLiteral("Chip inspector: continue-to-end-of-sample failed."));
        return;
    }
    debugger_refresh();
}

void MainWindow::debugger_refresh() {
    if (!debugger_) return;
    const fv1_snapshot snapshot = debugger_->snapshot();
    const fv1_trace trace = debugger_->last_step().trace;

    const auto q23 = [](std::int32_t value) {
        return static_cast<double>(value) / 8388608.0;
    };
    const auto hex24 = [](std::int32_t value) {
        return QStringLiteral("0x%1").arg(static_cast<std::uint32_t>(value) & 0x00ffffffu, 6, 16, QChar('0')).toUpper();
    };
    const auto set_state = [this](int row, const QString& value) {
        if (!debugger_table_ || row < 0 || row >= debugger_table_->rowCount()) return;
        auto* item = debugger_table_->item(row, 1);
        if (!item) {
            item = new QTableWidgetItem;
            debugger_table_->setItem(row, 1, item);
        }
        item->setText(value);
    };

    set_state(0, QStringLiteral("%1 / 128").arg(snapshot.program_counter));
    if (trace.raw_instruction != 0 || trace.pc_before != 0 || trace.pc_after != 0) {
        set_state(1, QStringLiteral("0x%1").arg(trace.raw_instruction, 8, 16, QChar('0')).toUpper());
        set_state(2, QStringLiteral("%1 (0x%2)")
            .arg(QString::fromLatin1(fv1_opcode_name(trace.opcode)))
            .arg(trace.opcode, 2, 16, QChar('0')).toUpper());
    } else {
        set_state(1, QStringLiteral("—"));
        set_state(2, QStringLiteral("—"));
    }
    set_state(3, QStringLiteral("%1   %2").arg(q23(snapshot.acc), 0, 'f', 6).arg(hex24(snapshot.acc)));
    set_state(4, QStringLiteral("%1   %2").arg(q23(snapshot.pacc), 0, 'f', 6).arg(hex24(snapshot.pacc)));
    set_state(5, QStringLiteral("%1   %2").arg(q23(snapshot.lr), 0, 'f', 6).arg(hex24(snapshot.lr)));
    set_state(6, QStringLiteral("%1").arg(snapshot.regs[FV1_REG_ADDR_PTR]));
    set_state(7, QStringLiteral("%1").arg(snapshot.sin_lfo[0]));
    set_state(8, QStringLiteral("%1").arg(snapshot.sin_lfo[1]));
    set_state(9, QStringLiteral("%1").arg(snapshot.ramp_lfo[0]));
    set_state(10, QStringLiteral("%1").arg(snapshot.ramp_lfo[1]));

    if (register_table_) {
        for (int i = 0; i < 32; ++i) {
            const std::int32_t value = snapshot.regs[FV1_REG0 + i];
            auto* item = register_table_->item(i, 1);
            if (!item) {
                item = new QTableWidgetItem;
                register_table_->setItem(i, 1, item);
            }
            item->setText(QStringLiteral("%1   %2").arg(q23(value), 0, 'f', 6).arg(hex24(value)));
        }
    }

    if (delay_view_) {
        QString text;
        QTextStream out(&text);
        const std::uint32_t center = snapshot.delay_pointer & (FV1_DELAY_WORDS - 1u);
        out << "Physical delay pointer: " << center << " / " << FV1_DELAY_WORDS - 1u << '\n';
        out << "REFERENCE_16 delay model — physical memory window\n\n";
        out << " Address       Q1.23        Hex\n";
        out << "-----------------------------------\n";
        constexpr int radius = 12;
        for (int offset = -radius; offset <= radius; ++offset) {
            const std::uint32_t address =
                (center + FV1_DELAY_WORDS + static_cast<std::uint32_t>(offset + FV1_DELAY_WORDS)) %
                FV1_DELAY_WORDS;
            std::int32_t value = 0;
            debugger_->read_delay_word(address, value);
            out << (offset == 0 ? "> " : "  ")
                << QStringLiteral("%1").arg(address, 5, 10, QChar('0')) << "    "
                << QStringLiteral("%1").arg(q23(value), 0, 'f', 6) << "    "
                << hex24(value) << '\n';
        }
        delay_view_->setPlainText(text);
    }
}

void MainWindow::log(const QString& text) {
    if (console_) console_->appendPlainText(text);
}

void MainWindow::update_signal_monitor_labels() {
    const QString label = dsp_enabled_ ? QStringLiteral("PROCESSED") : QStringLiteral("RAW INPUT / DSP BYPASS");
    if (scope_plot_) scope_plot_->set_signal_label(label);
    if (spectrum_plot_) spectrum_plot_->set_signal_label(label);
    if (spectrogram_plot_) spectrogram_plot_->set_signal_label(label);
    if (levels_plot_) levels_plot_->set_signal_label(label);
}

void MainWindow::set_dsp_enabled(bool enabled) {
    dsp_enabled_ = enabled;
    if (session_) session_->set_dsp_enabled(enabled);
    if (dsp_action_) {
        const bool blocked = dsp_action_->blockSignals(true);
        dsp_action_->setChecked(enabled);
        dsp_action_->setText(enabled ? QStringLiteral("DSP/FX ON — PROCESSED")
                                     : QStringLiteral("DSP/FX BYPASS — RAW"));
        dsp_action_->blockSignals(blocked);
    }
    update_signal_monitor_labels();
    QSettings settings;
    settings.setValue(QStringLiteral("audio/dspEnabled"), enabled);
    if (console_) log(enabled ? QStringLiteral("DSP/FX processing enabled; analyzers monitor processed output.")
                              : QStringLiteral("DSP/FX BYPASS enabled; output and analyzers monitor the raw source."));
    if (statusBar()) statusBar()->showMessage(enabled ? QStringLiteral("DSP/FX ON — processed signal")
                                                     : QStringLiteral("DSP/FX BYPASS — raw signal"), 3000);
}

void MainWindow::set_theme(const QString& theme_name) {
    theme_name_ = theme_name;
    ThemeManager::apply(*qApp, theme_name_, accent_name_);
    QSettings settings; settings.setValue(QStringLiteral("ui/theme"), theme_name_);
    update();
    log(QStringLiteral("Theme: ") + theme_name_);
}

void MainWindow::set_accent(const QString& accent_name) {
    accent_name_ = accent_name;
    ThemeManager::apply(*qApp, theme_name_, accent_name_);
    QSettings settings; settings.setValue(QStringLiteral("ui/accent"), accent_name_);
    update();
    log(QStringLiteral("Accent: ") + accent_name_);
}

void MainWindow::set_app_icon(const QString& icon_name) {
    const QString path = find_icon_asset(icon_name);
    if (path.isEmpty()) {
        if (console_) log(QStringLiteral("Application icon asset not found: ") + icon_name);
        return;
    }
    const QIcon icon(path);
    if (icon.isNull()) {
        if (console_) log(QStringLiteral("Application icon could not be loaded: ") + path);
        return;
    }
    icon_name_ = icon_name;
    QSettings().setValue(QStringLiteral("ui/appIcon"), icon_name_);
    if (qApp) qApp->setWindowIcon(icon);
    setWindowIcon(icon);
    if (console_) log(QStringLiteral("Application icon: ") + icon_name_);
}

} // namespace fv1::gui
