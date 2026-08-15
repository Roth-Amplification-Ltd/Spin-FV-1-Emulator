import AVFAudio
import AudioToolbox
import Combine
import Foundation


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
        #if os(iOS)
        refreshAvailableInputs()
        #endif
        registerConfigurationObservers()
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
        let inputNode = engine.inputNode
        let outputNode = engine.outputNode
        let inputHardware = inputNode.outputFormat(forBus: 0)
        let outputHardware = outputNode.inputFormat(forBus: 0)
        guard inputHardware.sampleRate > 0, inputHardware.channelCount > 0,
              outputHardware.sampleRate > 0, outputHardware.channelCount > 0 else {
            throw FV1EngineError.sdk(FV1_SDK_ERROR_BAD_STATE, "No usable Apple audio input/output route")
        }

        let inputChannels = AVAudioChannelCount(min(2, Int(inputHardware.channelCount)))
        guard let inputFormat = AVAudioFormat(commonFormat: .pcmFormatFloat32,
                                              sampleRate: inputHardware.sampleRate,
                                              channels: inputChannels,
                                              interleaved: false),
              let renderFormat = AVAudioFormat(commonFormat: .pcmFormatFloat32,
                                               sampleRate: outputHardware.sampleRate,
                                               channels: 2,
                                               interleaved: false) else {
            throw FV1EngineError.sdk(FV1_SDK_ERROR_UNSUPPORTED, "Unable to create canonical Float32 Apple audio formats")
        }

        try realtime.configureAndPrime(inputRate: inputFormat.sampleRate,
                                       outputRate: renderFormat.sampleRate,
                                       primeFrames: 256)
        realtime.setPots(pots)

        let bridge = realtime
        let source = AVAudioSourceNode(format: renderFormat) { _, _, frameCount, outputData in
            let buffers = UnsafeMutableAudioBufferListPointer(outputData)
            guard buffers.count >= 2,
                  let leftData = buffers[0].mData,
                  let rightData = buffers[1].mData else { return kAudio_ParamError }
            let left = leftData.bindMemory(to: Float.self, capacity: Int(frameCount))
            let right = rightData.bindMemory(to: Float.self, capacity: Int(frameCount))
            bridge.render(left: left, right: right, frames: Int(frameCount))
            return noErr
        }
        sourceNode = source
        engine.attach(source)
        engine.connect(source, to: engine.mainMixerNode, format: renderFormat)

        inputNode.installTap(onBus: 0, bufferSize: 512, format: inputFormat) { buffer, _ in
            guard let channels = buffer.floatChannelData, buffer.frameLength > 0 else { return }
            let left = UnsafePointer(channels[0])
            let right = UnsafePointer(channels[Int(min(1, inputFormat.channelCount - 1))])
            bridge.process(left: left, right: right, frames: Int(buffer.frameLength))
        }
        inputTapInstalled = true

        engine.prepare()
        do {
            try engine.start()
        } catch {
            inputNode.removeTap(onBus: 0)
            inputTapInstalled = false
            engine.detach(source)
            sourceNode = nil
            throw error
        }

        inputSampleRate = inputFormat.sampleRate
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
        try session.setCategory(.playAndRecord, mode: .measurement,
                                options: [.allowBluetoothHFP, .allowAirPlay])
        try session.setPreferredSampleRate(48_000)
        try session.setPreferredIOBufferDuration(0.005)
        try session.setActive(true)
        refreshAvailableInputs()
        #endif
    }

    private func updateRouteDescription() {
        #if os(iOS)
        let route = AVAudioSession.sharedInstance().currentRoute
        let input = route.inputs.first?.portName ?? "No input"
        let output = route.outputs.first?.portName ?? "No output"
        routeDescription = "\(input) → \(output)"
        #else
        routeDescription = "System default input → output"
        #endif
    }
}
