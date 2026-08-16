import Foundation
import SwiftUI

extension Notification.Name {
    static let fv1AnalyzerToggleFreezeAll =
        Notification.Name(
            "FV1AnalyzerToggleFreezeAll"
        )

    static let fv1AnalyzerClearAll =
        Notification.Name(
            "FV1AnalyzerClearAll"
        )

    static let fv1AnalyzerOverlayChanged =
        Notification.Name(
            "FV1AnalyzerOverlayChanged"
        )
}

enum FV1ScopeTriggerMode: String, CaseIterable, Identifiable {
    case off = "Off"
    case auto = "Auto"
    case normal = "Normal"
    case single = "Single"
    var id: String { rawValue }
}

enum FV1ScopeTriggerChannel: String, CaseIterable, Identifiable {
    case left = "Left"
    case right = "Right"
    var id: String { rawValue }
}

enum FV1ScopeTriggerSlope: String, CaseIterable, Identifiable {
    case rising = "Rising"
    case falling = "Falling"
    var id: String { rawValue }
}

struct FV1ScopeAnalyzerView: View {
    let raw: AppleAnalysisSnapshot
    let processed: AppleAnalysisSnapshot

    @State private var showRaw = true
    @State private var showProcessed = true
    @State private var gain = 1.0
    @State private var zoom = 1.0
    @State private var triggerMode: FV1ScopeTriggerMode = .auto
    @State private var triggerChannel: FV1ScopeTriggerChannel = .left
    @State private var triggerSlope: FV1ScopeTriggerSlope = .rising
    @State private var triggerLevel = 0.0
    @State private var frozen = false
    @State private var singleArmed = false
    @State private var displayRaw = AppleAnalysisSnapshot.empty
    @State private var displayProcessed = AppleAnalysisSnapshot.empty

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 10) {
                Toggle("RAW", isOn: $showRaw)
                Toggle("PROCESSED", isOn: $showProcessed)

                Picker("Trigger", selection: $triggerMode) {
                    ForEach(FV1ScopeTriggerMode.allCases) { mode in
                        Text(mode.rawValue).tag(mode)
                    }
                }
                .frame(width: 145)

                Picker("Channel", selection: $triggerChannel) {
                    ForEach(FV1ScopeTriggerChannel.allCases) { channel in
                        Text(channel.rawValue).tag(channel)
                    }
                }
                .frame(width: 130)

                Picker("Slope", selection: $triggerSlope) {
                    ForEach(FV1ScopeTriggerSlope.allCases) { slope in
                        Text(slope.rawValue).tag(slope)
                    }
                }
                .frame(width: 140)

                if triggerMode == .single {
                    Button(singleArmed ? "Armed" : "Re-arm") {
                        singleArmed = true
                    }
                }

                Spacer()

                Button(frozen ? "Unfreeze" : "Freeze") {
                    frozen.toggle()
                    if !frozen { updateDisplay() }
                }

                Button("Clear") {
                    displayRaw = .empty
                    displayProcessed = .empty
                }

                #if os(macOS)
                Button("CSV") {
                    MacExportPresenter.saveCSV(
                        suggestedName: "fv1-scope.csv",
                        contents: scopeCSV()
                    )
                }
                #endif
            }
            .font(.caption)

            HStack(spacing: 14) {
                LabeledContent("Time zoom") {
                    Slider(value: $zoom, in: 1...8, step: 1)
                        .frame(width: 120)
                }

                LabeledContent("Gain") {
                    Slider(value: $gain, in: 0.25...8)
                        .frame(width: 120)
                }

                if triggerMode != .off {
                    LabeledContent("Trigger level") {
                        Slider(value: $triggerLevel, in: -1...1)
                            .frame(width: 150)
                    }

                    Text(
                        triggerLevel,
                        format: .number.precision(.fractionLength(2))
                    )
                    .monospacedDigit()
                }

                Spacer()
            }
            .font(.caption)

            Phase8BScopeCanvas(
                raw: displayRaw,
                processed: displayProcessed,
                showRaw: showRaw,
                showProcessed: showProcessed,
                gain: gain,
                zoom: zoom
            )
            .frame(minHeight: 330)

            HStack {
                Text(
                    "RAW  L \(dbText(displayRaw.rmsLeft))  R \(dbText(displayRaw.rmsRight))"
                )
                Spacer()
                Text(
                    "PROCESSED  L \(dbText(displayProcessed.rmsLeft))  R \(dbText(displayProcessed.rmsRight))"
                )
            }
            .font(.caption2.monospaced())
            .foregroundStyle(.secondary)
        }
        .padding(8)
        .onAppear {
            displayRaw = raw
            displayProcessed = processed
        }
        .onChange(of: raw.sequence) { _, _ in updateDisplay() }
        .onChange(of: processed.sequence) { _, _ in updateDisplay() }
        .onChange(of: triggerMode) { _, mode in
            if mode == .single { singleArmed = true }
            updateDisplay()
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerToggleFreezeAll
            )
        ) { _ in
            frozen.toggle()
            if !frozen { updateDisplay() }
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerClearAll
            )
        ) { _ in
            displayRaw = .empty
            displayProcessed = .empty
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerOverlayChanged
            )
        ) { notification in
            if let enabled =
                notification.object as? Bool {
                showRaw = enabled
                showProcessed = true
            }
        }
        .onAppear {
            let defaults = UserDefaults.standard
            let key = "analysis/rawProcessedOverlay"
            if defaults.object(forKey: key) != nil {
                showRaw = defaults.bool(forKey: key)
            }
        }
    }

    private func updateDisplay() {
        guard !frozen else { return }

        let triggerSnapshot = showProcessed ? processed : raw
        let crossing = hasCrossing(triggerSnapshot)

        switch triggerMode {
        case .off, .auto:
            displayRaw = raw
            displayProcessed = processed

        case .normal:
            if crossing {
                displayRaw = raw
                displayProcessed = processed
            }

        case .single:
            if singleArmed && crossing {
                displayRaw = raw
                displayProcessed = processed
                singleArmed = false
            }
        }
    }

    private func hasCrossing(_ snapshot: AppleAnalysisSnapshot) -> Bool {
        let samples =
            triggerChannel == .left ? snapshot.scopeLeft : snapshot.scopeRight

        guard samples.count > 1 else { return false }

        let threshold = Float(triggerLevel)
        for index in 1..<samples.count {
            let previous = samples[index - 1]
            let current = samples[index]

            if triggerSlope == .rising {
                if previous < threshold && current >= threshold { return true }
            } else if previous > threshold && current <= threshold {
                return true
            }
        }
        return false
    }

    private func scopeCSV() -> String {
        let count = max(displayRaw.scopeLeft.count, displayProcessed.scopeLeft.count)
        var text = "index,raw_left,raw_right,processed_left,processed_right\n"

        for index in 0..<count {
            text += [
                String(index),
                csvValue(displayRaw.scopeLeft, index),
                csvValue(displayRaw.scopeRight, index),
                csvValue(displayProcessed.scopeLeft, index),
                csvValue(displayProcessed.scopeRight, index)
            ].joined(separator: ",") + "\n"
        }
        return text
    }
}

