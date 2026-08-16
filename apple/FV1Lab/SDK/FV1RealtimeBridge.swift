import Foundation

enum AppleAnalysisStream: UInt32, CaseIterable, Identifiable {
    case raw = 0
    case processed = 1

    var id: UInt32 { rawValue }

    var label: String {
        switch self {
        case .raw: return "RAW INPUT"
        case .processed: return "PROCESSED"
        }
    }
}

enum AppleRecordMode: UInt32, CaseIterable, Identifiable {
    case processed = 0
    case raw = 1
    case rawAndProcessed = 2

    var id: UInt32 { rawValue }

    var label: String {
        switch self {
        case .processed: return "Processed"
        case .raw: return "Raw"
        case .rawAndProcessed: return "Raw + Processed"
        }
    }
}

struct AppleAnalysisSnapshot {
    var sampleRate = 0.0
    var sequence: UInt64 = 0
    var droppedFrames: UInt64 = 0
    var peakLeft: Float = 0
    var peakRight: Float = 0
    var rmsLeft: Float = 0
    var rmsRight: Float = 0
    var correlation: Float = 0
    var dominantFrequency: Float = 0
    var dominantLevelDB: Float = -200
    var spectrumDB: [Float] = []
    var scopeLeft: [Float] = []
    var scopeRight: [Float] = []

    static let empty = AppleAnalysisSnapshot()
}

struct AppleRecorderStats {
    var rawFramesWritten: UInt64 = 0
    var processedFramesWritten: UInt64 = 0
    var rawFramesDropped: UInt64 = 0
    var processedFramesDropped: UInt64 = 0
}

enum AppleRecordingError: LocalizedError {
    case invalidSampleRates
    case recorder(String)

    var errorDescription: String? {
        switch self {
        case .invalidSampleRates:
            return "Raw + Processed recording requires matching Apple input and output sample rates."
        case let .recorder(message):
            return message
        }
    }
}

final class FV1RealtimeBridge: @unchecked Sendable {
    private var handle: OpaquePointer?

    init(
        inputRate: Double = 48_000,
        outputRate: Double = 48_000
    ) throws {
        var created: OpaquePointer?
        let result = fv1_apple_realtime_create(
            inputRate,
            outputRate,
            32_768,
            &created
        )
        guard result == FV1_SDK_OK,
              created != nil else {
            throw FV1EngineError.sdk(
                result,
                "Unable to create Apple realtime bridge"
            )
        }
        handle = created
    }

    deinit {
        fv1_apple_realtime_destroy(handle)
    }

    func configure(
        inputRate: Double,
        outputRate: Double
    ) throws {
        let result =
            fv1_apple_realtime_configure_rates(
                handle,
                inputRate,
                outputRate
            )
        if result != FV1_SDK_OK {
            throw FV1EngineError.sdk(
                result,
                "Unable to configure Apple audio rates"
            )
        }
    }

    func load(program: Data) throws {
        guard program.count
            == Int(FV1_SDK_PROGRAM_BYTES) else {
            throw FV1EngineError.invalidProgramSize(
                program.count
            )
        }

        let result =
            program.withUnsafeBytes {
                bytes -> fv1_sdk_result in

                guard let base =
                    bytes.bindMemory(
                        to: UInt8.self
                    ).baseAddress else {
                    return fv1_sdk_result(
                        FV1_SDK_ERROR_INVALID_ARGUMENT
                    )
                }

                return fv1_apple_realtime_load_program(
                    handle,
                    base,
                    bytes.count
                )
            }

        if result != FV1_SDK_OK {
            throw FV1EngineError.sdk(
                result,
                "Unable to load realtime program"
            )
        }
    }

    func setPots(_ pots: [Float]) {
        guard pots.count == 3 else { return }

        fv1_apple_realtime_set_pots(
            handle,
            pots[0],
            pots[1],
            pots[2]
        )
    }

    func setDSPEnabled(_ enabled: Bool) {
        fv1_apple_realtime_set_dsp_enabled(
            handle,
            enabled ? 1 : 0
        )
    }

    var dspEnabled: Bool {
        fv1_apple_realtime_get_dsp_enabled(
            handle
        ) != 0
    }

    func setAnalyzerFFTSize(_ size: Int) {
        guard [1024, 2048, 4096, 8192]
            .contains(size) else {
            return
        }

        _ = fv1_apple_realtime_set_analyzer_fft_size(
            handle,
            size
        )
    }

    var analyzerFFTSize: Int {
        Int(
            fv1_apple_realtime_get_analyzer_fft_size(
                handle
            )
        )
    }

    func configureTestGenerator(
        kind: UInt32,
        frequency: Double,
        amplitude: Double,
        sweepEnd: Double,
        sweepSeconds: Double,
        impulsePeriod: Double
    ) {
        fv1_apple_realtime_set_test_generator(
            handle,
            kind,
            frequency,
            amplitude,
            sweepEnd,
            sweepSeconds,
            impulsePeriod
        )
    }

    func processTestGenerator(frames: Int) {
        _ = fv1_apple_realtime_process_test_generator(
            handle,
            frames
        )
    }

    func reset() throws {
        let result =
            fv1_apple_realtime_reset(
                handle,
                1
            )
        if result != FV1_SDK_OK {
            throw FV1EngineError.sdk(
                result,
                "Unable to reset realtime engine"
            )
        }
    }

    func configureAndPrime(
        inputRate: Double,
        outputRate: Double,
        primeFrames: UInt32 = 256
    ) throws {
        try configure(
            inputRate: inputRate,
            outputRate: outputRate
        )

        fv1_apple_realtime_prime_silence(
            handle,
            primeFrames
        )
    }

