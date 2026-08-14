#include <fv1/gui/main_window.hpp>

#include <fv1/analysis.hpp>
#include <fv1/audio_host.hpp>
#include <fv1/audio_source.hpp>
#include <fv1/fv1.h>
#include <fv1/gui/instrument_plot.hpp>
#include <fv1/gui/theme_manager.hpp>
#include <fv1/runtime.hpp>

#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
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

QWidget* titled_text_panel(const QString& title, const QString& text, QWidget* parent = nullptr) {
    auto* group = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(group);
    auto* edit = new QPlainTextEdit(group);
    edit->setReadOnly(true);
    edit->setPlainText(text);
    layout->addWidget(edit);
    return group;
}

QString find_assembler_script() {
    const QByteArray env = qgetenv("FV1_ASSEMBLER_SCRIPT");
    if (!env.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(env))) return QString::fromLocal8Bit(env);

    const QStringList candidates{
        QDir::current().filePath(QStringLiteral("tools/fv1_assembler.py")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../tools/fv1_assembler.py")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../libexec/spin-fv1-emulator/fv1_assembler.py"))
    };
    for (const QString& path : candidates) {
        const QString clean = QDir::cleanPath(path);
        if (QFileInfo::exists(clean)) return clean;
    }
    return {};
}

bool load_program_image(const QString& path, QByteArray& bytes, QString& error) {
    QString actual = path;
    std::unique_ptr<QTemporaryDir> temp;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("spn")) {
        const QString assembler = find_assembler_script();
        if (assembler.isEmpty()) {
            error = QStringLiteral("Cannot locate tools/fv1_assembler.py. Set FV1_ASSEMBLER_SCRIPT if running an installed build.");
            return false;
        }
        temp = std::make_unique<QTemporaryDir>();
        if (!temp->isValid()) { error = QStringLiteral("Cannot create temporary directory for SpinASM output."); return false; }
        actual = temp->filePath(QStringLiteral("program.bin"));
        QProcess proc;
        proc.start(QStringLiteral("python3"), {assembler, path, actual});
        if (!proc.waitForStarted(3000) || !proc.waitForFinished(30000) || proc.exitCode() != 0) {
            error = QStringLiteral("SpinASM failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError()));
            return false;
        }
    } else if (suffix != QStringLiteral("bin")) {
        error = QStringLiteral("Phase 3 GUI currently opens .spn and 512-byte .bin programs. HEX/bank selection remains available in fv1-cli/fv1-live.");
        return false;
    }

    QFile file(actual);
    if (!file.open(QIODevice::ReadOnly)) { error = QStringLiteral("Cannot open %1").arg(actual); return false; }
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
               std::uint32_t host_rate,
               std::uint32_t period_frames,
               double fv1_rate,
               int playback_device,
               int capture_device,
               float pot0, float pot1, float pot2,
               QString& error) {
        stop();

        if (source_mode == QStringLiteral("Audio File Loop")) {
            if (audio_file.isEmpty()) { error = QStringLiteral("Choose an audio file before starting File Loop mode."); return false; }
            auto file = std::make_unique<fv1::FileLoopSource>();
            std::string source_error;
            if (!file->load(std::filesystem::path(audio_file.toStdString()), &source_error)) {
                error = QString::fromStdString(source_error); return false;
            }
            file->set_looping(true);
            file->play();
            source_ = std::move(file);
            needs_capture_ = false;
        } else if (source_mode == QStringLiteral("Test Generator")) {
            fv1::TestSignalConfig cfg;
            cfg.frequency_hz = generator_frequency;
            if (generator_kind == QStringLiteral("Sweep")) {
                cfg.kind = fv1::TestSignalKind::Sweep; cfg.sweep_end_hz = 12000.0; cfg.sweep_seconds = 5.0;
            } else if (generator_kind == QStringLiteral("White Noise")) cfg.kind = fv1::TestSignalKind::WhiteNoise;
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
        rc.resampler_quality = 7;
        if (!runtime_.prepare(rc)) { error = QStringLiteral("FV-1 runtime prepare failed."); stop(); return false; }
        if (!runtime_.load_program_bytes(reinterpret_cast<const std::uint8_t*>(program.constData()), static_cast<std::size_t>(program.size()))) {
            error = QStringLiteral("FV-1 program load failed."); stop(); return false;
        }
        runtime_.set_pots(pot0, pot1, pot2);

        if (!analyzer_.prepare(host_rate, 4096, 65536)) { error = QStringLiteral("Analyzer prepare failed."); stop(); return false; }
        analyzer_.start();

        fv1::AudioHostConfig hc;
        hc.host_sample_rate = host_rate;
        hc.period_frames = period_frames;
        hc.needs_capture = needs_capture_;
        hc.playback_device = playback_device;
        hc.capture_device = capture_device;
        std::string host_error;
        if (!host_.open(hc, *source_, runtime_, &analyzer_, &host_error)) {
            error = QString::fromStdString(host_error); stop(); return false;
        }
        if (!host_.start(&host_error)) {
            error = QString::fromStdString(host_error); stop(); return false;
        }
        running_ = true;
        return true;
    }

    void stop() noexcept {
        host_.stop();
        host_.close();
        analyzer_.stop();
        source_.reset();
        running_ = false;
    }

    bool running() const noexcept { return running_; }
    void set_pots(float a, float b, float c) noexcept { if (running_) runtime_.set_pots(a,b,c); }
    fv1::AnalysisSnapshot analysis() const { return analyzer_.latest(); }
    fv1::AudioHostStats host_stats() const noexcept { return host_.stats(); }
    fv1::RuntimeStats runtime_stats() const noexcept { return runtime_.stats(); }
    std::uint64_t analyzer_drops() const noexcept { return analyzer_.dropped_frames(); }
    bool using_speex() const noexcept { return runtime_.using_speexdsp(); }

private:
    fv1::Runtime runtime_;
    fv1::AnalyzerWorker analyzer_;
    fv1::AudioHost host_;
    std::unique_ptr<fv1::AudioSource> source_;
    bool needs_capture_{};
    bool running_{};
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), session_(std::make_unique<SessionController>()) {
    setWindowTitle(QStringLiteral("Spin FV-1 Emulator — FV-1 Lab"));
    resize(1680, 980);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks |
                   QMainWindow::AllowTabbedDocks | QMainWindow::GroupedDragging);

    QSettings settings;
    theme_name_ = settings.value(QStringLiteral("ui/theme"), QStringLiteral("Dark")).toString();
    accent_name_ = settings.value(QStringLiteral("ui/accent"), QStringLiteral("Cyan")).toString();
    ThemeManager::apply(*qApp, theme_name_, accent_name_);

    build_menus();
    build_toolbar();
    build_left_dock();
    build_center();
    build_right_dock();
    refresh_audio_devices();

    telemetry_timer_ = new QTimer(this);
    telemetry_timer_->setInterval(50);
    connect(telemetry_timer_, &QTimer::timeout, this, [this]{ update_telemetry(); });

    statusBar()->showMessage(QStringLiteral("Phase 3 — ready"));
    log(QStringLiteral("FV-1 Lab GUI initialized."));
    log(QStringLiteral("Phase-2 runtime connected: live input, file loop, test generator, virtual-clock SRC and analyzer."));
    log(QStringLiteral("External capture-interface acceptance remains deferred; playback path accepted on Cortana."));
}

