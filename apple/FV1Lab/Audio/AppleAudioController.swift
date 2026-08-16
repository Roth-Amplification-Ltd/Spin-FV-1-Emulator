import AVFAudio
import AudioToolbox
import Combine
import Foundation

enum AppleAudioSourceMode: String, CaseIterable, Identifiable {
    case testGenerator = "Test Generator"
    case audioInterface = "Audio Interface"
    case audioFileLoop = "Audio File Loop"

    var id: String { rawValue }
}

enum AppleTestSignalKind: String, CaseIterable, Identifiable {
    case sine = "Sine"
    case sweep = "Sweep"
    case whiteNoise = "White Noise"
    case pinkNoise = "Pink Noise"
    case impulse = "Impulse"

    var id: String { rawValue }

    var bridgeValue: UInt32 {
        switch self {
        case .sine: return 0
        case .sweep: return 1
        case .whiteNoise: return 2
        case .pinkNoise: return 3
        case .impulse: return 4
        }
    }
}

@MainActor
final class AppleAudioController: ObservableObject {
    @Published private(set) var isRunning = false
    @Published private(set) var routeDescription = "Stopped"
    @Published private(set) var inputSampleRate = 0.0
    @Published private(set) var outputSampleRate = 0.0
    @Published private(set) var inputFrames: UInt64 = 0
    @Published private(set) var chipFrames: UInt64 = 0
    @Published private(set) var underflows: UInt64 = 0
    @Published private(set) var overflows: UInt64 = 0
    @Published private(set) var lastError = ""

    @Published private(set) var scopeLeft: [Float] = []
    @Published private(set) var scopeRight: [Float] = []
    @Published private(set) var rawAnalysis = AppleAnalysisSnapshot.empty
    @Published private(set) var processedAnalysis = AppleAnalysisSnapshot.empty

    @Published private(set) var isRecording = false
    @Published private(set) var recorderStats = AppleRecorderStats()

    @Published private(set) var fileLoopInfo = AppleFileLoopInfo.empty
    @Published private(set) var fileLoopName = ""
    @Published var fileLoopBegin = 0.0
    @Published var fileLoopEnd = 0.0
    @Published var fileLoopCrossfadeMS = 5.0

    @Published var analyzerFFTSize = 4096 {
        didSet {
            guard [1024, 2048, 4096, 8192].contains(analyzerFFTSize) else {
                return
            }
            UserDefaults.standard.set(analyzerFFTSize, forKey: "analysis/fftSize")
            realtime.setAnalyzerFFTSize(analyzerFFTSize)
        }
    }

    @Published var dspEnabled = true {
        didSet {
            UserDefaults.standard.set(dspEnabled, forKey: "audio/dspEnabled")
            realtime.setDSPEnabled(dspEnabled)
            updateRouteDescription()
        }
    }

    @Published var sourceMode: AppleAudioSourceMode = .testGenerator {
        didSet { sourceModeDidChange() }
    }

    @Published var generatorKind: AppleTestSignalKind = .sine {
        didSet { generatorSettingChanged() }
    }
    @Published var generatorFrequency = 440.0 {
        didSet { generatorSettingChanged() }
    }
    @Published var generatorAmplitude = 0.25 {
        didSet { generatorSettingChanged() }
    }
    @Published var generatorSweepEnd = 12_000.0 {
        didSet { generatorSettingChanged() }
    }
    @Published var generatorSweepSeconds = 5.0 {
        didSet { generatorSettingChanged() }
    }
    @Published var generatorImpulsePeriod = 1.0 {
        didSet { generatorSettingChanged() }
    }

    #if os(iOS)
    @Published private(set) var availableInputs: [AVAudioSessionPortDescription] = []
    #endif

    #if os(macOS)
    let macAudioDevices = MacAudioDeviceManager()
    #endif

    private let engine = AVAudioEngine()
    private let realtime: FV1RealtimeBridge
    private let fileLoop: FV1FileLoopBridge

    private var sourceNode: AVAudioSourceNode?
    private var telemetryTimer: Timer?
    private var currentProgram: Data?
    private var fileLoopURL: URL?
    private var pots: [Float] = [0, 0, 0]
    private var inputTapInstalled = false
    private var notificationTokens: [NSObjectProtocol] = []
    private var isRecoveringRoute = false

