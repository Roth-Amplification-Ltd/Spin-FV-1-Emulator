import Foundation

enum AppleFileLoopState: UInt32, Sendable {
    case stopped = 0
    case playing = 1
    case paused = 2

    var label: String {
        switch self {
        case .stopped: return "Stopped"
        case .playing: return "Playing"
        case .paused: return "Paused"
        }
    }
}

struct AppleFileLoopInfo: Sendable {
    var state: AppleFileLoopState = .stopped
    var looping = true
    var fileSampleRate: UInt32 = 0
    var totalFrames: UInt64 = 0
    var duration = 0.0
    var position = 0.0
    var loopBegin = 0.0
    var loopEnd = 0.0
    var crossfadeMS = 0.0

    static let empty = AppleFileLoopInfo()
}

enum FV1TestbenchError: LocalizedError {
    case operation(String)

    var errorDescription: String? {
        switch self {
        case let .operation(message):
            return message
        }
    }
}

final class FV1FileLoopBridge: @unchecked Sendable {
    private var handle: OpaquePointer?

    init() throws {
        var created: OpaquePointer?
        let result = fv1_testbench_file_source_create(
            &created
        )
        guard result == FV1_TESTBENCH_OK,
              created != nil else {
            throw FV1TestbenchError.operation(
                "Unable to create FV-1 Lab file-loop source."
            )
        }
        handle = created
    }

    deinit {
        fv1_testbench_file_source_destroy(
            handle
        )
    }

    func load(url: URL) throws {
        var diagnostic = [CChar](
            repeating: 0,
            count: 1024
        )

        let result: fv1_testbench_result =
            url.path.withCString { path in
                diagnostic.withUnsafeMutableBufferPointer {
                    buffer in
                    fv1_testbench_file_source_load(
                        handle,
                        path,
                        buffer.baseAddress,
                        buffer.count
                    )
                }
            }

        guard result == FV1_TESTBENCH_OK else {
            throw FV1TestbenchError.operation(
                diagnosticString(
                    diagnostic,
                    fallback:
                        "Unable to load WAV file."
                )
            )
        }
    }

    func prepare(
        sampleRate: Double,
        maxFrames: Int = 4096
    ) throws {
        let result =
            fv1_testbench_file_source_prepare(
                handle,
                sampleRate,
                maxFrames
            )

        guard result == FV1_TESTBENCH_OK else {
            throw FV1TestbenchError.operation(
                "Unable to prepare the file-loop source at \(Int(sampleRate)) Hz."
            )
        }
    }

    func render(
        left: UnsafeMutablePointer<Float>,
        right: UnsafeMutablePointer<Float>,
        frames: Int
    ) {
        _ =
            fv1_testbench_file_source_render_planar(
                handle,
                left,
                right,
                frames
            )
    }

    func play() {
        fv1_testbench_file_source_play(
            handle
        )
    }

    func pause() {
        fv1_testbench_file_source_pause(
            handle
        )
    }

    func stop() {
        fv1_testbench_file_source_stop(
            handle
        )
    }

    func setLooping(_ enabled: Bool) {
        fv1_testbench_file_source_set_looping(
            handle,
            enabled ? 1 : 0
        )
    }

    func seek(seconds: Double) throws {
        let result =
            fv1_testbench_file_source_seek_seconds(
                handle,
                seconds
            )
        guard result == FV1_TESTBENCH_OK else {
            throw FV1TestbenchError.operation(
                "Unable to seek the audio loop."
            )
        }
    }

    func setLoopRegion(
        begin: Double,
        end: Double
    ) throws {
        let result =
            fv1_testbench_file_source_set_loop_region_seconds(
                handle,
                begin,
                end
            )
        guard result == FV1_TESTBENCH_OK else {
            throw FV1TestbenchError.operation(
                "Invalid audio-loop region."
            )
        }
    }

    func setCrossfade(milliseconds: Double) {
        fv1_testbench_file_source_set_crossfade_ms(
            handle,
            milliseconds
        )
    }