MainWindow::~MainWindow() { stop_session(); }

void MainWindow::build_menus() {
    auto* file = menuBar()->addMenu(QStringLiteral("&File"));
    auto* open_program = file->addAction(QStringLiteral("Open FV-1 Program…"));
    connect(open_program, &QAction::triggered, this, [this]{ choose_program(); });
    auto* open_audio = file->addAction(QStringLiteral("Open Audio Loop…"));
    connect(open_audio, &QAction::triggered, this, [this]{ choose_audio_file(); });
    file->addSeparator();
    auto* quit = file->addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

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
}

void MainWindow::build_toolbar() {
    auto* bar = addToolBar(QStringLiteral("Transport"));
    bar->setMovable(false);
    auto* start = bar->addAction(QStringLiteral("▶ Start"));
    auto* stop = bar->addAction(QStringLiteral("■ Stop"));
    bar->addSeparator();
    auto* open = bar->addAction(QStringLiteral("Open Program"));
    connect(open, &QAction::triggered, this, [this]{ choose_program(); });
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
    auto* file_row = new QHBoxLayout;
    auto* browse = new QPushButton(QStringLiteral("Browse…"), source);
    auto* loop = new QPushButton(QStringLiteral("↻ Loop"), source); loop->setCheckable(true); loop->setChecked(true);
    connect(browse, &QPushButton::clicked, this, [this]{ choose_audio_file(); });
    file_row->addWidget(browse); file_row->addWidget(loop);
    source_layout->addWidget(file_label_);
    source_layout->addLayout(file_row);

    auto* generator_row = new QHBoxLayout;
    generator_combo_ = new QComboBox(source);
    generator_combo_->addItems({QStringLiteral("Sine"), QStringLiteral("Sweep"), QStringLiteral("White Noise"), QStringLiteral("Pink Noise"), QStringLiteral("Impulse")});
    generator_frequency_ = new QDoubleSpinBox(source);
    generator_frequency_->setRange(1.0, 20000.0); generator_frequency_->setValue(440.0); generator_frequency_->setSuffix(QStringLiteral(" Hz"));
    generator_row->addWidget(generator_combo_); generator_row->addWidget(generator_frequency_);
    source_layout->addLayout(generator_row);
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
        if (session_) session_->set_pots(static_cast<float>(pot0_->value())/1000.0f,
                                         static_cast<float>(pot1_->value())/1000.0f,
                                         static_cast<float>(pot2_->value())/1000.0f);
    };
    connect(pot0_, &QSlider::valueChanged, this, [pot_update](int){ pot_update(); });
    connect(pot1_, &QSlider::valueChanged, this, [pot_update](int){ pot_update(); });
    connect(pot2_, &QSlider::valueChanged, this, [pot_update](int){ pot_update(); });
    layout->addWidget(controls);

    auto* audio = new QGroupBox(QStringLiteral("AUDIO / VIRTUAL CLOCK"), body);
    auto* audio_form = new QFormLayout(audio);
    playback_combo_ = new QComboBox(audio);
    capture_combo_ = new QComboBox(audio);
    host_rate_combo_ = new QComboBox(audio); host_rate_combo_->addItems({QStringLiteral("44100"), QStringLiteral("48000"), QStringLiteral("96000"), QStringLiteral("192000")}); host_rate_combo_->setCurrentText(QStringLiteral("48000"));
    buffer_combo_ = new QComboBox(audio); buffer_combo_->addItems({QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("256"), QStringLiteral("512"), QStringLiteral("1024")}); buffer_combo_->setCurrentText(QStringLiteral("256"));
    clock_combo_ = new QComboBox(audio); clock_combo_->addItems({QStringLiteral("32768"), QStringLiteral("46608.4")});
    audio_form->addRow(QStringLiteral("Playback"), playback_combo_);
    audio_form->addRow(QStringLiteral("Capture"), capture_combo_);
    audio_form->addRow(QStringLiteral("Host rate"), host_rate_combo_);
    audio_form->addRow(QStringLiteral("Buffer"), buffer_combo_);
    audio_form->addRow(QStringLiteral("FV-1 clock"), clock_combo_);
    layout->addWidget(audio);
    layout->addStretch(1);

    dock->setWidget(body);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::build_center() {
    auto* center = new QWidget(this);
    auto* main = new QVBoxLayout(center);
    main->setContentsMargins(4,4,4,4);

    auto* tabs = new QTabWidget(center);
    scope_plot_ = new InstrumentPlot(PlotKind::Oscilloscope, tabs);
    spectrum_plot_ = new InstrumentPlot(PlotKind::Spectrum, tabs);
    spectrogram_plot_ = new InstrumentPlot(PlotKind::Spectrogram, tabs);
    levels_plot_ = new InstrumentPlot(PlotKind::Levels, tabs);
    tabs->addTab(scope_plot_, QStringLiteral("OSCILLOSCOPE"));
    tabs->addTab(spectrum_plot_, QStringLiteral("SPECTRUM"));
    tabs->addTab(spectrogram_plot_, QStringLiteral("SPECTROGRAM"));
    tabs->addTab(levels_plot_, QStringLiteral("LEVELS"));
    main->addWidget(tabs, 1);

    auto* bottom = new QSplitter(Qt::Horizontal, center);
    bottom->setChildrenCollapsible(false);
    bottom->addWidget(titled_text_panel(QStringLiteral("DELAY RAM VIEWER"),
        QStringLiteral("32,768 words\n\nAddress activity map / waveform view\nwill consume debugger telemetry."), bottom));

    auto* resources = new QGroupBox(QStringLiteral("VIRTUAL DSP RESOURCE USAGE"), bottom);
    auto* resource_form = new QFormLayout(resources);
    program_usage_ = new QProgressBar(resources); program_usage_->setRange(0,128); program_usage_->setValue(0); program_usage_->setFormat(QStringLiteral("0 / 128 instructions"));
    delay_usage_ = new QProgressBar(resources); delay_usage_->setRange(0,32768); delay_usage_->setValue(0); delay_usage_->setFormat(QStringLiteral("0 / 32768 words"));
    auto* regs = new QProgressBar(resources); regs->setRange(0,32); regs->setValue(0); regs->setFormat(QStringLiteral("0 / 32 registers"));
    auto* sin = new QProgressBar(resources); sin->setRange(0,2); sin->setValue(0); sin->setFormat(QStringLiteral("0 / 2 SIN LFO"));
    auto* ramp = new QProgressBar(resources); ramp->setRange(0,2); ramp->setValue(0); ramp->setFormat(QStringLiteral("0 / 2 RAMP LFO"));
    resource_form->addRow(QStringLiteral("Program"), program_usage_);
    resource_form->addRow(QStringLiteral("Delay RAM"), delay_usage_);
    resource_form->addRow(QStringLiteral("Registers"), regs);
    resource_form->addRow(QStringLiteral("SIN LFO"), sin);
    resource_form->addRow(QStringLiteral("RAMP LFO"), ramp);
    bottom->addWidget(resources);

    auto* status = new QGroupBox(QStringLiteral("DSP STATUS"), bottom);
    auto* status_layout = new QVBoxLayout(status);
    runtime_status_ = new QLabel(QStringLiteral("Stopped\nHost/FV-1 runtime ready."), status);
    runtime_status_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    status_layout->addWidget(runtime_status_);
    bottom->addWidget(status);
    bottom->setSizes({300, 430, 300});
    main->addWidget(bottom, 0);
    setCentralWidget(center);
}