    init() {
        do {
            realtime = try FV1RealtimeBridge()
            fileLoop = try FV1FileLoopBridge()
        } catch {
            fatalError("FV-1 Apple audio/testbench bridge could not be created: \(error)")
        }

        let defaults = UserDefaults.standard

        let savedFFT = defaults.integer(forKey: "analysis/fftSize")
        if [1024, 2048, 4096, 8192].contains(savedFFT) {
            analyzerFFTSize = savedFFT
        }

        if defaults.object(forKey: "audio/dspEnabled") != nil {
            dspEnabled = defaults.bool(forKey: "audio/dspEnabled")
        }

        if let rawKind = defaults.string(forKey: "generator/kind"),
           let kind = AppleTestSignalKind(rawValue: rawKind) {
            generatorKind = kind
        }

        generatorFrequency = Self.savedDouble("generator/frequency", fallback: 440)
        generatorAmplitude = Self.savedDouble("generator/amplitude", fallback: 0.25)
        generatorSweepEnd = Self.savedDouble("generator/sweepEndHz", fallback: 12_000)
        generatorSweepSeconds = Self.savedDouble("generator/sweepSeconds", fallback: 5)
        generatorImpulsePeriod = Self.savedDouble("generator/impulsePeriodSeconds", fallback: 1)

        syncGeneratorSettings()
        realtime.setDSPEnabled(dspEnabled)
        realtime.setAnalyzerFFTSize(analyzerFFTSize)

        #if os(iOS)
        refreshAvailableInputs()
        #endif

        registerConfigurationObservers()
        updateRouteDescription()
    }

    func setProgram(_ program: Data) throws {
        let resume = isRunning
        if resume { stop() }
        try realtime.load(program: program)
        currentProgram = program
        if resume { try start() }
    }

    func setPots(_ values: [Float]) {
        guard values.count == 3 else { return }
        pots = values
        realtime.setPots(values)
    }

    func resetChip() throws {
        let resume = isRunning
        if resume { stop() }

        try realtime.reset()
        if let currentProgram {
            try realtime.load(program: currentProgram)
        }
        realtime.setPots(pots)

        if resume { try start() }
    }

    func toggle() {
        do {
            if isRunning {
                stop()
            } else {
                try start()
            }
        } catch {
            lastError = error.localizedDescription
            stop()
        }
    }