private struct Phase8BScopeCanvas: View {
    @Environment(
        \.fv1ThemePalette
    )
    private var theme

    let raw: AppleAnalysisSnapshot
    let processed: AppleAnalysisSnapshot
    let showRaw: Bool
    let showProcessed: Bool
    let gain: Double
    let zoom: Double

    var body: some View {
        Canvas { context, size in
            drawGrid(context: &context, size: size)

            if showRaw {
                drawStereo(
                    snapshot: raw,
                    color: theme.rawTrace,
                    context: &context,
                    size: size
                )
            }
            if showProcessed {
                drawStereo(
                    snapshot: processed,
                    color: theme.accent,
                    context: &context,
                    size: size
                )
            }
        }
        .background(theme.panel)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .accessibilityLabel("Raw and processed FV-1 oscilloscope")
    }

    private func drawGrid(context: inout GraphicsContext, size: CGSize) {
        for i in 1..<10 {
            let x = size.width * CGFloat(i) / 10
            var path = Path()
            path.move(to: CGPoint(x: x, y: 0))
            path.addLine(to: CGPoint(x: x, y: size.height))
            context.stroke(path, with: .color(theme.gridMinor), lineWidth: 0.5)
        }
        for i in 1..<8 {
            let y = size.height * CGFloat(i) / 8
            var path = Path()
            path.move(to: CGPoint(x: 0, y: y))
            path.addLine(to: CGPoint(x: size.width, y: y))
            context.stroke(path, with: .color(theme.gridMinor), lineWidth: 0.5)
        }
    }