void MainWindow::build_right_dock() {
    auto* dock = new QDockWidget(QStringLiteral("CONSOLE / DEBUGGER"), this);
    dock->setObjectName(QStringLiteral("debugDock"));
    dock->setMinimumWidth(410);
    auto* split = new QSplitter(Qt::Vertical, dock);
    split->setChildrenCollapsible(false);

    auto* console_group = new QGroupBox(QStringLiteral("CONSOLE / LOG"), split);
    auto* console_layout = new QVBoxLayout(console_group);
    console_ = new QPlainTextEdit(console_group);
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(4000);
    console_layout->addWidget(console_);
    split->addWidget(console_group);

    auto* debug_group = new QGroupBox(QStringLiteral("STEP DEBUGGER"), split);
    auto* debug_layout = new QVBoxLayout(debug_group);
    auto* note = new QLabel(QStringLiteral("Instruction stepping is intentionally offline-only while realtime audio is running."), debug_group);
    note->setWordWrap(true); debug_layout->addWidget(note);
    auto* controls = new QHBoxLayout;
    controls->addWidget(new QPushButton(QStringLiteral("Step Instruction"), debug_group));
    controls->addWidget(new QPushButton(QStringLiteral("Step Sample"), debug_group));
    controls->addWidget(new QPushButton(QStringLiteral("Continue"), debug_group));
    debug_layout->addLayout(controls);
    debugger_table_ = new QTableWidget(8, 2, debug_group);
    debugger_table_->setHorizontalHeaderLabels({QStringLiteral("State"), QStringLiteral("Value")});
    debugger_table_->horizontalHeader()->setStretchLastSection(true);
    const QStringList names{QStringLiteral("PC"), QStringLiteral("Instruction"), QStringLiteral("ACC"), QStringLiteral("PACC"), QStringLiteral("LR"), QStringLiteral("ADDR_PTR"), QStringLiteral("SIN0"), QStringLiteral("RMP0")};
    const QStringList values{QStringLiteral("000"), QStringLiteral("—"), QStringLiteral("0x000000"), QStringLiteral("0x000000"), QStringLiteral("0x000000"), QStringLiteral("00000"), QStringLiteral("0"), QStringLiteral("0")};
    for (qsizetype i=0;i<names.size();++i) { debugger_table_->setItem(static_cast<int>(i),0,new QTableWidgetItem(names[i])); debugger_table_->setItem(static_cast<int>(i),1,new QTableWidgetItem(values[i])); }
    debug_layout->addWidget(debugger_table_);
    split->addWidget(debug_group);

    split->addWidget(titled_text_panel(QStringLiteral("MEMORY / REGISTER INSPECTOR"),
        QStringLiteral("REG0  0x000000\nREG1  0x000000\nREG2  0x000000\n...\n\nDelay read/write activity will appear here."), split));
    split->setSizes({420, 350, 180});
    dock->setWidget(split);
    addDockWidget(Qt::RightDockWidgetArea, dock);
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

void MainWindow::choose_program() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open FV-1 Program"), {},
        QStringLiteral("FV-1 Programs (*.spn *.bin);;All files (*)"));
    if (path.isEmpty()) return;
    program_path_ = path;
    program_label_->setText(path);
    log(QStringLiteral("Program selected: ") + path);
    inspect_program();
}