    func start() throws {
        guard !isRunning else { return }

        try configurePlatformAudioSession()

        engine.stop()
        engine.reset()

        #if os(macOS)
        try macAudioDevices.apply(
            to: engine,
            needsInput: sourceMode == .audioInterface
        )
        #endif

        let outputHardware = engine.outputNode.inputFormat(forBus: 0)

        guard outputHardware.sampleRate > 0,
              outputHardware.channelCount > 0 else {
            throw FV1EngineError.sdk(
                fv1_sdk_result(FV1_SDK_ERROR_BAD_STATE),
                "No usable Apple audio output route"
            )
        }

        guard let renderFormat = AVAudioFormat(
            commonFormat: .pcmFormatFloat32,
            sampleRate: outputHardware.sampleRate,
            channels: 2,
            interleaved: false
        ) else {
            throw FV1EngineError.sdk(
                fv1_sdk_result(FV1_SDK_ERROR_UNSUPPORTED),
                "Unable to create canonical Float32 Apple output format"
            )
        }

        var inputNode: AVAudioInputNode?
        var inputFormat: AVAudioFormat?
        var bridgeInputRate = renderFormat.sampleRate

        if sourceMode == .audioInterface {
            let liveInput = engine.inputNode
            let inputHardware = liveInput.outputFormat(forBus: 0)

            guard inputHardware.sampleRate > 0,
                  inputHardware.channelCount > 0 else {
                throw FV1EngineError.sdk(
                    fv1_sdk_result(FV1_SDK_ERROR_BAD_STATE),
                    "No usable Apple audio input route"
                )
            }

            let channels = AVAudioChannelCount(min(2, Int(inputHardware.channelCount)))
            guard let canonicalInput = AVAudioFormat(
                commonFormat: .pcmFormatFloat32,
                sampleRate: inputHardware.sampleRate,
                channels: channels,
                interleaved: false
            ) else {
                throw FV1EngineError.sdk(
                    fv1_sdk_result(FV1_SDK_ERROR_UNSUPPORTED),
                    "Unable to create canonical Float32 Apple input format"
                )
            }

            inputNode = liveInput
            inputFormat = canonicalInput
            bridgeInputRate = canonicalInput.sampleRate
        } else if sourceMode == .audioFileLoop {
            guard fileLoopURL != nil else {
                throw FV1TestbenchError.operation(
                    "Load an audio loop before starting the file-loop source."
                )
            }

            try fileLoop.prepare(
                sampleRate: renderFormat.sampleRate,
                maxFrames: 4096
            )
            bridgeInputRate = renderFormat.sampleRate
        }

        realtime.setAnalyzerFFTSize(analyzerFFTSize)
        try realtime.configureAndPrime(
            inputRate: bridgeInputRate,
            outputRate: renderFormat.sampleRate,
            primeFrames: 256
        )
        realtime.setPots(pots)
        realtime.setDSPEnabled(dspEnabled)
        syncGeneratorSettings()

        let bridge = realtime
        let loopSource = fileLoop
        let useTestGenerator = sourceMode == .testGenerator
        let useFileLoop = sourceMode == .audioFileLoop

        let renderBlock: AVAudioSourceNodeRenderBlock = {
            @Sendable [bridge, loopSource] _, _, frameCount, outputData in

            let buffers = UnsafeMutableAudioBufferListPointer(outputData)
            guard buffers.count >= 2,
                  let leftData = buffers[0].mData,
                  let rightData = buffers[1].mData else {
                return kAudio_ParamError
            }

            let count = Int(frameCount)
            let left = leftData.bindMemory(to: Float.self, capacity: count)
            let right = rightData.bindMemory(to: Float.self, capacity: count)

            if useTestGenerator {
                bridge.processTestGenerator(frames: count)
            } else if useFileLoop {
                loopSource.render(left: left, right: right, frames: count)
                bridge.process(
                    left: UnsafePointer(left),
                    right: UnsafePointer(right),
                    frames: count
                )
            }

            bridge.render(left: left, right: right, frames: count)
            return noErr
        }

        let source = AVAudioSourceNode(format: renderFormat, renderBlock: renderBlock)
        sourceNode = source
        engine.attach(source)
        engine.connect(source, to: engine.mainMixerNode, format: renderFormat)

        if let inputNode, let inputFormat {
            let inputChannelCount = Int(inputFormat.channelCount)
            let inputTap: AVAudioNodeTapBlock = {
                @Sendable [bridge, inputChannelCount] buffer, _ in

                guard let channels = buffer.floatChannelData,
                      buffer.frameLength > 0 else {
                    return
                }

                let left = UnsafePointer(channels[0])
                let right = UnsafePointer(channels[min(1, inputChannelCount - 1)])

                bridge.process(
                    left: left,
                    right: right,
                    frames: Int(buffer.frameLength)
                )
            }

            inputNode.installTap(
                onBus: 0,
                bufferSize: 512,
                format: inputFormat,
                block: inputTap
            )
            inputTapInstalled = true
        }

        engine.prepare()

        do {
            try engine.start()
        } catch {
            if inputTapInstalled {
                inputNode?.removeTap(onBus: 0)
                inputTapInstalled = false
            }
            engine.detach(source)
            sourceNode = nil
            throw error
        }

        inputSampleRate = bridgeInputRate
        outputSampleRate = renderFormat.sampleRate
        isRunning = true
        lastError = ""

        updateRouteDescription()
        refreshFileLoopInfo()
        startTelemetry()
    }

    func stop() {
        if isRecording {
            stopRecording()
        }

        telemetryTimer?.invalidate()
        telemetryTimer = nil

        if inputTapInstalled {
            engine.inputNode.removeTap(onBus: 0)
            inputTapInstalled = false
        }

        engine.stop()

        if let sourceNode, engine.attachedNodes.contains(sourceNode) {
            engine.detach(sourceNode)
        }
        sourceNode = nil
        isRunning = false

        if sourceMode == .audioFileLoop {
            fileLoop.stop()
            refreshFileLoopInfo()
        }

        routeDescription = "Stopped"

        #if os(iOS)
        try? AVAudioSession.sharedInstance().setActive(
            false,
            options: .notifyOthersOnDeactivation
        )
        #endif
    }

    func loadFileLoop(url: URL) throws {
        if isRunning { stop() }

        try fileLoop.load(url: url)
        fileLoopURL = url
        fileLoopName = url.lastPathComponent

        let info = fileLoop.info()
        fileLoopInfo = info
        fileLoopBegin = info.loopBegin
        fileLoopEnd = info.loopEnd

        let savedCrossfade = UserDefaults.standard.double(forKey: "fileLoop/crossfadeMs")
        fileLoopCrossfadeMS = savedCrossfade > 0 ? savedCrossfade : 5.0
        fileLoop.setCrossfade(milliseconds: fileLoopCrossfadeMS)

        sourceMode = .audioFileLoop
        updateRouteDescription()
    }

