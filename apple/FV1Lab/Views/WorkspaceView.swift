import SwiftUI
import UniformTypeIdentifiers

#if os(macOS)
import AppKit
#endif

struct WorkspaceView: View {
    @StateObject private var model =
        FV1WorkspaceModel()

    @State private var spinASMSource = ""
    @State private var programName =
        "No program loaded"

    @State private var importingProgram =
        false
    @State private var importingAudioLoop =
        false

    @State private var showingAudioSettings =
        false
    @State private var showingGeneratorSettings =
        false
    @State private var showingLoopRegion =
        false

    private let spinASMType =
        UTType(
            exportedAs:
                "com.rothamplification.fv1.spinasm",
            conformingTo: .plainText
        )

    private var wavType: UTType {
        UTType(
            filenameExtension: "wav"
        ) ?? .audio
    }

    var body: some View {
        dashboard
            #if os(iOS)
            .fileImporter(
                isPresented:
                    $importingProgram,
                allowedContentTypes: [
                    spinASMType,
                    .plainText,
                    .data
                ],
                allowsMultipleSelection:
                    false,
                onCompletion:
                    importProgram
            )
            .fileImporter(
                isPresented:
                    $importingAudioLoop,
                allowedContentTypes: [
                    wavType
                ],
                allowsMultipleSelection:
                    false,
                onCompletion:
                    importAudioLoop
            )
            #endif
            .sheet(
                isPresented:
                    $showingGeneratorSettings
            ) {
                TestGeneratorSettingsView(
                    audio: model.audio
                )
            }
            .sheet(
                isPresented:
                    $showingLoopRegion
            ) {
                AudioLoopRegionView(
                    audio: model.audio
                )
            }
            #if os(macOS)
            .sheet(
                isPresented:
                    $showingAudioSettings
            ) {
                MacAudioSettingsView(
                    audio: model.audio,
                    devices:
                        model.audio
                            .macAudioDevices
                )
            }
            #endif
            .focusedSceneValue(
                \.fv1CompileAction
            ) {
                guard !spinASMSource
                    .isEmpty else {
                    return
                }
                model.compileAndLoad(
                    source:
                        spinASMSource
                )
            }
            #if os(macOS)
            .focusedSceneValue(
                \.fv1ToggleDSPAction
            ) {
                model.audio
                    .dspEnabled.toggle()
            }
            .focusedSceneValue(
                \.fv1RecordAction
            ) { mode in
                beginRecording(mode)
            }
            .focusedSceneValue(
                \.fv1StopRecordAction
            ) {
                model.audio
                    .stopRecording()
            }
            .focusedSceneValue(
                \.fv1RecordingState,
                model.audio.isRecording
            )
            #endif
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1OpenProgram
                    )
            ) { _ in
                requestProgramOpen()
            }
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1PasteSpinASM
                    )
            ) { _ in
                #if os(macOS)
                pasteSpinASMFromClipboard()
                #endif
            }
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1OpenAudioLoop
                    )
            ) { _ in
                requestAudioLoopOpen()
            }
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1ShowAudioSettings
                    )
            ) { _ in
                #if os(macOS)
                showingAudioSettings = true
                #endif
            }
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1RefreshAudioDevices
                    )
            ) { _ in
                #if os(macOS)
                model.audio
                    .refreshMacAudioDevices()
                #endif
            }
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1ShowGeneratorSettings
                    )
            ) { _ in
                showingGeneratorSettings = true
            }
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1ShowAudioLoopRegion
                    )
            ) { _ in
                showingLoopRegion = true
            }
            .onReceive(
                NotificationCenter.default
                    .publisher(
                        for:
                            .fv1SetAnalyzerFFTSize
                    )
            ) { notification in
                if let size =
                    notification.object
                        as? Int {
                    model.audio
                        .setAnalyzerFFTSize(
                            size
                        )

                    model.reportStatus(
                        "Analyzer FFT size set to \(size) for the next audio session."
                    )
                }
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
                    .frame(
                        minWidth: 300,
                        idealWidth: 330,
                        maxWidth: 390
                    )

                centerColumn
                    .frame(
                        minWidth: 650
                    )

                rightColumn
                    .frame(
                        minWidth: 390,
                        idealWidth: 440,
                        maxWidth: 540
                    )
            }
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
                        .frame(
                            width:
                                max(
                                    250,
                                    proxy.size
                                        .width
                                    * 0.24
                                )
                        )

                    centerColumn
                        .frame(
                            maxWidth:
                                .infinity
                        )

                    rightColumn
                        .frame(
                            width:
                                max(
                                    320,
                                    proxy.size
                                        .width
                                    * 0.30
                                )
                        )
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
                    model.audio.isRunning
                        ? "Stop"
                        : "Start",
                    systemImage:
                        model.audio.isRunning
                        ? "stop.fill"
                        : "play.fill"
                )
            }
            .buttonStyle(
                .borderedProminent
            )

            Button {
                requestProgramOpen()
            } label: {
                Label(
                    "Open Program",
                    systemImage: "folder"
                )
            }

            Button {
                guard !spinASMSource
                    .isEmpty else {
                    return
                }

                model.compileAndLoad(
                    source:
                        spinASMSource
                )
            } label: {
                Label(
                    "Compile & Load",
                    systemImage: "hammer"
                )
            }
            .disabled(
                spinASMSource.isEmpty
            )

            Button {
                model.audio
                    .dspEnabled.toggle()
            } label: {
                Label(
                    model.audio.dspEnabled
                        ? "DSP/FX ON"
                        : "DSP/FX BYPASS",
                    systemImage:
                        model.audio.dspEnabled
                        ? "waveform.path.ecg"
                        : "arrow.right"
                )
            }

            #if os(macOS)
            Menu {
                if model.audio.isRecording {
                    Button(
                        "Stop Recording"
                    ) {
                        model.audio
                            .stopRecording()
                    }
                } else {
                    Button(
                        "Record Processed…"
                    ) {
                        beginRecording(
                            .processed
                        )
                    }

                    Button(
                        "Record Raw…"
                    ) {
                        beginRecording(
                            .raw
                        )
                    }

                    Button(
                        "Record Raw + Processed…"
                    ) {
                        beginRecording(
                            .rawAndProcessed
                        )
                    }
                }
            } label: {
                Label(
                    model.audio.isRecording
                        ? "Recording"
                        : "Record",
                    systemImage:
                        model.audio.isRecording
                        ? "record.circle.fill"
                        : "record.circle"
                )
            }
            #endif

            Divider()
                .frame(height: 22)

            Label(
                model.audio.isRunning
                    ? "AUDIO RUNNING"
                    : "AUDIO STOPPED",
                systemImage:
                    model.audio.isRunning
                    ? "waveform.circle.fill"
                    : "waveform.circle"
            )
            .font(.caption.bold())

            Spacer()

            Text(
                model.compileSummary
            )
            .font(.caption)
            .foregroundStyle(
                .secondary
            )
            .lineLimit(1)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
    }

    private var leftColumn: some View {
        ScrollView {
            VStack(
                alignment: .leading,
                spacing: 10
            ) {
                programPanel
                sourcePanel
                virtualControlsPanel
                telemetryPanel
            }
            .padding(8)
        }
    }

    private var programPanel: some View {
        GroupBox("PROGRAM") {
            VStack(
                alignment: .leading,
                spacing: 8
            ) {
                Text(programName)
                    .font(.headline)
                    .lineLimit(2)

                Text(
                    model.compileSummary
                )
                .font(.caption)
                .foregroundStyle(
                    .secondary
                )

                HStack {
                    Button(
                        "Open .spn / .bin"
                    ) {
                        requestProgramOpen()
                    }

                    Button("Reload") {
                        guard !spinASMSource
                            .isEmpty else {
                            return
                        }

                        model.compileAndLoad(
                            source:
                                spinASMSource
                        )
                    }
                    .disabled(
                        spinASMSource
                            .isEmpty
                    )
                }
            }
            .frame(
                maxWidth: .infinity,
                alignment: .leading
            )
        }
    }

    private var sourcePanel: some View {
        GroupBox("INPUT SOURCE") {
            VStack(
                alignment: .leading,
                spacing: 8
            ) {
                Picker(
                    "Mode",
                    selection:
                        $model.audio.sourceMode
                ) {
                    ForEach(
                        AppleAudioSourceMode
                            .allCases
                    ) { mode in
                        Text(mode.rawValue)
                            .tag(mode)
                    }
                }

                switch model.audio.sourceMode {
                case .testGenerator:
                    generatorSourceControls

                case .audioInterface:
                    interfaceSourceControls

                case .audioFileLoop:
                    fileLoopControls
                }

                Divider()

                Text(
                    model.audio
                        .routeDescription
                )
                .font(.caption)
                .foregroundStyle(
                    .secondary
                )

                Button(
                    model.audio.isRunning
                        ? "Stop Audio"
                        : "Start Audio"
                ) {
                    model.audio.toggle()
                }
                .buttonStyle(
                    .borderedProminent
                )

                if model.audio.inputSampleRate
                    > 0 {
                    LabeledContent(
                        model.audio.sourceMode
                            == .testGenerator
                            ? "Generator"
                            : "Input",
                        value:
                            "\(Int(model.audio.inputSampleRate)) Hz"
                    )

                    LabeledContent(
                        "Output",
                        value:
                            "\(Int(model.audio.outputSampleRate)) Hz"
                    )

                    LabeledContent(
                        "FV-1",
                        value: "32768 Hz"
                    )
                }

                if !model.audio.lastError
                    .isEmpty {
                    Text(
                        model.audio.lastError
                    )
                    .font(.caption)
                    .foregroundStyle(.red)
                    .textSelection(
                        .enabled
                    )
                }
            }
            .frame(
                maxWidth: .infinity,
                alignment: .leading
            )
        }
    }

    private var generatorSourceControls:
        some View {
        VStack(
            alignment: .leading,
            spacing: 8
        ) {
            Divider()

            Picker(
                "Signal",
                selection:
                    $model.audio.generatorKind
            ) {
                ForEach(
                    AppleTestSignalKind
                        .allCases
                ) { kind in
                    Text(kind.rawValue)
                        .tag(kind)
                }
            }

            if model.audio.generatorKind
                == .sine
                || model.audio.generatorKind
                    == .sweep {
                LabeledContent(
                    "Start / Tone Frequency"
                ) {
                    HStack(spacing: 4) {
                        TextField(
                            "Hz",
                            value:
                                $model.audio
                                    .generatorFrequency,
                            format: .number
                        )
                        .frame(width: 82)

                        Text("Hz")
                    }
                }
            }

            LabeledContent(
                "Amplitude"
            ) {
                HStack(spacing: 6) {
                    Slider(
                        value:
                            $model.audio
                                .generatorAmplitude,
                        in: 0...1
                    )

                    Text(
                        model.audio
                            .generatorAmplitude,
                        format:
                            .number
                            .precision(
                                .fractionLength(
                                    2
                                )
                            )
                    )
                    .monospacedDigit()
                    .frame(width: 36)
                }
            }

            Button(
                "Generator Settings…"
            ) {
                showingGeneratorSettings =
                    true
            }
        }
    }

    private var interfaceSourceControls:
        some View {
        VStack(
            alignment: .leading,
            spacing: 7
        ) {
            Divider()

            Text(
                "Live input is captured from the selected Apple audio interface and fed through the host→32.768 kHz FV-1 clock bridge."
            )
            .font(.caption2)
            .foregroundStyle(
                .secondary
            )

            #if os(macOS)
            Button(
                "Audio Settings…"
            ) {
                showingAudioSettings =
                    true
            }
            #endif

            #if os(iOS)
            if !model.audio
                .availableInputs
                .isEmpty {
                Text(
                    "AVAILABLE INPUTS"
                )
                .font(.caption.bold())

                ForEach(
                    model.audio
                        .availableInputs,
                    id: \.uid
                ) { port in
                    Button(
                        port.portName
                    ) {
                        model.audio
                            .selectInput(
                                uid:
                                    port.uid
                            )
                    }
                    .buttonStyle(
                        .plain
                    )
                }
            }
            #endif
        }
    }

    private var fileLoopControls:
        some View {
        VStack(
            alignment: .leading,
            spacing: 8
        ) {
            Divider()

            if model.audio.fileLoopName
                .isEmpty {
                Text(
                    "No WAV loaded."
                )
                .font(.caption)
                .foregroundStyle(
                    .secondary
                )

                Button(
                    "Open Audio Loop…"
                ) {
                    requestAudioLoopOpen()
                }
            } else {
                Text(
                    model.audio.fileLoopName
                )
                .font(
                    .caption.monospaced()
                )
                .lineLimit(1)

                HStack {
                    Button {
                        model.audio
                            .playFileLoop()
                    } label: {
                        Image(
                            systemName:
                                "play.fill"
                        )
                    }

                    Button {
                        model.audio
                            .pauseFileLoop()
                    } label: {
                        Image(
                            systemName:
                                "pause.fill"
                        )
                    }

                    Button {
                        model.audio
                            .stopFileLoop()
                    } label: {
                        Image(
                            systemName:
                                "stop.fill"
                        )
                    }

                    Text(
                        model.audio
                            .fileLoopInfo
                            .state.label
                    )
                    .font(
                        .caption.monospaced()
                    )

                    Spacer()

                    Button("Region…") {
                        showingLoopRegion =
                            true
                    }
                }

                let duration =
                    max(
                        0,
                        model.audio
                            .fileLoopInfo
                            .duration
                    )

                if duration > 0 {
                    Slider(
                        value:
                            Binding(
                                get: {
                                    min(
                                        duration,
                                        model.audio
                                            .fileLoopInfo
                                            .position
                                    )
                                },
                                set: {
                                    model.audio
                                        .seekFileLoop(
                                            seconds:
                                                $0
                                        )
                                }
                            ),
                        in: 0...duration
                    )

                    HStack {
                        Text(
                            timeString(
                                model.audio
                                    .fileLoopInfo
                                    .position
                            )
                        )

                        Spacer()

                        Text(
                            timeString(
                                duration
                            )
                        )
                    }
                    .font(
                        .caption2
                            .monospaced()
                    )
                }

                Toggle(
                    "Loop",
                    isOn:
                        Binding(
                            get: {
                                model.audio
                                    .fileLoopInfo
                                    .looping
                            },
                            set: {
                                model.audio
                                    .setFileLooping(
                                        $0
                                    )
                            }
                        )
                )

                LabeledContent(
                    "Crossfade"
                ) {
                    Text(
                        String(
                            format:
                                "%.1f ms",
                            model.audio
                                .fileLoopInfo
                                .crossfadeMS
                        )
                    )
                    .monospacedDigit()
                }
            }
        }
    }

    private var virtualControlsPanel:
        some View {
        GroupBox(
            "VIRTUAL FV-1 CONTROLS"
        ) {
            VStack(spacing: 12) {
                pot(
                    "POT0",
                    value: $model.pot0
                )
                pot(
                    "POT1",
                    value: $model.pot1
                )
                pot(
                    "POT2",
                    value: $model.pot2
                )

                HStack {
                    Button("Reset Chip") {
                        model.reset()
                    }

                    Button(
                        "Refresh State"
                    ) {
                        model
                            .refreshInspection()
                    }
                }
            }
        }
    }

    private var telemetryPanel: some View {
        GroupBox("AUDIO TELEMETRY") {
            Grid(
                alignment: .leading,
                horizontalSpacing: 12,
                verticalSpacing: 5
            ) {
                telemetryRow(
                    "Input frames",
                    "\(model.audio.inputFrames)"
                )
                telemetryRow(
                    "FV-1 frames",
                    "\(model.audio.chipFrames)"
                )
                telemetryRow(
                    "Underflows",
                    "\(model.audio.underflows)"
                )
                telemetryRow(
                    "Overflows",
                    "\(model.audio.overflows)"
                )
                telemetryRow(
                    "FFT",
                    "\(model.audio.analyzerFFTSize)"
                )
            }
            .font(.caption)
            .monospacedDigit()
        }
    }

    private var centerColumn: some View {
        VStack(spacing: 8) {
            TabView {
                FV1ScopeAnalyzerView(
                    raw:
                        model.audio
                            .rawAnalysis,
                    processed:
                        model.audio
                            .processedAnalysis
                )
                .tabItem {
                    Label(
                        "OSCILLOSCOPE",
                        systemImage:
                            "waveform"
                    )
                }

                FV1SpectrumAnalyzerView(
                    raw:
                        model.audio
                            .rawAnalysis,
                    processed:
                        model.audio
                            .processedAnalysis
                )
                .tabItem {
                    Label(
                        "SPECTRUM",
                        systemImage:
                            "chart.bar.xaxis"
                    )
                }

                FV1SpectrogramView(
                    raw:
                        model.audio
                            .rawAnalysis,
                    processed:
                        model.audio
                            .processedAnalysis
                )
                .tabItem {
                    Label(
                        "SPECTROGRAM",
                        systemImage:
                            "water.waves"
                    )
                }

                FV1LevelsAnalyzerView(
                    raw:
                        model.audio
                            .rawAnalysis,
                    processed:
                        model.audio
                            .processedAnalysis
                )
                .tabItem {
                    Label(
                        "LEVELS",
                        systemImage:
                            "gauge.with.dots.needle.50percent"
                    )
                }

                ValidationView()
                    .tabItem {
                        Label(
                            "VALIDATION",
                            systemImage:
                                "checkmark.seal"
                        )
                    }
            }

            HStack(
                alignment: .top,
                spacing: 8
            ) {
                DelayRAMView(
                    model: model
                )

                resourcePanel
                dspStatusPanel
            }
            .frame(
                minHeight: 180,
                maxHeight: 240
            )
        }
        .padding(8)
    }

    private var resourcePanel: some View {
        GroupBox(
            "VIRTUAL DSP RESOURCE USAGE"
        ) {
            VStack(
                alignment: .leading,
                spacing: 7
            ) {
                if let resource =
                    model.resources {
                    resourceProgress(
                        "Program",
                        value:
                            Double(
                                resource
                                    .usedInstructions
                            ),
                        total: 128,
                        detail:
                            "\(resource.usedInstructions) / 128"
                    )

                    resourceProgress(
                        "Delay RAM",
                        value:
                            Double(
                                resource
                                    .highestStaticDelayAddress
                            ),
                        total: 32768,
                        detail:
                            "\(resource.highestStaticDelayAddress) / 32768"
                    )

                    resourceProgress(
                        "Registers",
                        value:
                            Double(
                                resource
                                    .generalRegistersUsed
                            ),
                        total: 32,
                        detail:
                            "\(resource.generalRegistersUsed) / 32"
                    )

                    Text(
                        "Reads \(resource.staticDelayReads) · Writes \(resource.staticDelayWrites) · Worst path \(resource.worstCasePath)"
                    )
                    .font(.caption2)
                    .foregroundStyle(
                        .secondary
                    )
                } else {
                    Text(
                        "Load a program to analyze resources."
                    )
                    .font(.caption)
                    .foregroundStyle(
                        .secondary
                    )
                }
            }
        }
        .frame(
            maxWidth: .infinity
        )
    }

    private var dspStatusPanel: some View {
        GroupBox("DSP STATUS") {
            VStack(
                alignment: .leading,
                spacing: 7
            ) {
                Text(
                    model.audio.isRunning
                        ? "Running"
                        : "Stopped"
                )
                .font(.headline)

                Text(
                    "Host/FV-1 runtime ready."
                )
                .font(.caption)

                Divider()

                Text(
                    "FV-1 clock: 32768 Hz"
                )
                .font(
                    .caption.monospaced()
                )

                Text(
                    "Analyzer FFT: \(model.audio.analyzerFFTSize)"
                )
                .font(
                    .caption.monospaced()
                )

                Text(
                    model.audio.dspEnabled
                        ? "Path: DSP/FX ON"
                        : "Path: DSP/FX BYPASS"
                )
                .font(
                    .caption.monospaced()
                )

                if model.audio.isRecording {
                    Text(
                        "REC raw \(model.audio.recorderStats.rawFramesWritten) · processed \(model.audio.recorderStats.processedFramesWritten)"
                    )
                    .font(
                        .caption2
                            .monospaced()
                    )
                }

                Text(
                    "Underflows: \(model.audio.underflows)"
                )
                .font(
                    .caption.monospaced()
                )

                Text(
                    "Overflows: \(model.audio.overflows)"
                )
                .font(
                    .caption.monospaced()
                )
            }
            .frame(
                maxWidth: .infinity,
                alignment: .leading
            )
        }
        .frame(
            maxWidth: .infinity
        )
    }

    private var rightColumn: some View {
        VStack(spacing: 8) {
            GroupBox("CONSOLE / LOG") {
                ScrollView {
                    Text(model.console)
                        .font(
                            .system(
                                .caption,
                                design:
                                    .monospaced
                            )
                        )
                        .textSelection(
                            .enabled
                        )
                        .frame(
                            maxWidth:
                                .infinity,
                            alignment:
                                .leading
                        )
                }
            }
            .frame(minHeight: 190)

            GroupBox(
                "OFFLINE FV-1 CHIP INSPECTOR"
            ) {
                InspectionView(
                    model: model
                )
            }
            .frame(
                maxHeight: .infinity
            )
        }
        .padding(8)
    }

    private var statusFooter: some View {
        HStack {
            Text(
                "FV-1 Lab — native Apple frontend"
            )

            Spacer()

            Text(
                model.audio.isRunning
                    ? "RUNNING"
                    : "READY"
            )
            .monospaced()

            Spacer()

            Text(
                "© 2026 Roth Amplification LTD"
            )
        }
        .font(.caption2)
        .foregroundStyle(.secondary)
        .padding(.horizontal, 10)
        .padding(.vertical, 5)
    }

    private func pot(
        _ title: String,
        value: Binding<Double>
    ) -> some View {
        VStack(
            alignment: .leading,
            spacing: 4
        ) {
            HStack {
                Text(title)
                Spacer()
                Text(
                    value.wrappedValue,
                    format:
                        .number.precision(
                            .fractionLength(
                                3
                            )
                        )
                )
                .monospacedDigit()
            }

            Slider(
                value: value,
                in: 0...1
            )
        }
    }

    @ViewBuilder
    private func telemetryRow(
        _ label: String,
        _ value: String
    ) -> some View {
        GridRow {
            Text(label)
                .foregroundStyle(
                    .secondary
                )
            Text(value)
        }
    }

    private func resourceProgress(
        _ title: String,
        value: Double,
        total: Double,
        detail: String
    ) -> some View {
        VStack(
            alignment: .leading,
            spacing: 3
        ) {
            HStack {
                Text(title)
                Spacer()
                Text(detail)
                    .monospacedDigit()
            }
            .font(.caption)

            ProgressView(
                value: value,
                total: total
            )
        }
    }

    private func requestProgramOpen() {
        #if os(macOS)
        let panel = NSOpenPanel()
        panel.title =
            "Open FV-1 Program"
        panel.prompt = "Open"
        panel.allowsMultipleSelection =
            false
        panel.canChooseDirectories =
            false
        panel.canChooseFiles = true
        panel.allowedContentTypes = [
            spinASMType,
            .plainText,
            .data
        ]

        guard panel.runModal()
            == .OK,
              let url = panel.url else {
            return
        }

        importProgram(
            .success([url])
        )
        #else
        importingProgram = true
        #endif
    }

    private func requestAudioLoopOpen() {
        #if os(macOS)
        let panel = NSOpenPanel()
        panel.title =
            "Open Audio Loop"
        panel.prompt = "Open"
        panel.allowsMultipleSelection =
            false
        panel.canChooseDirectories =
            false
        panel.canChooseFiles = true
        panel.allowedContentTypes = [
            wavType
        ]

        guard panel.runModal()
            == .OK,
              let url = panel.url else {
            return
        }

        importAudioLoop(
            .success([url])
        )
        #else
        importingAudioLoop = true
        #endif
    }

    private func importProgram(
        _ result:
            Result<[URL], Error>
    ) {
        do {
            guard let url =
                try result.get().first else {
                return
            }

            let accessing =
                url.startAccessingSecurityScopedResource()
            defer {
                if accessing {
                    url.stopAccessingSecurityScopedResource()
                }
            }

            let data =
                try Data(
                    contentsOf: url
                )

            let extensionName =
                url.pathExtension
                    .lowercased()

            let binaryExtension =
                [
                    "bin",
                    "rom",
                    "fv1bin"
                ]
                .contains(
                    extensionName
                )

            if binaryExtension {
                guard data.count
                    == Int(
                        FV1_SDK_PROGRAM_BYTES
                    ) else {
                    throw FV1EngineError
                        .invalidProgramSize(
                            data.count
                        )
                }

                spinASMSource = ""
                programName =
                    url.lastPathComponent
                model.loadRawProgram(
                    data
                )
                return
            }

            let containsNUL =
                data.contains(0)

            if !containsNUL,
               let source =
                String(
                    data: data,
                    encoding: .utf8
                ) {
                spinASMSource = source
                programName =
                    url.lastPathComponent

                model.compileAndLoad(
                    source: source
                )
                return
            }

            if data.count
                == Int(
                    FV1_SDK_PROGRAM_BYTES
                ) {
                spinASMSource = ""
                programName =
                    url.lastPathComponent
                model.loadRawProgram(
                    data
                )
                return
            }

            throw FV1TestbenchError
                .operation(
                    "Unsupported FV-1 program file. Expected UTF-8 SpinASM text or a 512-byte binary image."
                )
        } catch {
            model.reportExternalError(
                error.localizedDescription
            )
        }
    }

    private func importAudioLoop(
        _ result:
            Result<[URL], Error>
    ) {
        do {
            guard let url =
                try result.get().first else {
                return
            }

            let accessing =
                url.startAccessingSecurityScopedResource()
            defer {
                if accessing {
                    url.stopAccessingSecurityScopedResource()
                }
            }

            try model.audio.loadFileLoop(
                url: url
            )

            model.reportStatus(
                "Audio loop loaded: \(url.lastPathComponent)"
            )
        } catch {
            model.reportExternalError(
                error.localizedDescription
            )
        }
    }

    #if os(macOS)
    private func pasteSpinASMFromClipboard() {
        guard let source =
            NSPasteboard.general
                .string(
                    forType: .string
                ),
              !source
                .trimmingCharacters(
                    in:
                        .whitespacesAndNewlines
                )
                .isEmpty else {
            model.reportExternalError(
                "Clipboard does not contain SpinASM text."
            )
            return
        }

        spinASMSource = source
        programName =
            "Pasted SpinASM"

        model.compileAndLoad(
            source: source
        )
    }

    private func beginRecording(
        _ mode: AppleRecordMode
    ) {
        let suffix: String

        switch mode {
        case .processed:
            suffix = "processed"
        case .raw:
            suffix = "raw"
        case .rawAndProcessed:
            suffix =
                "raw-processed"
        }

        guard let url =
            MacExportPresenter
                .chooseRecordingURL(
                    suggestedName:
                        "fv1-\(suffix).wav"
                ) else {
            return
        }

        do {
            try model.audio
                .startRecording(
                    to: url,
                    mode: mode
                )
        } catch {
            model.reportExternalError(
                error.localizedDescription
            )
        }
    }
    #endif

    private func timeString(
        _ seconds: Double
    ) -> String {
        let clamped =
            max(0, seconds)
        let minutes =
            Int(clamped) / 60
        let remaining =
            clamped
                - Double(
                    minutes * 60
                )

        return String(
            format:
                "%d:%05.2f",
            minutes,
            remaining
        )
    }
}
