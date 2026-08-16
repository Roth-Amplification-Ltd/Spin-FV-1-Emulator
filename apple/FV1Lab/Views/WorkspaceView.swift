import SwiftUI
import UniformTypeIdentifiers

struct WorkspaceView: View {
    @StateObject private var model = FV1WorkspaceModel()
    @State private var spinASMSource = ""
    @State private var programName = "No program loaded"
    @State private var importingProgram = false

    private let spinASMType = UTType(
        exportedAs: "com.rothamplification.fv1.spinasm",
        conformingTo: .plainText
    )

    var body: some View {
        dashboard
            .fileImporter(
                isPresented: $importingProgram,
                allowedContentTypes: [spinASMType, .plainText, .data],
                allowsMultipleSelection: false,
                onCompletion: importProgram
            )
            .focusedSceneValue(\.fv1CompileAction) {
                guard !spinASMSource.isEmpty else { return }
                model.compileAndLoad(source: spinASMSource)
            }
    }

    @ViewBuilder
    private var dashboard: some View {
        VStack(spacing: 0) {
            transportBar
            Divider()

            #if os(macOS)
            HSplitView {
                leftColumn
                    .frame(minWidth: 300, idealWidth: 330, maxWidth: 390)

                centerColumn
                    .frame(minWidth: 650)

                rightColumn
                    .frame(minWidth: 390, idealWidth: 440, maxWidth: 540)
            }
            // The persistent three-column engineering layout needs
            // roughly 300 + 650 + 390 points before split-view dividers.
            // Advertise the real minimum and a useful desktop ideal size.
            .frame(
                minWidth: 1360,
                idealWidth: 1500,
                minHeight: 760,
                idealHeight: 860
            )
            #else
            GeometryReader { proxy in
                HStack(spacing: 8) {
                    leftColumn
                        .frame(width: max(250, proxy.size.width * 0.24))

                    centerColumn
                        .frame(maxWidth: .infinity)

                    rightColumn
                        .frame(width: max(320, proxy.size.width * 0.30))
                }
                .padding(8)
            }
            #endif

            Divider()
            statusFooter
        }
    }