    func playFileLoop() {
        fileLoop.play()
        refreshFileLoopInfo()
    }

    func pauseFileLoop() {
        fileLoop.pause()
        refreshFileLoopInfo()
    }

    func stopFileLoop() {
        fileLoop.stop()
        refreshFileLoopInfo()
    }

    func seekFileLoop(seconds: Double) {
        do {
            try fileLoop.seek(seconds: seconds)
            refreshFileLoopInfo()
        } catch {
            lastError = error.localizedDescription
        }
    }

    func setFileLooping(_ enabled: Bool) {
        fileLoop.setLooping(enabled)
        refreshFileLoopInfo()
    }

    func applyFileLoopRegion() {
        guard fileLoopInfo.duration > 0 else { return }

        let begin = max(0, min(fileLoopBegin, fileLoopInfo.duration))
        let end = max(
            begin + 0.001,
            min(fileLoopEnd, fileLoopInfo.duration)
        )

        do {
            try fileLoop.setLoopRegion(begin: begin, end: end)
            let crossfade = max(0, min(500, fileLoopCrossfadeMS))
            fileLoop.setCrossfade(milliseconds: crossfade)
            UserDefaults.standard.set(crossfade, forKey: "fileLoop/crossfadeMs")

            fileLoopBegin = begin
            fileLoopEnd = end
            fileLoopCrossfadeMS = crossfade
            refreshFileLoopInfo()
        } catch {
            lastError = error.localizedDescription
        }
    }

    func setAnalyzerFFTSize(_ size: Int) {
        guard [1024, 2048, 4096, 8192].contains(size) else { return }
        analyzerFFTSize = size
    }

    func startRecording(to url: URL, mode: AppleRecordMode) throws {
        guard isRunning else {
            throw AppleRecordingError.recorder(
                "Start an audio session before recording."
            )
        }

        let selectedRate: Double
        switch mode {
        case .processed:
            selectedRate = outputSampleRate
        case .raw:
            selectedRate = inputSampleRate
        case .rawAndProcessed:
            guard abs(inputSampleRate - outputSampleRate) < 0.5 else {
                throw AppleRecordingError.invalidSampleRates
            }
            selectedRate = outputSampleRate
        }

        guard selectedRate > 0 else {
            throw AppleRecordingError.recorder(
                "The active Apple audio route has no valid sample rate."
            )
        }

        try realtime.startRecording(
            url: url,
            sampleRate: UInt32(selectedRate.rounded()),
            mode: mode
        )

        isRecording = true
        recorderStats = realtime.recorderStats()
    }

    func stopRecording() {
        realtime.stopRecording()
        recorderStats = realtime.recorderStats()
        isRecording = false
    }

    #if os(macOS)
    func refreshMacAudioDevices() {
        macAudioDevices.refresh()
    }
    #endif

    #if os(iOS)
    func refreshAvailableInputs() {
        availableInputs = AVAudioSession.sharedInstance().availableInputs ?? []
    }

    func selectInput(uid: String) {
        do {
            let session = AVAudioSession.sharedInstance()
            guard let port = session.availableInputs?.first(where: { $0.uid == uid }) else {
                return
            }

            sourceMode = .audioInterface
            try session.setPreferredInput(port)
            refreshAvailableInputs()

            if isRunning {
                stop()
                try start()
            }
        } catch {
            lastError = error.localizedDescription
        }
    }
    #endif

    private func registerConfigurationObservers() {
        let center = NotificationCenter.default

        notificationTokens.append(
            center.addObserver(
                forName: .AVAudioEngineConfigurationChange,
                object: engine,
                queue: .main
            ) { [weak self] _ in
                Task { @MainActor in
                    self?.recoverAfterRouteChange()
                }
            }
        )

        #if os(iOS)
        notificationTokens.append(
            center.addObserver(
                forName: AVAudioSession.routeChangeNotification,
                object: nil,
                queue: .main
            ) { [weak self] _ in
                Task { @MainActor in
                    self?.refreshAvailableInputs()
                    self?.recoverAfterRouteChange()
                }
            }
        )
        #endif
    }

    private func recoverAfterRouteChange() {
        guard isRunning, !isRecoveringRoute else {
            updateRouteDescription()
            return
        }

        isRecoveringRoute = true
        defer { isRecoveringRoute = false }

        stop()
        do {
            try start()
        } catch {
            lastError = "Audio route recovery failed: \(error.localizedDescription)"
            stop()
        }
    }

