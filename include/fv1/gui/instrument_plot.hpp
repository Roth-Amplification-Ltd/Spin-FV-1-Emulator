#pragma once

#include <fv1/analysis.hpp>

#include <QString>
#include <QWidget>

#include <vector>

namespace fv1::gui {

enum class PlotKind { Oscilloscope, Spectrum, Spectrogram, Levels };

class InstrumentPlot final : public QWidget {
public:
    explicit InstrumentPlot(PlotKind kind, QWidget* parent = nullptr);
    QSize minimumSizeHint() const override;
    void set_snapshot(const fv1::AnalysisSnapshot& snapshot);
    void set_signal_label(const QString& label);
    void clear_display();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    PlotKind kind_;
    fv1::AnalysisSnapshot snapshot_;
    std::vector<std::vector<float>> spectrogram_history_;
    QString signal_label_{QStringLiteral("PROCESSED")};
};

} // namespace fv1::gui