    private func drawStereo(
        snapshot: AppleAnalysisSnapshot,
        color: Color,
        context: inout GraphicsContext,
        size: CGSize
    ) {
        drawTrace(
            samples: visible(snapshot.scopeLeft),
            center: size.height * 0.34,
            color: color,
            context: &context,
            size: size
        )
        drawTrace(
            samples: visible(snapshot.scopeRight),
            center: size.height * 0.66,
            color: color.opacity(0.65),
            context: &context,
            size: size
        )
    }

    private func visible(_ samples: [Float]) -> [Float] {
        guard !samples.isEmpty else { return [] }
        let count = min(
            samples.count,
            max(2, Int(Double(samples.count) / max(1, zoom)))
        )
        return Array(samples.suffix(count))
    }

    private func drawTrace(
        samples: [Float],
        center: CGFloat,
        color: Color,
        context: inout GraphicsContext,
        size: CGSize
    ) {
        guard samples.count > 1 else { return }

        var path = Path()
        let vertical = size.height * 0.145 * CGFloat(gain)

        for (index, sample) in samples.enumerated() {
            let x = size.width * CGFloat(index) / CGFloat(samples.count - 1)
            let y = center - CGFloat(sample) * vertical

            if index == 0 {
                path.move(to: CGPoint(x: x, y: y))
            } else {
                path.addLine(to: CGPoint(x: x, y: y))
            }
        }

        context.stroke(path, with: .color(color), lineWidth: 1.25)
    }
}

struct FV1SpectrumAnalyzerView: View {
    let raw: AppleAnalysisSnapshot
    let processed: AppleAnalysisSnapshot

    @State private var showRaw = true
    @State private var showProcessed = true
    @State private var logFrequency = true
    @State private var floorDB = -100.0
    @State private var peakHold = false
    @State private var frozen = false
    @State private var frozenRaw = AppleAnalysisSnapshot.empty
    @State private var frozenProcessed = AppleAnalysisSnapshot.empty
    @State private var rawPeaks: [Float] = []
    @State private var processedPeaks: [Float] = []