void MainWindow::choose_audio_file() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Test Audio"), {},
        QStringLiteral("Wave audio (*.wav);;All files (*)"));
    if (path.isEmpty()) return;
    audio_file_path_ = path;
    file_label_->setText(path);
    source_combo_->setCurrentText(QStringLiteral("Audio File Loop"));
    log(QStringLiteral("Loop source selected: ") + path);
}

void MainWindow::inspect_program() {
    if (program_path_.isEmpty()) return;
    QByteArray bytes; QString error;
    if (!load_program_image(program_path_, bytes, error)) { log(QStringLiteral("Program inspection failed: ") + error); return; }
    fv1_config cfg{32768.0, FV1_DELAY_REFERENCE_16};
    fv1_engine* engine = fv1_create(&cfg);
    if (!engine) { log(QStringLiteral("Program inspection failed: cannot create FV-1 engine.")); return; }
    const auto result = fv1_load_bytes(engine, reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size()));
    fv1_resource_report report{};
    if (result == FV1_OK && fv1_analyze_program(engine, &report) == FV1_OK) {
        program_usage_->setValue(static_cast<int>(report.used_instructions));
        program_usage_->setFormat(QStringLiteral("%1 / 128 instructions").arg(report.used_instructions));
        delay_usage_->setValue(static_cast<int>(std::min<std::uint32_t>(report.highest_static_delay_address, FV1_DELAY_WORDS)));
        delay_usage_->setFormat(QStringLiteral("%1 / 32768 highest static address").arg(report.highest_static_delay_address));
        log(QStringLiteral("Resource analysis: %1 instructions, delay <= %2, %3 general registers, %4 POTs.")
            .arg(report.used_instructions).arg(report.highest_static_delay_address).arg(report.general_registers_used).arg(report.pots_used));
    }
    fv1_destroy(engine);
}

