#include <fv1/gui/validation_panel.hpp>
#include <fv1/gui/path_utils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>

namespace fv1::gui {
namespace {

QString n(double value, int decimals = 4) {
    if (!std::isfinite(value)) return QStringLiteral("—");
    return QString::number(value, 'f', decimals);
}

QTableWidgetItem* item(const QString& text) {
    auto* i = new QTableWidgetItem(text);
    i->setFlags(i->flags() & ~Qt::ItemIsEditable);
    return i;
}

} // namespace

ValidationPanel::ValidationPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);

    auto* heading = new QLabel(QStringLiteral(
        "PHASE 5B HARDWARE VALIDATION — compare the emulator/reference response with a captured physical FV-1 response. "
        "Recordings are automatically time-aligned before residual, gain, correlation and spectral error measurements."), this);
    heading->setWordWrap(true);
    outer->addWidget(heading);

    auto* files = new QGroupBox(QStringLiteral("REFERENCE / CAPTURE"), this);
    auto* files_layout = new QFormLayout(files);
    auto make_path_row = [this](QLineEdit*& edit, const QString& button_text, auto callback) {
        auto* row = new QWidget(this);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(row);
        auto* button = new QPushButton(button_text, row);
        connect(button, &QPushButton::clicked, this, callback);
        layout->addWidget(edit, 1);
        layout->addWidget(button);
        return row;
    };
    files_layout->addRow(QStringLiteral("Virtual / reference WAV"),
                         make_path_row(reference_path_, QStringLiteral("Browse…"), [this]{ choose_reference(); }));
    files_layout->addRow(QStringLiteral("Hardware / capture WAV"),
                         make_path_row(capture_path_, QStringLiteral("Browse…"), [this]{ choose_capture(); }));
    outer->addWidget(files);

    auto* settings = new QGroupBox(QStringLiteral("ALIGNMENT / ACCEPTANCE"), this);
    auto* settings_form = new QFormLayout(settings);
    max_lag_ms_ = new QDoubleSpinBox(settings);
    max_lag_ms_->setRange(0.0, 2000.0);
    max_lag_ms_->setDecimals(2);
    max_lag_ms_->setValue(100.0);
    max_lag_ms_->setSuffix(QStringLiteral(" ms"));
    gain_match_ = new QCheckBox(QStringLiteral("Gain-match capture for residual/SNR (raw gain error is still reported)"), settings);
    fft_size_ = new QComboBox(settings);
    for (int size : {2048, 4096, 8192, 16384, 32768, 65536}) fft_size_->addItem(QString::number(size), size);
    fft_size_->setCurrentText(QStringLiteral("16384"));
    min_correlation_ = new QDoubleSpinBox(settings);
    min_correlation_->setRange(-1.0, 1.0);
    min_correlation_->setDecimals(6);
    min_correlation_->setSingleStep(0.0001);
    min_correlation_->setValue(0.995);
    max_residual_rms_ = new QDoubleSpinBox(settings);
    max_residual_rms_->setRange(-200.0, 0.0);
    max_residual_rms_->setDecimals(1);
    max_residual_rms_->setValue(-45.0);
    max_residual_rms_->setSuffix(QStringLiteral(" dBFS"));
    max_residual_peak_ = new QDoubleSpinBox(settings);
    max_residual_peak_->setRange(-200.0, 0.0);
    max_residual_peak_->setDecimals(1);
    max_residual_peak_->setValue(-24.0);
    max_residual_peak_->setSuffix(QStringLiteral(" dBFS"));
    settings_form->addRow(QStringLiteral("Maximum alignment search"), max_lag_ms_);
    settings_form->addRow(QStringLiteral("Residual normalization"), gain_match_);
    settings_form->addRow(QStringLiteral("Spectral FFT"), fft_size_);
    settings_form->addRow(QStringLiteral("Minimum correlation"), min_correlation_);
    settings_form->addRow(QStringLiteral("Maximum residual RMS"), max_residual_rms_);
    settings_form->addRow(QStringLiteral("Maximum residual peak"), max_residual_peak_);
    outer->addWidget(settings);

    auto* buttons = new QHBoxLayout;
    auto* stimulus = new QPushButton(QStringLiteral("Generate Validation Stimulus…"), this);
    auto* hardware_pack = new QPushButton(QStringLiteral("Generate Hardware Test Pack…"), this);
    auto* analyze_button = new QPushButton(QStringLiteral("Analyze Reference vs Capture"), this);
    export_button_ = new QPushButton(QStringLiteral("Export Validation Report…"), this);
    export_button_->setEnabled(false);
    connect(stimulus, &QPushButton::clicked, this, [this]{ generate_stimulus(); });
    connect(hardware_pack, &QPushButton::clicked, this, [this]{ generate_hardware_pack(); });
    connect(analyze_button, &QPushButton::clicked, this, [this]{ analyze(); });
    connect(export_button_, &QPushButton::clicked, this, [this]{ export_report(); });
    buttons->addWidget(stimulus);
    buttons->addWidget(hardware_pack);
    buttons->addStretch(1);
    buttons->addWidget(analyze_button);
    buttons->addWidget(export_button_);
    outer->addLayout(buttons);

    verdict_ = new QLabel(QStringLiteral("READY — select a reference and capture"), this);
    verdict_->setAlignment(Qt::AlignCenter);
    verdict_->setMinimumHeight(34);
    outer->addWidget(verdict_);

    auto* results = new QGroupBox(QStringLiteral("MEASUREMENT SUMMARY"), this);
    auto* results_layout = new QVBoxLayout(results);
    metrics_ = new QTableWidget(8, 3, results);
    metrics_->setHorizontalHeaderLabels({QStringLiteral("Metric"), QStringLiteral("Left"), QStringLiteral("Right")});
    metrics_->verticalHeader()->setVisible(false);
    metrics_->horizontalHeader()->setStretchLastSection(true);
    const QStringList names{
        QStringLiteral("Reference RMS (dBFS)"), QStringLiteral("Capture RMS (dBFS)"),
        QStringLiteral("Raw gain error (dB)"), QStringLiteral("Correlation"),
        QStringLiteral("Residual RMS (dBFS)"), QStringLiteral("Residual peak (dBFS)"),
        QStringLiteral("SNR (dB)"), QStringLiteral("Capture delay (frames / ms)")};
    for (int row = 0; row < static_cast<int>(names.size()); ++row) {
        metrics_->setItem(row, 0, item(names[row]));
        metrics_->setItem(row, 1, item(QStringLiteral("—")));
        metrics_->setItem(row, 2, item(QStringLiteral("—")));
    }
    results_layout->addWidget(metrics_);
    outer->addWidget(results);

    auto* spectral = new QGroupBox(QStringLiteral("SPECTRAL ERROR (REFERENCE-ACTIVE BINS)"), this);
    auto* spectral_layout = new QVBoxLayout(spectral);
    frequency_ = new QTableWidget(0, 4, spectral);
    frequency_->setHorizontalHeaderLabels({QStringLiteral("Frequency"), QStringLiteral("Magnitude error"),
                                           QStringLiteral("Phase error"), QStringLiteral("Reference level")});
    frequency_->verticalHeader()->setVisible(false);
    frequency_->horizontalHeader()->setStretchLastSection(true);
    spectral_layout->addWidget(frequency_);
    outer->addWidget(spectral, 1);

    report_preview_ = new QPlainTextEdit(this);
    report_preview_->setReadOnly(true);
    report_preview_->setMaximumHeight(130);
    report_preview_->setPlaceholderText(QStringLiteral("Pass/fail details and validation notes appear here."));
    outer->addWidget(report_preview_);
}

