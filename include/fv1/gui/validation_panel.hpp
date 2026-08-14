#pragma once

#include <fv1/validation.hpp>

#include <QWidget>

#include <functional>
#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace fv1::gui {

// Offline Phase-5 validation workspace. This is intentionally a testbench
// instrument: it compares emulator/reference audio with captured hardware
// audio, but does not introduce project/editor/IDE concepts.
class ValidationPanel final : public QWidget {
public:
    explicit ValidationPanel(QWidget* parent = nullptr);

    void set_log_callback(std::function<void(const QString&)> callback);
    void set_reference_path(const QString& path);
    void set_capture_path(const QString& path);

private:
    void choose_reference();
    void choose_capture();
    void analyze();
    void export_report();
    void generate_stimulus();
    void refresh_result();

    std::function<void(const QString&)> log_callback_;
    std::optional<fv1::ValidationResult> result_;

    QLineEdit* reference_path_{};
    QLineEdit* capture_path_{};
    QDoubleSpinBox* max_lag_ms_{};
    QCheckBox* gain_match_{};
    QComboBox* fft_size_{};
    QDoubleSpinBox* min_correlation_{};
    QDoubleSpinBox* max_residual_rms_{};
    QDoubleSpinBox* max_residual_peak_{};
    QLabel* verdict_{};
    QTableWidget* metrics_{};
    QTableWidget* frequency_{};
    QPlainTextEdit* report_preview_{};
    QPushButton* export_button_{};
};

} // namespace fv1::gui