    func info() -> AppleFileLoopInfo {
        var raw = fv1_testbench_file_info_v1()
        fv1_testbench_file_info_v1_init(
            &raw
        )

        guard fv1_testbench_file_source_get_info(
            handle,
            &raw
        ) == FV1_TESTBENCH_OK else {
            return .empty
        }

        return AppleFileLoopInfo(
            state:
                AppleFileLoopState(
                    rawValue:
                        raw.transport_state
                ) ?? .stopped,
            looping: raw.looping != 0,
            fileSampleRate:
                raw.file_sample_rate,
            totalFrames:
                raw.total_frames,
            duration:
                raw.duration_seconds,
            position:
                raw.position_seconds,
            loopBegin:
                raw.loop_begin_seconds,
            loopEnd:
                raw.loop_end_seconds,
            crossfadeMS:
                raw.loop_crossfade_ms
        )
    }
}


struct FV1ValidationConfiguration: Sendable {
    var maxAlignmentMS = 100.0
    var gainMatchResidual = false
    var fftSize: UInt32 = 16_384
    var spectralFloorDB = -90.0
    var minimumCorrelation = 0.995
    var maximumResidualRMSDBFS = -45.0
    var maximumResidualPeakDBFS = -24.0
}

struct FV1ValidationChannelSummary: Sendable {
    var referenceRMSDBFS = -200.0
    var captureRMSDBFS = -200.0
    var gainErrorDB = 0.0
    var correlation = 0.0
    var residualRMSDBFS = -200.0
    var residualPeakDBFS = -200.0
    var snrDB = 200.0
}

struct FV1ValidationSummary: Sendable {
    var passed = false
    var sampleRate: UInt32 = 0
    var failureCount: UInt32 = 0
    var captureDelayFrames: Int64 = 0
    var comparedFrames: UInt64 = 0
    var captureDelayMS = 0.0
    var appliedCaptureGainDB = 0.0
    var left = FV1ValidationChannelSummary()
    var right = FV1ValidationChannelSummary()
    var spectralRMSMagnitudeErrorDB = 0.0
    var spectralWorstMagnitudeErrorDB = 0.0
    var spectralWorstPhaseErrorDegrees = 0.0
}

struct FV1ValidationOutcome: Sendable {
    let summary: FV1ValidationSummary
    let failureText: String
}