void MainWindow::start_session() {
    if (session_->running()) stop_session();
    if (program_path_.isEmpty()) { log(QStringLiteral("Start refused: open an FV-1 program first.")); statusBar()->showMessage(QStringLiteral("Open an FV-1 program first"), 5000); return; }
    QByteArray bytes; QString error;
    if (!load_program_image(program_path_, bytes, error)) { log(QStringLiteral("Start failed: ") + error); return; }

    const auto host_rate = static_cast<std::uint32_t>(host_rate_combo_->currentText().toUInt());
    const auto buffer = static_cast<std::uint32_t>(buffer_combo_->currentText().toUInt());
    const double clock = clock_combo_->currentText().toDouble();
    const int playback = playback_combo_->currentData().toInt();
    const int capture = capture_combo_->currentData().toInt();
    if (!session_->start(bytes, source_combo_->currentText(), audio_file_path_, generator_combo_->currentText(),
                         generator_frequency_->value(), host_rate, buffer, clock, playback, capture,
                         static_cast<float>(pot0_->value())/1000.0f,
                         static_cast<float>(pot1_->value())/1000.0f,
                         static_cast<float>(pot2_->value())/1000.0f, error)) {
        log(QStringLiteral("Session start failed: ") + error);
        statusBar()->showMessage(QStringLiteral("Session start failed"), 5000);
        return;
    }
    telemetry_timer_->start();
    log(QStringLiteral("Realtime session started: %1, host %2 Hz, virtual FV-1 %3 Hz, buffer %4.")
        .arg(source_combo_->currentText()).arg(host_rate).arg(clock,0,'f',1).arg(buffer));
    statusBar()->showMessage(QStringLiteral("RUNNING"));
}

