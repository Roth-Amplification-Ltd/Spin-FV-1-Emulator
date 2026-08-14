#include <fv1/gui/instrument_plot.hpp>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fv1::gui {
namespace {

constexpr double kMinLogHz = 20.0;

QColor property_color(const char* name, const QColor& fallback) {
    const QVariant value = qApp->property(name);
    return value.isValid() ? value.value<QColor>() : fallback;
}

} // namespace

InstrumentPlot::InstrumentPlot(PlotKind kind, QWidget* parent) : QWidget(parent), kind_(kind) {
    setAutoFillBackground(false);
}

QSize InstrumentPlot::minimumSizeHint() const { return {480, 260}; }

void InstrumentPlot::set_signal_label(const QString& label) {
    signal_label_ = label;
    update();
}

void InstrumentPlot::set_frozen(bool frozen) {
    frozen_ = frozen;
    update();
}

void InstrumentPlot::set_time_zoom(double zoom) {
    time_zoom_ = std::clamp(zoom, 1.0, 16.0);
    update();
}

void InstrumentPlot::set_vertical_gain(double gain) {
    vertical_gain_ = std::clamp(gain, 0.125, 16.0);
    update();
}

void InstrumentPlot::set_trigger_mode(TriggerMode mode) {
    trigger_mode_ = mode;
    if (mode == TriggerMode::Single) single_trigger_armed_ = true;
    update();
}

void InstrumentPlot::set_trigger_channel(TriggerChannel channel) {
    trigger_channel_ = channel;
    update();
}

void InstrumentPlot::set_trigger_slope(TriggerSlope slope) {
    trigger_slope_ = slope;
    update();
}

void InstrumentPlot::set_trigger_level(double level) {
    trigger_level_ = std::clamp(level, -1.0, 1.0);
    update();
}

void InstrumentPlot::rearm_single_trigger() {
    single_trigger_armed_ = true;
    frozen_ = false;
    update();
}

void InstrumentPlot::set_log_frequency(bool enabled) {
    log_frequency_ = enabled;
    update();
}

void InstrumentPlot::set_db_range(double minimum_db, double maximum_db) {
    if (!std::isfinite(minimum_db) || !std::isfinite(maximum_db) || minimum_db >= maximum_db)
        return;
    minimum_db_ = minimum_db;
    maximum_db_ = maximum_db;
    update();
}

void InstrumentPlot::set_peak_hold(bool enabled) {
    peak_hold_enabled_ = enabled;
    if (!enabled) peak_hold_db_.clear();
    update();
}

void InstrumentPlot::set_spectrogram_history_columns(std::size_t columns) {
    spectrogram_history_columns_ = std::clamp<std::size_t>(columns, 32, 1024);
    if (spectrogram_history_.size() > spectrogram_history_columns_) {
        spectrogram_history_.erase(
            spectrogram_history_.begin(),
            spectrogram_history_.begin() + static_cast<std::ptrdiff_t>(spectrogram_history_.size() - spectrogram_history_columns_));
    }
    update();
}

void InstrumentPlot::set_compare_enabled(bool enabled) {
    compare_enabled_ = enabled;
    update();
}

void InstrumentPlot::clear_display() {
    snapshot_ = {};
    secondary_snapshot_ = {};
    spectrogram_history_.clear();
    peak_hold_db_.clear();
    trigger_index_ = static_cast<std::size_t>(-1);
    if (trigger_mode_ == TriggerMode::Single) single_trigger_armed_ = true;
    update();
}

std::size_t InstrumentPlot::find_trigger_index(const fv1::AnalysisSnapshot& snapshot) const noexcept {
    if (snapshot.scope_frames.size() < 2) return static_cast<std::size_t>(-1);
    const auto sample = [this](const fv1::StereoFrame& frame) {
        return trigger_channel_ == TriggerChannel::Left ? static_cast<double>(frame.left)
                                                        : static_cast<double>(frame.right);
    };
    for (std::size_t i = 1; i < snapshot.scope_frames.size(); ++i) {
        const double a = sample(snapshot.scope_frames[i - 1]);
        const double b = sample(snapshot.scope_frames[i]);
        if (trigger_slope_ == TriggerSlope::Rising) {
            if (a < trigger_level_ && b >= trigger_level_) return i;
        } else {
            if (a > trigger_level_ && b <= trigger_level_) return i;
        }
    }
    return static_cast<std::size_t>(-1);
}