    private var transportBar: some View {
        HStack(spacing: 10) {
            Button {
                model.audio.toggle()
            } label: {
                Label(
                    model.audio.isRunning ? "Stop" : "Start",
                    systemImage: model.audio.isRunning ? "stop.fill" : "play.fill"
                )
            }
            .buttonStyle(.borderedProminent)

            Button {
                importingProgram = true
            } label: {
                Label("Open Program", systemImage: "folder")
            }

            Button {
                guard !spinASMSource.isEmpty else { return }
                model.compileAndLoad(source: spinASMSource)
            } label: {
                Label("Compile & Load", systemImage: "hammer")
            }
            .disabled(spinASMSource.isEmpty)

            Divider().frame(height: 22)

            Label(
                model.audio.isRunning ? "AUDIO RUNNING" : "AUDIO STOPPED",
                systemImage: model.audio.isRunning ? "waveform.circle.fill" : "waveform.circle"
            )
            .font(.caption.bold())

            Spacer()

            Text(model.compileSummary)
                .font(.caption)
                .foregroundStyle(.secondary)
                .lineLimit(1)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
    }

    private var leftColumn: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 10) {
                GroupBox("PROGRAM") {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(programName)
                            .font(.headline)
                            .lineLimit(2)

                        Text(model.compileSummary)
                            .font(.caption)
                            .foregroundStyle(.secondary)

                        HStack {
                            Button("Open .spn / .bin") {
                                importingProgram = true
                            }

                            Button("Reload") {
                                guard !spinASMSource.isEmpty else { return }
                                model.compileAndLoad(source: spinASMSource)
                            }
                            .disabled(spinASMSource.isEmpty)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }

                GroupBox("INPUT SOURCE") {
                    VStack(alignment: .leading, spacing: 8) {
                        Picker("Mode", selection: $model.audio.sourceMode) {
                            ForEach(AppleAudioSourceMode.allCases) { mode in
                                Text(mode.rawValue).tag(mode)
                            }
                        }

                        if model.audio.sourceMode == .testGenerator {
                            Divider()

                            Picker("Signal", selection: $model.audio.generatorKind) {
                                ForEach(AppleTestSignalKind.allCases) { kind in
                                    Text(kind.rawValue).tag(kind)
                                }
                            }

                            if model.audio.generatorKind == .sine ||
                               model.audio.generatorKind == .sweep {
                                LabeledContent("Start / Tone Frequency") {
                                    HStack(spacing: 4) {
                                        TextField(
                                            "Hz",
                                            value: $model.audio.generatorFrequency,
                                            format: .number
                                        )
                                        .frame(width: 82)
                                        Text("Hz")
                                    }
                                }
                            }

                            LabeledContent("Amplitude") {
                                HStack(spacing: 6) {
                                    Slider(
                                        value: $model.audio.generatorAmplitude,
                                        in: 0...1
                                    )
                                    Text(
                                        model.audio.generatorAmplitude,
                                        format: .number.precision(.fractionLength(2))
                                    )
                                    .monospacedDigit()
                                    .frame(width: 36)
                                }
                            }

                            if model.audio.generatorKind == .sweep {
                                LabeledContent("Sweep End") {
                                    HStack(spacing: 4) {
                                        TextField(
                                            "Hz",
                                            value: $model.audio.generatorSweepEnd,
                                            format: .number
                                        )
                                        .frame(width: 82)
                                        Text("Hz")
                                    }
                                }

                                LabeledContent("Sweep Time") {
                                    HStack(spacing: 4) {
                                        TextField(
                                            "Seconds",
                                            value: $model.audio.generatorSweepSeconds,
                                            format: .number
                                        )
                                        .frame(width: 72)
                                        Text("s")
                                    }
                                }
                            }

                            if model.audio.generatorKind == .impulse {
                                LabeledContent("Impulse Period") {
                                    HStack(spacing: 4) {
                                        TextField(
                                            "Seconds",
                                            value: $model.audio.generatorImpulsePeriod,
                                            format: .number
                                        )
                                        .frame(width: 72)
                                        Text("s")
                                    }
                                }
                            }
                        }

                        Divider()

                        Text(model.audio.routeDescription)
                            .font(.caption)
                            .foregroundStyle(.secondary)

                        Button(model.audio.isRunning ? "Stop Audio" : "Start Audio") {
                            model.audio.toggle()
                        }
                        .buttonStyle(.borderedProminent)

                        if model.audio.inputSampleRate > 0 {
                            LabeledContent(
                                model.audio.sourceMode == .testGenerator
                                    ? "Generator"
                                    : "Input",
                                value: "\(Int(model.audio.inputSampleRate)) Hz"
                            )
                            LabeledContent(
                                "Output",
                                value: "\(Int(model.audio.outputSampleRate)) Hz"
                            )
                            LabeledContent("FV-1", value: "32768 Hz")
                        }

                        if !model.audio.lastError.isEmpty {
                            Text(model.audio.lastError)
                                .font(.caption)
                                .foregroundStyle(.red)
                                .textSelection(.enabled)
                        }

                        #if os(iOS)
                        if model.audio.sourceMode == .audioInterface &&
                           !model.audio.availableInputs.isEmpty {
                            Divider()
                            Text("AVAILABLE INPUTS")
                                .font(.caption.bold())

                            ForEach(model.audio.availableInputs, id: \.uid) { port in
                                Button(port.portName) {
                                    model.audio.selectInput(uid: port.uid)
                                }
                                .buttonStyle(.plain)
                            }
                        }
                        #endif
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }

                GroupBox("VIRTUAL FV-1 CONTROLS") {
                    VStack(spacing: 12) {
                        pot("POT0", value: $model.pot0)
                        pot("POT1", value: $model.pot1)
                        pot("POT2", value: $model.pot2)

                        HStack {
                            Button("Reset Chip") { model.reset() }
                            Button("Refresh State") { model.refreshInspection() }
                        }
                    }
                }

                GroupBox("AUDIO TELEMETRY") {
                    Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 5) {
                        telemetryRow("Input frames", "\(model.audio.inputFrames)")
                        telemetryRow("FV-1 frames", "\(model.audio.chipFrames)")
                        telemetryRow("Underflows", "\(model.audio.underflows)")
                        telemetryRow("Overflows", "\(model.audio.overflows)")
                    }
                    .font(.caption)
                    .monospacedDigit()
                }
            }
            .padding(8)
        }
    }

    private var centerColumn: some View {
        VStack(spacing: 8) {
            TabView {
                analyzerScope
                    .tabItem { Label("OSCILLOSCOPE", systemImage: "waveform") }

                analyzerPlaceholder(
                    "SPECTRUM",
                    "Spectrum analyzer parity is not yet wired into the Apple bridge."
                )
                .tabItem { Label("SPECTRUM", systemImage: "chart.bar.xaxis") }

                analyzerPlaceholder(
                    "SPECTROGRAM",
                    "Spectrogram parity is not yet wired into the Apple bridge."
                )
                .tabItem { Label("SPECTROGRAM", systemImage: "water.waves") }

                analyzerPlaceholder(
                    "LEVELS",
                    "Raw/processed level analysis remains to be ported from the Linux analyzer."
                )
                .tabItem { Label("LEVELS", systemImage: "gauge.with.dots.needle.50percent") }

                analyzerPlaceholder(
                    "VALIDATION",
                    "Hardware-validation tools remain available in the core and need their native Apple panel."
                )
                .tabItem { Label("VALIDATION", systemImage: "checkmark.seal") }
            }

            HStack(alignment: .top, spacing: 8) {
                delayRAMPanel
                resourcePanel
                dspStatusPanel
            }
            .frame(minHeight: 180, maxHeight: 220)
        }
        .padding(8)
    }

    private var analyzerScope: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("REALTIME STEREO OUTPUT")
                    .font(.caption.bold())
                Spacer()
                Text(model.audio.routeDescription)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            ScopeView(
                left: model.audio.scopeLeft,
                right: model.audio.scopeRight
            )
            .frame(minHeight: 360)
        }
        .padding(8)
    }

    private func analyzerPlaceholder(_ title: String, _ message: String) -> some View {
        VStack(spacing: 12) {
            Image(systemName: "waveform.path.ecg")
                .font(.system(size: 42))
                .foregroundStyle(.secondary)

            Text(title)
                .font(.headline)

            Text(message)
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
                .frame(maxWidth: 460)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding()
    }

    private var delayRAMPanel: some View {
        GroupBox("DELAY RAM VIEWER") {
            ScrollView {
                Text(
                    "Offline chip inspector ready.\n\n"
                    + "The native Apple UI now follows the Linux FV-1 Lab dashboard layout. "
                    + "A detailed live delay-pointer window still needs to be exposed through "
                    + "the public inspection API."
                )
                .font(.system(.caption, design: .monospaced))
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .frame(maxWidth: .infinity)
    }

    private var resourcePanel: some View {
        GroupBox("VIRTUAL DSP RESOURCE USAGE") {
            VStack(alignment: .leading, spacing: 7) {
                if let r = model.resources {
                    resourceProgress(
                        "Program",
                        value: Double(r.usedInstructions),
                        total: 128,
                        detail: "\(r.usedInstructions) / 128"
                    )

                    resourceProgress(
                        "Delay RAM",
                        value: Double(r.highestStaticDelayAddress),
                        total: 32768,
                        detail: "\(r.highestStaticDelayAddress) / 32768"
                    )

                    resourceProgress(
                        "Registers",
                        value: Double(r.generalRegistersUsed),
                        total: 32,
                        detail: "\(r.generalRegistersUsed) / 32"
                    )

                    Text(
                        "Reads \(r.staticDelayReads) · Writes \(r.staticDelayWrites) · Worst path \(r.worstCasePath)"
                    )
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                } else {
                    Text("Load a program to analyze resources.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .frame(maxWidth: .infinity)
    }

    private var dspStatusPanel: some View {
        GroupBox("DSP STATUS") {
            VStack(alignment: .leading, spacing: 7) {
                Text(model.audio.isRunning ? "Running" : "Stopped")
                    .font(.headline)

                Text("Host/FV-1 runtime ready.")
                    .font(.caption)

                Divider()

                Text("FV-1 clock: 32768 Hz")
                    .font(.caption.monospaced())

                Text("Underflows: \(model.audio.underflows)")
                    .font(.caption.monospaced())

                Text("Overflows: \(model.audio.overflows)")
                    .font(.caption.monospaced())
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .frame(maxWidth: .infinity)
    }

    private var rightColumn: some View {
        VStack(spacing: 8) {
            GroupBox("CONSOLE / LOG") {
                ScrollView {
                    Text(model.console)
                        .font(.system(.caption, design: .monospaced))
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
            }
            .frame(minHeight: 220)

            GroupBox("OFFLINE FV-1 CHIP INSPECTOR") {
                InspectionView(model: model)
            }
            .frame(maxHeight: .infinity)
        }
        .padding(8)
    }

    private var statusFooter: some View {
        HStack {
            Text("FV-1 Lab — native Apple frontend")
            Spacer()
            Text(model.audio.isRunning ? "RUNNING" : "READY")
                .monospaced()
            Spacer()
            Text("© 2026 Roth Amplification LTD")
        }
        .font(.caption2)
        .foregroundStyle(.secondary)
        .padding(.horizontal, 10)
        .padding(.vertical, 5)
    }

    private func pot(_ title: String, value: Binding<Double>) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(title)
                Spacer()
                Text(
                    value.wrappedValue,
                    format: .number.precision(.fractionLength(3))
                )
                .monospacedDigit()
            }

            Slider(value: value, in: 0...1)
        }
    }

    @ViewBuilder
    private func telemetryRow(_ title: String, _ value: String) -> some View {
        GridRow {
            Text(title).foregroundStyle(.secondary)
            Text(value)
        }
    }

    private func resourceProgress(
        _ title: String,
        value: Double,
        total: Double,
        detail: String
    ) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack {
                Text(title).font(.caption)
                Spacer()
                Text(detail).font(.caption2).monospacedDigit()
            }
            ProgressView(value: value, total: total)
        }
    }

    private func importProgram(
        _ result: Result<[URL], any Error>
    ) {
        do {
            guard let url = try result.get().first else { return }

            let access = url.startAccessingSecurityScopedResource()
            defer {
                if access {
                    url.stopAccessingSecurityScopedResource()
                }
            }

            let data = try Data(contentsOf: url)
            let ext = url.pathExtension.lowercased()
            let explicitBinaryExtensions: Set<String> = [
                "bin", "rom", "fv1bin"
            ]

            programName = url.lastPathComponent

            if explicitBinaryExtensions.contains(ext) {
                spinASMSource = ""
                model.loadRawProgram(data)
                return
            }

            if !data.contains(0),
               let source = String(data: data, encoding: .utf8) {
                spinASMSource = source
                model.compileAndLoad(source: source)
                return
            }

            if data.count == 512 {
                spinASMSource = ""
                model.loadRawProgram(data)
                return
            }

            throw CocoaError(
                .fileReadCorruptFile,
                userInfo: [
                    NSLocalizedDescriptionKey:
                        "File is neither readable SpinASM text nor a 512-byte raw FV-1 program."
                ]
            )
        } catch {
            programName = "Program open failed"
            model.reportExternalError(
                "Open failed: " + error.localizedDescription
            )
        }
    }
}
