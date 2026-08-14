#include <fv1/gui/instrument_plot.hpp>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

#include <algorithm>
#include <cmath>

namespace fv1::gui {

InstrumentPlot::InstrumentPlot(PlotKind kind, QWidget* parent) : QWidget(parent), kind_(kind) {
    setAutoFillBackground(false);
}

QSize InstrumentPlot::minimumSizeHint() const { return {480, 260}; }

void InstrumentPlot::set_signal_label(const QString& label) {
    signal_label_ = label;
    update();
}

void InstrumentPlot::clear_display() {
    snapshot_ = {};
    spectrogram_history_.clear();
    update();
}

void InstrumentPlot::set_snapshot(const fv1::AnalysisSnapshot& snapshot) {
    snapshot_ = snapshot;
    if (kind_ == PlotKind::Spectrogram && !snapshot.spectrum_db.empty()) {
        spectrogram_history_.push_back(snapshot.spectrum_db);
        constexpr std::size_t max_columns = 160;
        if (spectrogram_history_.size() > max_columns)
            spectrogram_history_.erase(spectrogram_history_.begin(),
                                       spectrogram_history_.begin() + static_cast<std::ptrdiff_t>(spectrogram_history_.size() - max_columns));
    }
    update();
}

void InstrumentPlot::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect().adjusted(10, 10, -10, -10);
    const QColor bg = palette().color(QPalette::Base);
    const QColor fg = palette().color(QPalette::Text);
    const QColor accent = qApp->property("fv1ThemeAccent").value<QColor>();
    const QColor gridMajor = qApp->property("fv1ThemeGridMajor").value<QColor>();
    const QColor gridMinor = qApp->property("fv1ThemeGridMinor").value<QColor>();
    p.fillRect(rect(), bg);

    p.setPen(QPen(gridMinor, 1));
    for (int i = 1; i < 20; ++i) {
        const double x = r.left() + r.width() * i / 20.0;
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
    }
    for (int i = 1; i < 10; ++i) {
        const double y = r.top() + r.height() * i / 10.0;
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }
    p.setPen(QPen(gridMajor, 1));
    p.drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));
    p.drawLine(QPointF(r.left(), r.top()), QPointF(r.left(), r.bottom()));

    if (kind_ == PlotKind::Oscilloscope && !snapshot_.scope_frames.empty()) {
        QPainterPath left, right;
        const auto n = snapshot_.scope_frames.size();
        for (std::size_t i = 0; i < n; ++i) {
            const double t = n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.0;
            const double x = r.left() + t * r.width();
            const QPointF pl(x, r.center().y() - std::clamp(static_cast<double>(snapshot_.scope_frames[i].left), -1.0, 1.0) * r.height() * 0.45);
            const QPointF pr(x, r.center().y() - std::clamp(static_cast<double>(snapshot_.scope_frames[i].right), -1.0, 1.0) * r.height() * 0.45);
            if (i == 0) { left.moveTo(pl); right.moveTo(pr); }
            else { left.lineTo(pl); right.lineTo(pr); }
        }
        p.setPen(QPen(accent, 1.7)); p.drawPath(left);
        p.setPen(QPen(accent.lighter(145), 1.2)); p.drawPath(right);
    } else if (kind_ == PlotKind::Spectrum && !snapshot_.spectrum_db.empty()) {
        QPainterPath spectrum;
        const auto n = snapshot_.spectrum_db.size();
        for (std::size_t i = 0; i < n; ++i) {
            const double t = n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.0;
            const double x = r.left() + t * r.width();
            const double norm = std::clamp((static_cast<double>(snapshot_.spectrum_db[i]) + 100.0) / 100.0, 0.0, 1.0);
            const QPointF pt(x, r.bottom() - norm * r.height());
            if (i == 0) spectrum.moveTo(pt); else spectrum.lineTo(pt);
        }
        p.setPen(QPen(accent, 1.7)); p.drawPath(spectrum);
    } else if (kind_ == PlotKind::Spectrogram && !spectrogram_history_.empty()) {
        const double column_w = r.width() / static_cast<double>(spectrogram_history_.size());
        for (std::size_t x = 0; x < spectrogram_history_.size(); ++x) {
            const auto& spectrum = spectrogram_history_[x];
            const std::size_t rows = std::min<std::size_t>(spectrum.size(), 256);
            for (std::size_t y = 0; y < rows; ++y) {
                const std::size_t bin = y * spectrum.size() / rows;
                const double energy = std::clamp((static_cast<double>(spectrum[bin]) + 100.0) / 100.0, 0.0, 1.0);
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
    }

    p.setPen(fg);
    QString title;
    if (kind_ == PlotKind::Oscilloscope) title = QStringLiteral("L / R TIME DOMAIN");
    else if (kind_ == PlotKind::Spectrum) title = QStringLiteral("FFT SPECTRUM");
    else if (kind_ == PlotKind::Spectrogram) title = QStringLiteral("SPECTROGRAM HISTORY");
    else title = QStringLiteral("OUTPUT LEVELS");
    title += QStringLiteral("    [%1]").arg(signal_label_);
    if (snapshot_.sequence != 0 && kind_ == PlotKind::Spectrum) {
        title += QStringLiteral("    dominant %1 Hz").arg(snapshot_.dominant_frequency_hz, 0, 'f', 1);
    }
    p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, title);
}

} // namespace fv1::gui