void MainWindow::stop_session() {
    if (!session_) return;
    const bool was_running = session_->running();
    if (telemetry_timer_) telemetry_timer_->stop();
    session_->stop();
    if (runtime_status_) runtime_status_->setText(QStringLiteral("Stopped\nHost/FV-1 runtime ready."));
    if (was_running) log(QStringLiteral("Realtime session stopped."));
    if (statusBar()) statusBar()->showMessage(QStringLiteral("Stopped"));
}

void MainWindow::update_telemetry() {
    if (!session_ || !session_->running()) return;
    const auto a = session_->analysis();
    scope_plot_->set_snapshot(a);
    spectrum_plot_->set_snapshot(a);
    spectrogram_plot_->set_snapshot(a);
    levels_plot_->set_snapshot(a);
    const auto hs = session_->host_stats();
    const auto rs = session_->runtime_stats();
    runtime_status_->setText(QStringLiteral(
        "RUNNING\nHost frames      %1\nFV-1 frames      %2\nCallback CPU     %3%\nUnderruns        %4\nAnalyzer drops   %5\nRMS L/R          %6 / %7\nDominant         %8 Hz\nSRC              %9")
        .arg(rs.host_output_frames)
        .arg(rs.fv1_frames)
        .arg(hs.callback_cpu_load * 100.0, 0, 'f', 2)
        .arg(rs.output_underrun_frames)
        .arg(session_->analyzer_drops())
        .arg(a.rms_left, 0, 'f', 3)
        .arg(a.rms_right, 0, 'f', 3)
        .arg(a.dominant_frequency_hz, 0, 'f', 1)
        .arg(session_->using_speex() ? QStringLiteral("SpeexDSP") : QStringLiteral("linear fallback")));
}

void MainWindow::log(const QString& text) {
    if (console_) console_->appendPlainText(text);
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

} // namespace fv1::gui