    private var visibleRaw: AppleAnalysisSnapshot { frozen ? frozenRaw : raw }
    private var visibleProcessed: AppleAnalysisSnapshot { frozen ? frozenProcessed : processed }

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 10) {
                Toggle("RAW", isOn: $showRaw)
                Toggle("PROCESSED", isOn: $showProcessed)
                Toggle("Log Frequency", isOn: $logFrequency)
                Toggle("Peak Hold", isOn: $peakHold)

                Picker("Floor", selection: $floorDB) {
                    ForEach([-60.0, -80.0, -100.0, -120.0], id: \.self) {
                        Text("\(Int($0)) dB").tag($0)
                    }
                }
                .frame(width: 145)

                Spacer()

                Button(frozen ? "Unfreeze" : "Freeze") {
                    if !frozen {
                        frozenRaw = raw
                        frozenProcessed = processed
                    }
                    frozen.toggle()
                }

                Button("Clear Peaks") {
                    rawPeaks = []
                    processedPeaks = []
                }

                #if os(macOS)
                Button("CSV") {
                    MacExportPresenter.saveCSV(
                        suggestedName: "fv1-spectrum.csv",
                        contents: spectrumCSV()
                    )
                }
                #endif
            }
            .font(.caption)

            Phase8BSpectrumCanvas(
                raw: visibleRaw,
                processed: visibleProcessed,
                showRaw: showRaw,
                showProcessed: showProcessed,
                logFrequency: logFrequency,
                floorDB: floorDB,
                rawPeaks: peakHold ? rawPeaks : [],
                processedPeaks: peakHold ? processedPeaks : []
            )
            .frame(minHeight: 350)

            HStack {
                Text("RAW dominant  \(frequencyText(visibleRaw.dominantFrequency))")
                Spacer()
                Text("PROCESSED dominant  \(frequencyText(visibleProcessed.dominantFrequency))")
            }
            .font(.caption.monospaced())
            .foregroundStyle(.secondary)
        }
        .padding(8)
        .onChange(of: raw.sequence) { _, _ in updatePeaks() }
        .onChange(of: processed.sequence) { _, _ in updatePeaks() }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerToggleFreezeAll
            )
        ) { _ in
            if !frozen {
                frozenRaw = raw
                frozenProcessed = processed
            }
            frozen.toggle()
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerClearAll
            )
        ) { _ in
            rawPeaks = []
            processedPeaks = []
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerOverlayChanged
            )
        ) { notification in
            if let enabled =
                notification.object as? Bool {
                showRaw = enabled
                showProcessed = true
            }
        }
        .onAppear {
            let defaults = UserDefaults.standard
            let key = "analysis/rawProcessedOverlay"
            if defaults.object(forKey: key) != nil {
                showRaw = defaults.bool(forKey: key)
            }
        }
    }

    private func updatePeaks() {
        guard peakHold, !frozen else { return }
        rawPeaks = held(old: rawPeaks, new: raw.spectrumDB)
        processedPeaks = held(old: processedPeaks, new: processed.spectrumDB)
    }

    private func held(old: [Float], new: [Float]) -> [Float] {
        guard !new.isEmpty else { return old }
        guard old.count == new.count else { return new }
        return zip(old, new).map { max($0.0, $0.1) }
    }

    private func spectrumCSV() -> String {
        let count = max(visibleRaw.spectrumDB.count, visibleProcessed.spectrumDB.count)
        let rate = visibleProcessed.sampleRate > 0
            ? visibleProcessed.sampleRate
            : visibleRaw.sampleRate
        let fftSize = max(2, (count - 1) * 2)

        var text = "frequency_hz,raw_db,processed_db\n"
        for index in 0..<count {
            let frequency = rate * Double(index) / Double(fftSize)
            text += [
                String(frequency),
                csvValue(visibleRaw.spectrumDB, index),
                csvValue(visibleProcessed.spectrumDB, index)
            ].joined(separator: ",") + "\n"
        }
        return text
    }
}

private struct Phase8BSpectrumCanvas: View {
    @Environment(
        \.fv1ThemePalette
    )
    private var theme

    let raw: AppleAnalysisSnapshot
    let processed: AppleAnalysisSnapshot
    let showRaw: Bool
    let showProcessed: Bool
    let logFrequency: Bool
    let floorDB: Double
    let rawPeaks: [Float]
    let processedPeaks: [Float]

    var body: some View {
        Canvas { context, size in
            for i in 1..<6 {
                let y = size.height * CGFloat(i) / 6
                var grid = Path()
                grid.move(to: CGPoint(x: 0, y: y))
                grid.addLine(to: CGPoint(x: size.width, y: y))
                context.stroke(grid, with: .color(theme.gridMinor), lineWidth: 0.5)
            }

            if showRaw {
                draw(raw.spectrumDB, snapshot: raw, color: theme.rawTrace, context: &context, size: size)
                draw(rawPeaks, snapshot: raw, color: theme.text.opacity(0.30), context: &context, size: size)
            }
            if showProcessed {
                draw(processed.spectrumDB, snapshot: processed, color: theme.accent, context: &context, size: size)
                draw(processedPeaks, snapshot: processed, color: theme.accent.opacity(0.30), context: &context, size: size)
            }
        }
        .background(theme.panel)
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    private func draw(
        _ values: [Float],
        snapshot: AppleAnalysisSnapshot,
        color: Color,
        context: inout GraphicsContext,
        size: CGSize
    ) {
        guard values.count > 1 else { return }

        let nyquist = max(1.0, snapshot.sampleRate * 0.5)
        var path = Path()

        for index in values.indices {
            let frequency = nyquist * Double(index) / Double(values.count - 1)
            let xFraction: Double

            if logFrequency {
                let minimum = 20.0
                let denominator = max(1.0e-9, log10(nyquist / minimum))
                xFraction = max(
                    0,
                    min(
                        1,
                        log10(max(minimum, frequency) / minimum) / denominator
                    )
                )
            } else {
                xFraction = frequency / nyquist
            }

            let value = max(floorDB, min(0, Double(values[index])))
            let yFraction = (0 - value) / (0 - floorDB)

            let point = CGPoint(
                x: size.width * CGFloat(xFraction),
                y: size.height * CGFloat(yFraction)
            )

            if index == values.startIndex {
                path.move(to: point)
            } else {
                path.addLine(to: point)
            }
        }

        context.stroke(path, with: .color(color), lineWidth: 1.2)
    }
}

struct FV1SpectrogramView: View {
    @Environment(
        \.fv1ThemePalette
    )
    private var theme