void ValidationPanel::set_log_callback(std::function<void(const QString&)> callback) {
    log_callback_ = std::move(callback);
}

void ValidationPanel::set_reference_path(const QString& path) { reference_path_->setText(path); }
void ValidationPanel::set_capture_path(const QString& path) { capture_path_->setText(path); }

void ValidationPanel::choose_reference() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Virtual / Reference WAV"), {},
                                                      QStringLiteral("WAV audio (*.wav);;All files (*)"));
    if (!path.isEmpty()) reference_path_->setText(path);
}

void ValidationPanel::choose_capture() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Hardware / Capture WAV"), {},
                                                      QStringLiteral("WAV audio (*.wav);;All files (*)"));
    if (!path.isEmpty()) capture_path_->setText(path);
}

void ValidationPanel::analyze() {
    const QString ref_path = reference_path_->text().trimmed();
    const QString cap_path = capture_path_->text().trimmed();
    if (ref_path.isEmpty() || cap_path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("FV-1 Validation"),
                                 QStringLiteral("Choose both a virtual/reference WAV and a hardware/capture WAV."));
        return;
    }
    fv1::ValidationAudio reference, capture;
    std::string error;
    if (!fv1::load_validation_wav(path_from_qstring(ref_path), reference, &error) ||
        !fv1::load_validation_wav(path_from_qstring(cap_path), capture, &error)) {
        QMessageBox::warning(this, QStringLiteral("FV-1 Validation"), QString::fromStdString(error));
        return;
    }
    fv1::ValidationConfig cfg;
    cfg.max_alignment_ms = max_lag_ms_->value();
    cfg.gain_match_residual = gain_match_->isChecked();
    cfg.fft_size = static_cast<std::size_t>(fft_size_->currentData().toULongLong());
    cfg.minimum_correlation = min_correlation_->value();
    cfg.maximum_residual_rms_dbfs = max_residual_rms_->value();
    cfg.maximum_residual_peak_dbfs = max_residual_peak_->value();
    result_ = fv1::validate_recordings(reference, capture, cfg);
    refresh_result();
    if (log_callback_) {
        log_callback_(QStringLiteral("Validation %1: delay %2 frames / %3 ms, correlation L/R %4 / %5, residual RMS L/R %6 / %7 dBFS")
            .arg(result_->passed ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
            .arg(static_cast<qlonglong>(result_->capture_delay_frames)).arg(result_->capture_delay_ms, 0, 'f', 4)
            .arg(result_->left.correlation, 0, 'f', 6).arg(result_->right.correlation, 0, 'f', 6)
            .arg(result_->left.residual_rms_dbfs, 0, 'f', 2).arg(result_->right.residual_rms_dbfs, 0, 'f', 2));
    }
}