    func process(
        left: UnsafePointer<Float>,
        right: UnsafePointer<Float>,
        frames: Int
    ) {
        _ = fv1_apple_realtime_process_planar_input(
            handle,
            left,
            right,
            frames
        )
    }

    func render(
        left: UnsafeMutablePointer<Float>,
        right: UnsafeMutablePointer<Float>,
        frames: Int
    ) {
        fv1_apple_realtime_render_planar_output(
            handle,
            left,
            right,
            frames
        )
    }

    func stats()
        -> fv1_apple_realtime_stats_v1 {
        var value =
            fv1_apple_realtime_stats_v1()
        fv1_apple_realtime_stats_v1_init(
            &value
        )
        fv1_apple_realtime_get_stats(
            handle,
            &value
        )
        return value
    }

    func analysis(
        stream: AppleAnalysisStream,
        maxSpectrumBins: Int = 4097,
        maxScopeFrames: Int = 512
    ) -> AppleAnalysisSnapshot {
        var metadata =
            fv1_apple_analysis_snapshot_v1()
        fv1_apple_analysis_snapshot_v1_init(
            &metadata
        )

        var spectrum = [Float](
            repeating: -200,
            count: maxSpectrumBins
        )
        var left = [Float](
            repeating: 0,
            count: maxScopeFrames
        )
        var right = [Float](
            repeating: 0,
            count: maxScopeFrames
        )

        let result =
            spectrum
                .withUnsafeMutableBufferPointer {
                    spectrumBuffer in
                    left
                        .withUnsafeMutableBufferPointer {
                            leftBuffer in
                            right
                                .withUnsafeMutableBufferPointer {
                                    rightBuffer in

                                    fv1_apple_realtime_copy_analysis(
                                        handle,
                                        stream.rawValue,
                                        &metadata,
                                        spectrumBuffer.baseAddress,
                                        spectrumBuffer.count,
                                        leftBuffer.baseAddress,
                                        rightBuffer.baseAddress,
                                        leftBuffer.count
                                    )
                                }
                        }
                }

        guard result == FV1_SDK_OK else {
            return .empty
        }

        let spectrumCount =
            min(
                Int(
                    metadata.spectrum_bins
                ),
                spectrum.count
            )

        let scopeCount =
            min(
                Int(
                    metadata.scope_frames
                ),
                left.count
            )

        return AppleAnalysisSnapshot(
            sampleRate:
                metadata.sample_rate,
            sequence:
                metadata.sequence,
            droppedFrames:
                metadata.dropped_frames,
            peakLeft:
                metadata.peak_left,
            peakRight:
                metadata.peak_right,
            rmsLeft:
                metadata.rms_left,
            rmsRight:
                metadata.rms_right,
            correlation:
                metadata.correlation,
            dominantFrequency:
                metadata
                    .dominant_frequency_hz,
            dominantLevelDB:
                metadata
                    .dominant_level_db,
            spectrumDB:
                Array(
                    spectrum.prefix(
                        spectrumCount
                    )
                ),
            scopeLeft:
                Array(
                    left.prefix(
                        scopeCount
                    )
                ),
            scopeRight:
                Array(
                    right.prefix(
                        scopeCount
                    )
                )
        )
    }

    func startRecording(
        url: URL,
        sampleRate: UInt32,
        mode: AppleRecordMode
    ) throws {
        var diagnostic = [CChar](
            repeating: 0,
            count: 1024
        )

        let result:
            fv1_sdk_result =
            url.path.withCString {
                path in
                diagnostic
                    .withUnsafeMutableBufferPointer {
                        buffer in

                        fv1_apple_realtime_start_recording(
                            handle,
                            path,
                            sampleRate,
                            mode.rawValue,
                            buffer.baseAddress,
                            buffer.count
                        )
                    }
            }

        guard result == FV1_SDK_OK else {
            let message =
                diagnostic
                    .withUnsafeBufferPointer {
                        buffer -> String in
                        guard let base =
                            buffer.baseAddress else {
                            return "Unable to start recording."
                        }
                        return String(
                            cString: base
                        )
                    }

            throw AppleRecordingError
                .recorder(
                    message.isEmpty
                        ? "Unable to start recording."
                        : message
                )
        }
    }

    func stopRecording() {
        fv1_apple_realtime_stop_recording(
            handle
        )
    }

    var isRecording: Bool {
        fv1_apple_realtime_is_recording(
            handle
        ) != 0
    }

    func recorderStats()
        -> AppleRecorderStats {
        var value =
            fv1_apple_recorder_stats_v1()
        fv1_apple_recorder_stats_v1_init(
            &value
        )

        fv1_apple_realtime_get_recorder_stats(
            handle,
            &value
        )

        return AppleRecorderStats(
            rawFramesWritten:
                value.raw_frames_written,
            processedFramesWritten:
                value.processed_frames_written,
            rawFramesDropped:
                value.raw_frames_dropped,
            processedFramesDropped:
                value.processed_frames_dropped
        )
    }

    func scope(
        maxFrames: Int = 1024
    ) -> ([Float], [Float]) {
        var left = [Float](
            repeating: 0,
            count: maxFrames
        )
        var right = [Float](
            repeating: 0,
            count: maxFrames
        )

        let count =
            left
                .withUnsafeMutableBufferPointer {
                    leftBuffer in
                    right
                        .withUnsafeMutableBufferPointer {
                            rightBuffer in

                            fv1_apple_realtime_copy_scope(
                                handle,
                                leftBuffer.baseAddress,
                                rightBuffer.baseAddress,
                                maxFrames
                            )
                        }
                }

        return (
            Array(
                left.prefix(
                    Int(count)
                )
            ),
            Array(
                right.prefix(
                    Int(count)
                )
            )
        )
    }
}