    let raw: AppleAnalysisSnapshot
    let processed: AppleAnalysisSnapshot

    @State private var stream: AppleAnalysisStream = .processed
    @State private var floorDB = -100.0
    @State private var frozen = false
    @State private var historyDepth = 120.0
    @State private var history: [[Float]] = []
    @State private var lastSequence: UInt64 = 0

    private var selected: AppleAnalysisSnapshot {
        stream == .raw ? raw : processed
    }

    var body: some View {
        VStack(spacing: 8) {
            HStack {
                Picker("Stream", selection: $stream) {
                    ForEach(AppleAnalysisStream.allCases) {
                        Text($0.label).tag($0)
                    }
                }
                .frame(width: 180)

                Picker("Floor", selection: $floorDB) {
                    ForEach([-60.0, -80.0, -100.0, -120.0], id: \.self) {
                        Text("\(Int($0)) dB").tag($0)
                    }
                }
                .frame(width: 145)

                LabeledContent("History") {
                    Slider(value: $historyDepth, in: 40...240, step: 20)
                        .frame(width: 140)
                }

                Spacer()

                Button(frozen ? "Unfreeze" : "Freeze") {
                    frozen.toggle()
                }

                Button("Clear") {
                    history = []
                    lastSequence = 0
                }
            }
            .font(.caption)

            Canvas { context, size in
                guard !history.isEmpty else { return }

                let columns = history.count
                let bins = history[0].count
                let cellWidth = size.width / CGFloat(max(1, columns))
                let cellHeight = size.height / CGFloat(max(1, bins))

                for columnIndex in 0..<columns {
                    let column = history[columnIndex]
                    for binIndex in 0..<min(bins, column.count) {
                        let normalized = max(
                            0,
                            min(
                                1,
                                (Double(column[binIndex]) - floorDB) / -floorDB
                            )
                        )

                        let color = Color(
                            hue: 0.66 - normalized * 0.66,
                            saturation: 0.90,
                            brightness: 0.16 + normalized * 0.84
                        )

                        let rect = CGRect(
                            x: CGFloat(columnIndex) * cellWidth,
                            y: size.height - CGFloat(binIndex + 1) * cellHeight,
                            width: cellWidth + 0.5,
                            height: cellHeight + 0.5
                        )
                        context.fill(Path(rect), with: .color(color))
                    }
                }
            }
            .background(theme.panel)
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .frame(minHeight: 360)

            HStack {
                Text(stream.label)
                Spacer()
                Text("\(history.count) history columns")
            }
            .font(.caption2.monospaced())
            .foregroundStyle(.secondary)
        }
        .padding(8)
        .onChange(of: raw.sequence) { _, _ in appendIfNeeded() }
        .onChange(of: processed.sequence) { _, _ in appendIfNeeded() }
        .onChange(of: stream) { _, _ in
            history = []
            lastSequence = 0
            appendIfNeeded()
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerToggleFreezeAll
            )
        ) { _ in
            frozen.toggle()
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerClearAll
            )
        ) { _ in
            history = []
            lastSequence = 0
        }
    }

    private func appendIfNeeded() {
        guard !frozen,
              selected.sequence != 0,
              selected.sequence != lastSequence,
              !selected.spectrumDB.isEmpty else {
            return
        }

        lastSequence = selected.sequence

        let displayBins = 128
        var column = [Float](repeating: Float(floorDB), count: displayBins)

        for displayIndex in 0..<displayBins {
            let sourceIndex =
                displayIndex * max(1, selected.spectrumDB.count - 1)
                / max(1, displayBins - 1)
            column[displayIndex] = selected.spectrumDB[
                min(sourceIndex, selected.spectrumDB.count - 1)
            ]
        }

        history.append(column)
        let maximum = Int(historyDepth)
        if history.count > maximum {
            history.removeFirst(history.count - maximum)
        }
    }
}