void InstrumentPlot::set_snapshot(const fv1::AnalysisSnapshot& snapshot) {
    if (frozen_) return;

    if (kind_ == PlotKind::Oscilloscope && trigger_mode_ != TriggerMode::Off) {
        const std::size_t candidate = find_trigger_index(snapshot);
        const bool found = candidate != static_cast<std::size_t>(-1);
        if (trigger_mode_ == TriggerMode::Normal && !found) return;
        if (trigger_mode_ == TriggerMode::Single) {
            if (!single_trigger_armed_ || !found) return;
            single_trigger_armed_ = false;
        }
        trigger_index_ = candidate;
    } else if (kind_ == PlotKind::Oscilloscope) {
        trigger_index_ = static_cast<std::size_t>(-1);
    }

    snapshot_ = snapshot;

    if (peak_hold_enabled_ && !snapshot.spectrum_db.empty()) {
        if (peak_hold_db_.size() != snapshot.spectrum_db.size())
            peak_hold_db_.assign(snapshot.spectrum_db.size(), -200.0f);
        for (std::size_t i = 0; i < snapshot.spectrum_db.size(); ++i)
            peak_hold_db_[i] = std::max(peak_hold_db_[i], snapshot.spectrum_db[i]);
    }

    if (kind_ == PlotKind::Spectrogram && !snapshot.spectrum_db.empty()) {
        spectrogram_history_.push_back(snapshot.spectrum_db);
        if (spectrogram_history_.size() > spectrogram_history_columns_) {
            spectrogram_history_.erase(
                spectrogram_history_.begin(),
                spectrogram_history_.begin() + static_cast<std::ptrdiff_t>(spectrogram_history_.size() - spectrogram_history_columns_));
        }
    }
    update();
}

void InstrumentPlot::set_secondary_snapshot(const fv1::AnalysisSnapshot& snapshot) {
    if (frozen_) return;
    secondary_snapshot_ = snapshot;
    update();
}

double InstrumentPlot::spectrum_x_for_bin(std::size_t bin, std::size_t bins, const QRectF& r) const {
    if (bins <= 1) return r.left();
    const double nyquist = snapshot_.sample_rate > 0.0 ? snapshot_.sample_rate * 0.5 : 24000.0;
    const double frequency = static_cast<double>(bin) * nyquist / static_cast<double>(bins - 1);
    if (!log_frequency_) {
        return r.left() + (static_cast<double>(bin) / static_cast<double>(bins - 1)) * r.width();
    }
    if (frequency <= kMinLogHz || nyquist <= kMinLogHz) return r.left();
    const double normalized = std::log(frequency / kMinLogHz) / std::log(nyquist / kMinLogHz);
    return r.left() + std::clamp(normalized, 0.0, 1.0) * r.width();
}