void ValidationPanel::refresh_result() {
    if (!result_) return;
    const auto& r = *result_;
    verdict_->setText(r.passed ? QStringLiteral("PASS — capture is inside the configured limits")
                               : QStringLiteral("FAIL — one or more validation limits exceeded"));
    verdict_->setProperty("validationPass", r.passed);
    verdict_->style()->unpolish(verdict_);
    verdict_->style()->polish(verdict_);

    auto set = [this](int row, const QString& left, const QString& right) {
        metrics_->item(row, 1)->setText(left);
        metrics_->item(row, 2)->setText(right);
    };
    set(0, n(r.left.reference_rms_dbfs, 2), n(r.right.reference_rms_dbfs, 2));
    set(1, n(r.left.capture_rms_dbfs, 2), n(r.right.capture_rms_dbfs, 2));
    set(2, n(r.left.gain_error_db, 3), n(r.right.gain_error_db, 3));
    set(3, n(r.left.correlation, 6), n(r.right.correlation, 6));
    set(4, n(r.left.residual_rms_dbfs, 2), n(r.right.residual_rms_dbfs, 2));
    set(5, n(r.left.residual_peak_dbfs, 2), n(r.right.residual_peak_dbfs, 2));
    set(6, n(r.left.snr_db, 2), n(r.right.snr_db, 2));
    const QString delay = QStringLiteral("%1 / %2 ms").arg(static_cast<qlonglong>(r.capture_delay_frames)).arg(r.capture_delay_ms, 0, 'f', 4);
    set(7, delay, delay);

    frequency_->setRowCount(static_cast<int>(r.frequency_response.size()));
    for (int row = 0; row < frequency_->rowCount(); ++row) {
        const auto& p = r.frequency_response[static_cast<std::size_t>(row)];
        frequency_->setItem(row, 0, item(QStringLiteral("%1 Hz").arg(p.frequency_hz, 0, 'f', 2)));
        frequency_->setItem(row, 1, item(QStringLiteral("%1 dB").arg(p.magnitude_error_db, 0, 'f', 3)));
        frequency_->setItem(row, 2, item(QStringLiteral("%1°").arg(p.phase_error_degrees, 0, 'f', 2)));
        frequency_->setItem(row, 3, item(QStringLiteral("%1 dBFS").arg(p.reference_level_dbfs, 0, 'f', 2)));
    }

    QStringList preview;
    preview << QStringLiteral("Sample rate: %1 Hz; compared: %2 frames; gain correction used for residual: %3 dB")
                   .arg(r.sample_rate).arg(static_cast<qulonglong>(r.compared_frames)).arg(r.applied_capture_gain_db, 0, 'f', 3);
    preview << QStringLiteral("Spectral magnitude error RMS/worst: %1 / %2 dB; worst phase error: %3°")
                   .arg(r.spectral_rms_magnitude_error_db, 0, 'f', 3)
                   .arg(r.spectral_worst_magnitude_error_db, 0, 'f', 3)
                   .arg(r.spectral_worst_phase_error_degrees, 0, 'f', 2);
    if (r.failures.empty()) preview << QStringLiteral("All configured acceptance limits passed.");
    else {
        preview << QStringLiteral("Failed limits:");
        for (const auto& f : r.failures) preview << QStringLiteral("• ") + QString::fromStdString(f);
    }
    report_preview_->setPlainText(preview.join('\n'));
    export_button_->setEnabled(true);
}

