import AVFAudio
import AudioToolbox
import Combine
import Foundation

enum AppleAudioSourceMode: String, CaseIterable, Identifiable {
    case testGenerator = "Test Generator"
    case audioInterface = "Audio Interface"

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

    @Published var sourceMode: AppleAudioSourceMode = .testGenerator {
        didSet { sourceModeDidChange() }
    }
    @Published var generatorKind: AppleTestSignalKind = .sine {
        didSet { syncGeneratorSettings() }
    }
    @Published var generatorFrequency = 440.0 {
        didSet { syncGeneratorSettings() }
    }
    @Published var generatorAmplitude = 0.25 {
        didSet { syncGeneratorSettings() }
    }
    @Published var generatorSweepEnd = 12_000.0 {
        didSet { syncGeneratorSettings() }
    }
    @Published var generatorSweepSeconds = 5.0 {
        didSet { syncGeneratorSettings() }
    }
    @Published var generatorImpulsePeriod = 1.0 {
        didSet { syncGeneratorSettings() }
    }

    #if os(iOS)
    @Published private(set) var availableInputs: [AVAudioSessionPortDescription] = []
    #endif

    private let engine = AVAudioEngine()
    private let realtime: FV1RealtimeBridge
    private var sourceNode: AVAudioSourceNode?
    private var telemetryTimer: Timer?
    private var currentProgram: Data?
    private var pots: [Float] = [0, 0, 0]
    private var inputTapInstalled = false
    private var notificationTokens: [NSObjectProtocol] = []
    private var isRecoveringRoute = false

    init() {
        do {
            realtime = try FV1RealtimeBridge()
        } catch {
            fatalError("FV-1 realtime bridge could not be created: \(error)")
        }
        syncGeneratorSettings()
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
        if let currentProgram { try realtime.load(program: currentProgram) }
        realtime.setPots(pots)
        if resume { try start() }
    }

    func toggle() {
        do {
            if isRunning { stop() } else { try start() }
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

        let outputNode = engine.outputNode
        let outputHardware = outputNode.inputFormat(forBus: 0)

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

            let inputChannels = AVAudioChannelCount(
                min(2, Int(inputHardware.channelCount))
            )

            guard let canonicalInput = AVAudioFormat(
                commonFormat: .pcmFormatFloat32,
                sampleRate: inputHardware.sampleRate,
                channels: inputChannels,
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
        }

        try realtime.configureAndPrime(
            inputRate: bridgeInputRate,
            outputRate: renderFormat.sampleRate,
            primeFrames: 256
        )
        realtime.setPots(pots)
        syncGeneratorSettings()

        let bridge = realtime
        let useTestGenerator = sourceMode == .testGenerator

        let renderBlock: AVAudioSourceNodeRenderBlock = {
            @Sendable [bridge] _, _, frameCount, outputData in

            let buffers = UnsafeMutableAudioBufferListPointer(outputData)
            guard buffers.count >= 2,
                  let leftData = buffers[0].mData,
                  let rightData = buffers[1].mData else {
                return kAudio_ParamError
            }

            let left = leftData.bindMemory(
                to: Float.self,
                capacity: Int(frameCount)
            )
            let right = rightData.bindMemory(
                to: Float.self,
                capacity: Int(frameCount)
            )

            if useTestGenerator {
                bridge.processTestGenerator(frames: Int(frameCount))
            }

            bridge.render(
                left: left,
                right: right,
                frames: Int(frameCount)
            )
            return noErr
        }

        let source = AVAudioSourceNode(
            format: renderFormat,
            renderBlock: renderBlock
        )
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
                let rightIndex = min(1, inputChannelCount - 1)
                let right = UnsafePointer(channels[rightIndex])

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
        startTelemetry()
    }

    func stop() {
        telemetryTimer?.invalidate()
        telemetryTimer = nil
        if inputTapInstalled {
            engine.inputNode.removeTap(onBus: 0)
            inputTapInstalled = false
        }
        engine.stop()
        if let sourceNode, engine.attachedNodes.contains(sourceNode) { engine.detach(sourceNode) }
        sourceNode = nil
        isRunning = false
        routeDescription = "Stopped"
        #if os(iOS)
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        #endif
    }

    #if os(iOS)
    func refreshAvailableInputs() {
        availableInputs = AVAudioSession.sharedInstance().availableInputs ?? []
    }

    func selectInput(uid: String) {
        do {
            let session = AVAudioSession.sharedInstance()
            guard let port = session.availableInputs?.first(where: { $0.uid == uid }) else { return }
            sourceMode = .audioInterface
            try session.setPreferredInput(port)
            refreshAvailableInputs()
            if isRunning { stop(); try start() }
        } catch { lastError = error.localizedDescription }
    }
    #endif


    private func registerConfigurationObservers() {
        let center = NotificationCenter.default
        notificationTokens.append(center.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: .main) { [weak self] _ in
            Task { @MainActor in self?.recoverAfterRouteChange() }
        })
        #if os(iOS)
        notificationTokens.append(center.addObserver(forName: AVAudioSession.routeChangeNotification, object: nil, queue: .main) { [weak self] _ in
            Task { @MainActor in
                self?.refreshAvailableInputs()
                self?.recoverAfterRouteChange()
            }
        })
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
        telemetryTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 20.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.refreshTelemetry() }
        }
    }

    private func refreshTelemetry() {
        let stats = realtime.stats()
        inputFrames = stats.input_frames
        chipFrames = stats.chip_frames
        underflows = stats.output_underflows
        overflows = stats.output_overflows
        let scope = realtime.scope(maxFrames: 1024)
        scopeLeft = scope.0
        scopeRight = scope.1
        updateRouteDescription()
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
        #if os(iOS)
        let route = AVAudioSession.sharedInstance().currentRoute
        let output = route.outputs.first?.portName ?? "No output"
        if sourceMode == .testGenerator {
            routeDescription = "Test Generator → \(output)"
        } else {
            let input = route.inputs.first?.portName ?? "No input"
            routeDescription = "\(input) → \(output)"
        }
        #else
        if sourceMode == .testGenerator {
            routeDescription = "Test Generator → system default output"
        } else {
            routeDescription = "System default input → output"
        }
        #endif
    }

    private func syncGeneratorSettings() {
        realtime.configureTestGenerator(
            kind: generatorKind.bridgeValue,
            frequency: max(0.0, generatorFrequency),
            amplitude: min(1.0, max(0.0, generatorAmplitude)),
            sweepEnd: max(1.0, generatorSweepEnd),
            sweepSeconds: max(0.001, generatorSweepSeconds),
            impulsePeriod: max(0.001, generatorImpulsePeriod)
        )
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
}