    private func startTelemetry() {
        telemetryTimer?.invalidate()
        telemetryTimer = Timer.scheduledTimer(
            withTimeInterval: 1.0 / 20.0,
            repeats: true
        ) { [weak self] _ in
            Task { @MainActor in
                self?.refreshTelemetry()
            }
        }
    }

    private func refreshTelemetry() {
        let stats = realtime.stats()
        inputFrames = stats.input_frames
        chipFrames = stats.chip_frames
        underflows = stats.output_underflows
        overflows = stats.output_overflows

        rawAnalysis = realtime.analysis(stream: .raw)
        processedAnalysis = realtime.analysis(stream: .processed)

        scopeLeft = processedAnalysis.scopeLeft
        scopeRight = processedAnalysis.scopeRight

        isRecording = realtime.isRecording
        recorderStats = realtime.recorderStats()

        if fileLoopURL != nil {
            refreshFileLoopInfo()
        }

        updateRouteDescription()
    }

    private func refreshFileLoopInfo() {
        fileLoopInfo = fileLoop.info()
    }

    private func configurePlatformAudioSession() throws {
        #if os(iOS)
        let session = AVAudioSession.sharedInstance()

        if sourceMode == .audioInterface {
            try session.setCategory(
                .playAndRecord,
                mode: .measurement,
                options: [.allowBluetooth, .allowAirPlay]
            )
        } else {
            try session.setCategory(
                .playback,
                mode: .measurement,
                options: [.allowAirPlay]
            )
        }

        try session.setPreferredSampleRate(48_000)
        try session.setPreferredIOBufferDuration(0.005)
        try session.setActive(true)

        if sourceMode == .audioInterface {
            refreshAvailableInputs()
        }
        #endif
    }

    private func updateRouteDescription() {
        let path = dspEnabled ? "DSP/FX ON" : "DSP/FX BYPASS"

        #if os(iOS)
        let route = AVAudioSession.sharedInstance().currentRoute
        let output = route.outputs.first?.portName ?? "No output"

        switch sourceMode {
        case .testGenerator:
            routeDescription = "Test Generator → \(path) → \(output)"
        case .audioInterface:
            let input = route.inputs.first?.portName ?? "No input"
            routeDescription = "\(input) → \(path) → \(output)"
        case .audioFileLoop:
            let file = fileLoopName.isEmpty ? "Audio File Loop" : fileLoopName
            routeDescription = "\(file) → \(path) → \(output)"
        }
        #else
        let selectedDevice =
            macAudioDevices
                .selectedDevice()?
                .displayName

        let output =
            selectedDevice
            ?? "system default output"

        switch sourceMode {
        case .testGenerator:
            routeDescription =
                "Test Generator → \(path) → \(output)"

        case .audioInterface:
            let input =
                selectedDevice
                ?? "system default input"

            routeDescription =
                "\(input) → \(path) → \(output)"

        case .audioFileLoop:
            let file =
                fileLoopName.isEmpty
                ? "Audio File Loop"
                : fileLoopName

            routeDescription =
                "\(file) → \(path) → \(output)"
        }
        #endif
    }

    private func syncGeneratorSettings() {
        realtime.configureTestGenerator(
            kind: generatorKind.bridgeValue,
            frequency: max(0, generatorFrequency),
            amplitude: min(1, max(0, generatorAmplitude)),
            sweepEnd: max(1, generatorSweepEnd),
            sweepSeconds: max(0.001, generatorSweepSeconds),
            impulsePeriod: max(0.001, generatorImpulsePeriod)
        )
    }

    private func generatorSettingChanged() {
        let defaults = UserDefaults.standard
        defaults.set(generatorKind.rawValue, forKey: "generator/kind")
        defaults.set(generatorFrequency, forKey: "generator/frequency")
        defaults.set(generatorAmplitude, forKey: "generator/amplitude")
        defaults.set(generatorSweepEnd, forKey: "generator/sweepEndHz")
        defaults.set(generatorSweepSeconds, forKey: "generator/sweepSeconds")
        defaults.set(generatorImpulsePeriod, forKey: "generator/impulsePeriodSeconds")
        syncGeneratorSettings()
    }

    private func sourceModeDidChange() {
        updateRouteDescription()

        guard isRunning else { return }

        stop()
        do {
            try start()
        } catch {
            lastError = "Audio source change failed: \(error.localizedDescription)"
            stop()
        }
    }

    private static func savedDouble(_ key: String, fallback: Double) -> Double {
        let defaults = UserDefaults.standard
        guard defaults.object(forKey: key) != nil else {
            return fallback
        }
        return defaults.double(forKey: key)
    }
}