enum FV1Testbench {
    static func validate(
        referenceURL: URL,
        captureURL: URL,
        configuration:
            FV1ValidationConfiguration,
        reportPrefixURL: URL?
    ) throws -> FV1ValidationOutcome {
        var config =
            fv1_testbench_validation_config_v1()
        fv1_testbench_validation_config_v1_init(
            &config
        )

        config.max_alignment_ms =
            configuration.maxAlignmentMS
        config.gain_match_residual =
            configuration.gainMatchResidual
            ? 1 : 0
        config.fft_size =
            configuration.fftSize
        config.spectral_floor_db =
            configuration.spectralFloorDB
        config.minimum_correlation =
            configuration.minimumCorrelation
        config.maximum_residual_rms_dbfs =
            configuration.maximumResidualRMSDBFS
        config.maximum_residual_peak_dbfs =
            configuration.maximumResidualPeakDBFS

        var rawSummary =
            fv1_testbench_validation_summary_v1()
        fv1_testbench_validation_summary_v1_init(
            &rawSummary
        )

        var failures = [CChar](
            repeating: 0,
            count: 4096
        )
        var error = [CChar](
            repeating: 0,
            count: 2048
        )

        let reportPath =
            reportPrefixURL?.path ?? ""

        let result: fv1_testbench_result =
            referenceURL.path.withCString {
                referencePath in
                captureURL.path.withCString {
                    capturePath in
                    reportPath.withCString {
                        reportPrefix in
                        failures
                            .withUnsafeMutableBufferPointer {
                                failureBuffer in
                                error
                                    .withUnsafeMutableBufferPointer {
                                        errorBuffer in
                                        fv1_testbench_validate_wavs(
                                            referencePath,
                                            capturePath,
                                            &config,
                                            reportPath.isEmpty
                                                ? nil
                                                : reportPrefix,
                                            &rawSummary,
                                            failureBuffer.baseAddress,
                                            failureBuffer.count,
                                            errorBuffer.baseAddress,
                                            errorBuffer.count
                                        )
                                    }
                            }
                    }
                }
            }

        guard result == FV1_TESTBENCH_OK else {
            throw FV1TestbenchError.operation(
                diagnosticString(
                    error,
                    fallback:
                        "FV-1 validation comparison failed."
                )
            )
        }

        let left =
            FV1ValidationChannelSummary(
                referenceRMSDBFS:
                    rawSummary
                        .left_reference_rms_dbfs,
                captureRMSDBFS:
                    rawSummary
                        .left_capture_rms_dbfs,
                gainErrorDB:
                    rawSummary
                        .left_gain_error_db,
                correlation:
                    rawSummary
                        .left_correlation,
                residualRMSDBFS:
                    rawSummary
                        .left_residual_rms_dbfs,
                residualPeakDBFS:
                    rawSummary
                        .left_residual_peak_dbfs,
                snrDB:
                    rawSummary.left_snr_db
            )

        let right =
            FV1ValidationChannelSummary(
                referenceRMSDBFS:
                    rawSummary
                        .right_reference_rms_dbfs,
                captureRMSDBFS:
                    rawSummary
                        .right_capture_rms_dbfs,
                gainErrorDB:
                    rawSummary
                        .right_gain_error_db,
                correlation:
                    rawSummary
                        .right_correlation,
                residualRMSDBFS:
                    rawSummary
                        .right_residual_rms_dbfs,
                residualPeakDBFS:
                    rawSummary
                        .right_residual_peak_dbfs,
                snrDB:
                    rawSummary.right_snr_db
            )

        let summary =
            FV1ValidationSummary(
                passed:
                    rawSummary.passed != 0,
                sampleRate:
                    rawSummary.sample_rate,
                failureCount:
                    rawSummary.failure_count,
                captureDelayFrames:
                    rawSummary
                        .capture_delay_frames,
                comparedFrames:
                    rawSummary.compared_frames,
                captureDelayMS:
                    rawSummary.capture_delay_ms,
                appliedCaptureGainDB:
                    rawSummary
                        .applied_capture_gain_db,
                left: left,
                right: right,
                spectralRMSMagnitudeErrorDB:
                    rawSummary
                        .spectral_rms_magnitude_error_db,
                spectralWorstMagnitudeErrorDB:
                    rawSummary
                        .spectral_worst_magnitude_error_db,
                spectralWorstPhaseErrorDegrees:
                    rawSummary
                        .spectral_worst_phase_error_degrees
            )

        return FV1ValidationOutcome(
            summary: summary,
            failureText:
                diagnosticString(
                    failures,
                    fallback: ""
                )
        )
    }

    static func writeValidationPack(
        directoryURL: URL,
        sampleRate: UInt32,
        seconds: Double,
        level: Double,
        seed: UInt32 = 0x465631
    ) throws {
        var error = [CChar](
            repeating: 0,
            count: 2048
        )

        let result: fv1_testbench_result =
            directoryURL.path.withCString {
                path in
                error.withUnsafeMutableBufferPointer {
                    buffer in
                    fv1_testbench_write_validation_pack(
                        path,
                        sampleRate,
                        seconds,
                        level,
                        seed,
                        buffer.baseAddress,
                        buffer.count
                    )
                }
            }

        guard result == FV1_TESTBENCH_OK else {
            throw FV1TestbenchError.operation(
                diagnosticString(
                    error,
                    fallback:
                        "Unable to generate validation pack."
                )
            )
        }
    }
}


private func diagnosticString(
    _ buffer: [CChar],
    fallback: String
) -> String {
    let text =
        buffer.withUnsafeBufferPointer {
            raw -> String in
            guard let base = raw.baseAddress else {
                return ""
            }
            return String(cString: base)
        }

    return text.isEmpty
        ? fallback
        : text
}