struct FV1LevelsAnalyzerView: View {
    let raw: AppleAnalysisSnapshot
    let processed: AppleAnalysisSnapshot

    @State private var frozen = false
    @State private var frozenRaw =
        AppleAnalysisSnapshot.empty
    @State private var frozenProcessed =
        AppleAnalysisSnapshot.empty

    private var visibleRaw:
        AppleAnalysisSnapshot {
        frozen ? frozenRaw : raw
    }

    private var visibleProcessed:
        AppleAnalysisSnapshot {
        frozen ? frozenProcessed : processed
    }

    var body: some View {
        HStack(spacing: 18) {
            levelPanel(
                "RAW INPUT",
                snapshot: visibleRaw
            )
            levelPanel(
                "PROCESSED",
                snapshot: visibleProcessed
            )
        }
        .padding(12)
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerToggleFreezeAll
            )
        ) { _ in
            if !frozen {
                frozenRaw = raw
                frozenProcessed = processed
            }
            frozen.toggle()
        }
        .onReceive(
            NotificationCenter.default.publisher(
                for: .fv1AnalyzerClearAll
            )
        ) { _ in
            if frozen {
                frozenRaw = .empty
                frozenProcessed = .empty
            }
        }
    }

    private func levelPanel(
        _ title: String,
        snapshot: AppleAnalysisSnapshot
    ) -> some View {
        GroupBox(title) {
            VStack(alignment: .leading, spacing: 14) {
                meter("Peak L", value: snapshot.peakLeft)
                meter("Peak R", value: snapshot.peakRight)
                meter("RMS L", value: snapshot.rmsLeft)
                meter("RMS R", value: snapshot.rmsRight)

                Divider()

                LabeledContent("Correlation") {
                    Text(
                        snapshot.correlation,
                        format: .number.precision(.fractionLength(3))
                    )
                    .monospacedDigit()
                }

                ProgressView(
                    value: Double((snapshot.correlation + 1) * 0.5)
                )

                LabeledContent("Dominant") {
                    Text(frequencyText(snapshot.dominantFrequency))
                        .monospacedDigit()
                }

                LabeledContent("Analyzer drops") {
                    Text("\(snapshot.droppedFrames)")
                        .monospacedDigit()
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(4)
        }
        .frame(maxWidth: .infinity)
    }

    private func meter(_ title: String, value: Float) -> some View {
        let level = dbValue(value)
        let normalized = max(0, min(1, (Double(level) + 80) / 80))

        return VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(title)
                Spacer()
                Text(String(format: "%.1f dBFS", level))
                    .monospacedDigit()
            }
            ProgressView(value: normalized)
        }
    }
}

private func dbValue(_ value: Float) -> Float {
    guard value > 1.0e-8 else { return -160 }
    return 20 * log10(value)
}

private func dbText(_ value: Float) -> String {
    String(format: "%.1f dBFS", dbValue(value))
}

private func frequencyText(_ frequency: Float) -> String {
    guard frequency > 0 else { return "—" }
    if frequency >= 1000 {
        return String(format: "%.2f kHz", frequency / 1000)
    }
    return String(format: "%.1f Hz", frequency)
}

private func csvValue(_ values: [Float], _ index: Int) -> String {
    guard values.indices.contains(index) else { return "" }
    return String(values[index])
}