void InstrumentPlot::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect().adjusted(10, 10, -10, -10);
    const QColor bg = palette().color(QPalette::Base);
    const QColor fg = palette().color(QPalette::Text);
    const QColor accent = property_color("fv1ThemeAccent", palette().color(QPalette::Highlight));
    const QColor raw_trace = property_color("fv1ThemeRawTrace", palette().color(QPalette::Mid));
    const QColor gridMajor = property_color("fv1ThemeGridMajor", palette().color(QPalette::Mid));
    const QColor gridMinor = property_color("fv1ThemeGridMinor", palette().color(QPalette::Dark));
    const QColor muted = property_color("fv1ThemeMuted", palette().color(QPalette::Mid));
    p.fillRect(rect(), bg);

    p.setPen(QPen(gridMinor, 1));
    for (int i = 1; i < 20; ++i) {
        const double x = r.left() + r.width() * static_cast<double>(i) / 20.0;
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
    }
    for (int i = 1; i < 10; ++i) {
        const double y = r.top() + r.height() * static_cast<double>(i) / 10.0;
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }
    p.setPen(QPen(gridMajor, 1));
    p.drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));
    p.drawLine(QPointF(r.left(), r.top()), QPointF(r.left(), r.bottom()));

    if (kind_ == PlotKind::Oscilloscope && !snapshot_.scope_frames.empty()) {
        const auto draw_scope = [&](const fv1::AnalysisSnapshot& snap, const QColor& left_color,
                                    const QColor& right_color, double width) {
            if (snap.scope_frames.empty()) return;
            QPainterPath left, right;
            const std::size_t n = snap.scope_frames.size();
            const std::size_t visible = std::max<std::size_t>(2, std::min(n, static_cast<std::size_t>(
                std::llround(static_cast<double>(n) / time_zoom_))));
            std::size_t begin = n > visible ? n - visible : 0;
            if (trigger_index_ != static_cast<std::size_t>(-1) && trigger_index_ < n) {
                const std::size_t pretrigger = visible / 4;
                begin = trigger_index_ > pretrigger ? trigger_index_ - pretrigger : 0;
                if (begin + visible > n) begin = n - visible;
            }
            const std::size_t count = std::min(visible, n - begin);
            for (std::size_t i = 0; i < count; ++i) {
                const double t = count > 1 ? static_cast<double>(i) / static_cast<double>(count - 1) : 0.0;
                const double x = r.left() + t * r.width();
                const auto& frame = snap.scope_frames[begin + i];
                const double l = std::clamp(static_cast<double>(frame.left) * vertical_gain_, -1.0, 1.0);
                const double rr = std::clamp(static_cast<double>(frame.right) * vertical_gain_, -1.0, 1.0);
                const QPointF pl(x, r.center().y() - l * r.height() * 0.45);
                const QPointF pr(x, r.center().y() - rr * r.height() * 0.45);
                if (i == 0) { left.moveTo(pl); right.moveTo(pr); }
                else { left.lineTo(pl); right.lineTo(pr); }
            }
            p.setPen(QPen(left_color, width)); p.drawPath(left);
            p.setPen(QPen(right_color, std::max(1.0, width - 0.4))); p.drawPath(right);
        };

        if (compare_enabled_ && !secondary_snapshot_.scope_frames.empty()) {
            QColor raw_r = raw_trace; raw_r.setAlpha(155);
            draw_scope(secondary_snapshot_, raw_trace, raw_r, 1.1);
        }
        draw_scope(snapshot_, accent, accent.lighter(145), 1.7);

        if (trigger_mode_ != TriggerMode::Off) {
            QColor trigger_color = property_color("fv1ThemeWarning", accent.lighter(160));
            trigger_color.setAlpha(180);
            const double y = r.center().y() - std::clamp(trigger_level_ * vertical_gain_, -1.0, 1.0) * r.height() * 0.45;
            p.setPen(QPen(trigger_color, 1.0, Qt::DashLine));
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
    } else if (kind_ == PlotKind::Spectrum && !snapshot_.spectrum_db.empty()) {
        const auto draw_spectrum = [&](const std::vector<float>& spectrum, const QColor& color, double width) {
            if (spectrum.empty()) return;
            QPainterPath path;
            const auto n = spectrum.size();
            for (std::size_t i = 0; i < n; ++i) {
                const double x = spectrum_x_for_bin(i, n, r);
                const double norm = std::clamp(
                    (static_cast<double>(spectrum[i]) - minimum_db_) / (maximum_db_ - minimum_db_), 0.0, 1.0);
                const QPointF pt(x, r.bottom() - norm * r.height());
                if (i == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            p.setPen(QPen(color, width));
            p.drawPath(path);
        };

        if (compare_enabled_ && !secondary_snapshot_.spectrum_db.empty())
            draw_spectrum(secondary_snapshot_.spectrum_db, raw_trace, 1.1);
        draw_spectrum(snapshot_.spectrum_db, accent, 1.7);
        if (peak_hold_enabled_ && !peak_hold_db_.empty()) {
            QColor peak = accent.lighter(165); peak.setAlpha(160);
            draw_spectrum(peak_hold_db_, peak, 1.0);
        }
    } else if (kind_ == PlotKind::Spectrogram && !spectrogram_history_.empty()) {
        const double column_w = r.width() / static_cast<double>(spectrogram_history_.size());
        for (std::size_t x = 0; x < spectrogram_history_.size(); ++x) {
            const auto& spectrum = spectrogram_history_[x];
            const std::size_t rows = std::min<std::size_t>(spectrum.size(), 256);
            for (std::size_t y = 0; y < rows; ++y) {
                const std::size_t bin = y * spectrum.size() / rows;
                const double energy = std::clamp(
                    (static_cast<double>(spectrum[bin]) - minimum_db_) / (maximum_db_ - minimum_db_), 0.0, 1.0);
                QColor c = accent;
                c.setAlphaF(static_cast<float>(0.03 + energy * 0.82));
                const double row_h = r.height() / static_cast<double>(rows);
                p.fillRect(QRectF(r.left() + static_cast<double>(x) * column_w,
                                  r.bottom() - static_cast<double>(y + 1) * row_h,
                                  column_w + 0.5, row_h + 0.5), c);
            }
        }
    } else if (kind_ == PlotKind::Levels) {
        const double center = r.center().x();
        const double w = r.width() * 0.13;
        const double levels[2]{std::clamp(static_cast<double>(snapshot_.peak_left), 0.0, 1.0),
                               std::clamp(static_cast<double>(snapshot_.peak_right), 0.0, 1.0)};
        for (int ch = 0; ch < 2; ++ch) {
            const double x = center + (ch == 0 ? -1.2 : 0.2) * w;
            QRectF meter(x, r.top() + r.height() * 0.08, w, r.height() * 0.84);
            p.setPen(QPen(gridMajor, 1)); p.drawRect(meter);
            QRectF fill = meter;
            fill.setTop(meter.bottom() - meter.height() * levels[ch]);
            p.fillRect(fill, accent);
        }
        p.setPen(muted);
        p.drawText(r.adjusted(8, 28, -8, -8), Qt::AlignTop | Qt::AlignLeft,
                   QStringLiteral("RMS %1 / %2    Corr %3")
                       .arg(snapshot_.rms_left, 0, 'f', 3)
                       .arg(snapshot_.rms_right, 0, 'f', 3)
                       .arg(snapshot_.correlation, 0, 'f', 3));
    }

    p.setPen(fg);
    QString title;
    if (kind_ == PlotKind::Oscilloscope) {
        title = QStringLiteral("L / R TIME DOMAIN    %1x TIME    %2x V")
                    .arg(time_zoom_, 0, 'g', 3).arg(vertical_gain_, 0, 'g', 3);
        if (trigger_mode_ != TriggerMode::Off) {
            QString mode = QStringLiteral("AUTO");
            if (trigger_mode_ == TriggerMode::Normal) mode = QStringLiteral("NORMAL");
            else if (trigger_mode_ == TriggerMode::Single) mode = single_trigger_armed_ ? QStringLiteral("SINGLE ARMED") : QStringLiteral("SINGLE HOLD");
            title += QStringLiteral("    TRIG %1 %2 %3 %4")
                .arg(mode)
                .arg(trigger_channel_ == TriggerChannel::Left ? QStringLiteral("L") : QStringLiteral("R"))
                .arg(trigger_slope_ == TriggerSlope::Rising ? QStringLiteral("↗") : QStringLiteral("↘"))
                .arg(trigger_level_, 0, 'f', 2);
        }
    } else if (kind_ == PlotKind::Spectrum) {
        title = QStringLiteral("FFT SPECTRUM    %1    %2..%3 dB")
                    .arg(log_frequency_ ? QStringLiteral("LOG Hz") : QStringLiteral("LINEAR Hz"))
                    .arg(minimum_db_, 0, 'f', 0).arg(maximum_db_, 0, 'f', 0);
        if (peak_hold_enabled_) title += QStringLiteral("    PEAK HOLD");
    } else if (kind_ == PlotKind::Spectrogram) {
        title = QStringLiteral("SPECTROGRAM HISTORY    %1 columns").arg(spectrogram_history_columns_);
    } else {
        title = QStringLiteral("OUTPUT LEVELS");
    }
    title += QStringLiteral("    [%1]").arg(signal_label_);
    if (compare_enabled_ && (kind_ == PlotKind::Oscilloscope || kind_ == PlotKind::Spectrum))
        title += QStringLiteral("    RAW + PROCESSED");
    if (frozen_) title += QStringLiteral("    ❄ FROZEN");
    if (snapshot_.sequence != 0 && kind_ == PlotKind::Spectrum && snapshot_.dominant_frequency_hz > 0.0f) {
        title += QStringLiteral("    dominant %1 Hz").arg(snapshot_.dominant_frequency_hz, 0, 'f', 1);
    }
    p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, title);
}

} // namespace fv1::gui