void ValidationPanel::export_report() {
    if (!result_) return;
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export FV-1 Validation Report Bundle"),
                                                QStringLiteral("fv1-validation.md"),
                                                QStringLiteral("Markdown report (*.md)"));
    if (path.isEmpty()) return;
    if (path.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) path.chop(3);
    std::string error;
    if (!fv1::write_validation_report_bundle(path_from_qstring(path), *result_, &error)) {
        QMessageBox::warning(this, QStringLiteral("FV-1 Validation"), QString::fromStdString(error));
        return;
    }
    if (log_callback_) log_callback_(QStringLiteral("Validation report bundle exported: ") + path);
    QMessageBox::information(this, QStringLiteral("FV-1 Validation"),
        QStringLiteral("Exported JSON, Markdown, frequency CSV and residual WAV using prefix:\n%1").arg(path));
}

void ValidationPanel::generate_hardware_pack() {
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Create Phase 5B Hardware Validation Pack"));
    if (directory.isEmpty()) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Hardware Validation Pack"));
    auto* outer = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* rate = new QSpinBox(&dialog);
    rate->setRange(8000, 384000);
    rate->setValue(48000);
    rate->setSuffix(QStringLiteral(" Hz"));
    auto* seconds = new QDoubleSpinBox(&dialog);
    seconds->setRange(0.05, 120.0);
    seconds->setDecimals(2);
    seconds->setValue(5.0);
    seconds->setSuffix(QStringLiteral(" s"));
    auto* level = new QDoubleSpinBox(&dialog);
    level->setRange(0.01, 0.8);
    level->setDecimals(3);
    level->setValue(0.25);
    auto* seed = new QSpinBox(&dialog);
    seed->setRange(1, 0x7fffffff);
    seed->setValue(0x465631);
    form->addRow(QStringLiteral("Host/capture sample rate"), rate);
    form->addRow(QStringLiteral("Standard stimulus duration"), seconds);
    form->addRow(QStringLiteral("Nominal level"), level);
    form->addRow(QStringLiteral("Deterministic seed"), seed);
    outer->addLayout(form);
    auto* note = new QLabel(QStringLiteral(
        "Creates impulse, multitone, log sweep, 1 kHz sine, white-noise and pink-noise WAVs plus a manifest and fixture workflow README. Use the untouched WAVs for both virtual and physical runs."), &dialog);
    note->setWordWrap(true);
    outer->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Generate Pack"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;

    fv1::ValidationPackConfig cfg;
    cfg.sample_rate = static_cast<std::uint32_t>(rate->value());
    cfg.standard_seconds = seconds->value();
    cfg.level = level->value();
    cfg.seed = static_cast<std::uint32_t>(seed->value());
    const std::filesystem::path pack_dir = path_from_qstring(directory) / "fv1-hardware-validation-pack";
    std::string error;
    if (!fv1::write_validation_stimulus_pack(pack_dir, cfg, &error)) {
        QMessageBox::warning(this, QStringLiteral("Hardware Validation Pack"), QString::fromStdString(error));
        return;
    }
    const QString created = QString::fromStdString(pack_dir.string());
    QMessageBox::information(this, QStringLiteral("Hardware Validation Pack"),
        QStringLiteral("Created deterministic Phase 5B hardware-validation pack:\n%1").arg(created));
    if (log_callback_) log_callback_(QStringLiteral("Hardware validation stimulus pack generated: ") + created);
}

