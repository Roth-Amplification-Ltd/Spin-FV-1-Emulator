#pragma once

#include <fv1/analysis.hpp>

#include <QRectF>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <vector>

namespace fv1::gui {

enum class PlotKind { Oscilloscope, Spectrum, Spectrogram, Levels };
enum class TriggerMode { Off, Auto, Normal, Single };
enum class TriggerChannel { Left, Right };
enum class TriggerSlope { Rising, Falling };

class InstrumentPlot final : public QWidget {
public:
    explicit InstrumentPlot(PlotKind kind, QWidget* parent = nullptr);
    QSize minimumSizeHint() const override;

    void set_snapshot(const fv1::AnalysisSnapshot& snapshot);
    void set_secondary_snapshot(const fv1::AnalysisSnapshot& snapshot);
    void set_signal_label(const QString& label);
    void clear_display();

    PlotKind kind() const noexcept { return kind_; }

    void set_frozen(bool frozen);
    bool frozen() const noexcept { return frozen_; }

    // Oscilloscope controls. time_zoom >= 1 shows a progressively shorter
    // section of the current analyzer capture; vertical_gain magnifies traces.
    void set_time_zoom(double time_zoom);
    double time_zoom() const noexcept { return time_zoom_; }
    void set_vertical_gain(double gain);
    double vertical_gain() const noexcept { return vertical_gain_; }

    // Oscilloscope trigger. Normal holds the previous capture until a crossing
    // arrives; Single captures one crossing and then holds until re-armed.
    void set_trigger_mode(TriggerMode mode);
    TriggerMode trigger_mode() const noexcept { return trigger_mode_; }
    void set_trigger_channel(TriggerChannel channel);
    TriggerChannel trigger_channel() const noexcept { return trigger_channel_; }
    void set_trigger_slope(TriggerSlope slope);
    TriggerSlope trigger_slope() const noexcept { return trigger_slope_; }
    void set_trigger_level(double level);
    double trigger_level() const noexcept { return trigger_level_; }
    void rearm_single_trigger();

    // Spectrum/spectrogram display controls. These affect visualization only;
    // FFT size is selected by the analyzer when a session begins.
    void set_log_frequency(bool enabled);
    bool log_frequency() const noexcept { return log_frequency_; }
    void set_db_range(double minimum_db, double maximum_db);
    double minimum_db() const noexcept { return minimum_db_; }
    double maximum_db() const noexcept { return maximum_db_; }
    void set_peak_hold(bool enabled);
    bool peak_hold() const noexcept { return peak_hold_enabled_; }
    void set_spectrogram_history_columns(std::size_t columns);
    std::size_t spectrogram_history_columns() const noexcept { return spectrogram_history_columns_; }

    // Raw-vs-processed comparison is intentionally a monitoring feature, not
    // an editor/IDE concept. The secondary trace is normally the pre-DSP tap.
    void set_compare_enabled(bool enabled);
    bool compare_enabled() const noexcept { return compare_enabled_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double spectrum_x_for_bin(std::size_t bin, std::size_t bins, const QRectF& rect) const;
    std::size_t find_trigger_index(const fv1::AnalysisSnapshot& snapshot) const noexcept;

    PlotKind kind_;
    fv1::AnalysisSnapshot snapshot_;
    fv1::AnalysisSnapshot secondary_snapshot_;
    std::vector<std::vector<float>> spectrogram_history_;
    std::vector<float> peak_hold_db_;
    QString signal_label_{QStringLiteral("PROCESSED")};

    bool frozen_{};
    bool compare_enabled_{};
    bool log_frequency_{true};
    bool peak_hold_enabled_{};
    double time_zoom_{1.0};
    double vertical_gain_{1.0};
    TriggerMode trigger_mode_{TriggerMode::Off};
    TriggerChannel trigger_channel_{TriggerChannel::Left};
    TriggerSlope trigger_slope_{TriggerSlope::Rising};
    double trigger_level_{};
    std::size_t trigger_index_{static_cast<std::size_t>(-1)};
    bool single_trigger_armed_{true};
    double minimum_db_{-100.0};
    double maximum_db_{0.0};
    std::size_t spectrogram_history_columns_{160};
};

} // namespace fv1::gui