void ValidationPanel::generate_stimulus() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Generate Deterministic Validation Stimulus"));
    auto* outer = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* kind = new QComboBox(&dialog);
    kind->addItems({QStringLiteral("multitone"), QStringLiteral("sweep"), QStringLiteral("sine"),
                    QStringLiteral("white"), QStringLiteral("pink"), QStringLiteral("impulse")});
    auto* rate = new QComboBox(&dialog);
    for (int value : {32768, 44100, 46608, 48000, 88200, 96000, 192000}) rate->addItem(QString::number(value), value);
    rate->setCurrentText(QStringLiteral("48000"));
    auto* seconds = new QDoubleSpinBox(&dialog);
    seconds->setRange(0.1, 600.0); seconds->setValue(5.0); seconds->setSuffix(QStringLiteral(" s"));
    auto* level = new QDoubleSpinBox(&dialog);
    level->setRange(0.001, 0.95); level->setDecimals(3); level->setValue(0.25);
    auto* frequency = new QDoubleSpinBox(&dialog);
    frequency->setRange(1.0, 40000.0); frequency->setValue(440.0); frequency->setSuffix(QStringLiteral(" Hz"));
    auto* sweep_end = new QDoubleSpinBox(&dialog);
    sweep_end->setRange(10.0, 80000.0); sweep_end->setValue(16000.0); sweep_end->setSuffix(QStringLiteral(" Hz"));
    auto* seed = new QSpinBox(&dialog);
    seed->setRange(1, 2147483647); seed->setValue(0x465631);
    form->addRow(QStringLiteral("Kind"), kind);
    form->addRow(QStringLiteral("Sample rate"), rate);
    form->addRow(QStringLiteral("Duration"), seconds);
    form->addRow(QStringLiteral("Level"), level);
    form->addRow(QStringLiteral("Sine / sweep start"), frequency);
    form->addRow(QStringLiteral("Sweep end"), sweep_end);
    form->addRow(QStringLiteral("Noise seed"), seed);
    outer->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;

    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Validation Stimulus"),
        QStringLiteral("fv1-validation-%1.wav").arg(kind->currentText()), QStringLiteral("WAV audio (*.wav)"));
    if (path.isEmpty()) return;
    fv1::ValidationAudio audio;
    std::string error;
    if (!fv1::generate_validation_stimulus(audio, static_cast<std::uint32_t>(rate->currentData().toUInt()),
            seconds->value(), kind->currentText().toStdString(), level->value(), frequency->value(),
            sweep_end->value(), static_cast<std::uint32_t>(seed->value()), &error) ||
        !fv1::write_validation_wav(path_from_qstring(path), audio, &error)) {
        QMessageBox::warning(this, QStringLiteral("FV-1 Validation"), QString::fromStdString(error));
        return;
    }
    if (log_callback_) log_callback_(QStringLiteral("Generated deterministic validation stimulus: ") + path);
}

} // namespace fv1::gui
