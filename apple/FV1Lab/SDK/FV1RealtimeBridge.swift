import Foundation

final class FV1RealtimeBridge: @unchecked Sendable {
    private var handle: OpaquePointer?

    init(inputRate: Double = 48_000, outputRate: Double = 48_000) throws {
        var created: OpaquePointer?
        let result = fv1_apple_realtime_create(inputRate, outputRate, 32_768, &created)
        guard result == FV1_SDK_OK, created != nil else {
            throw FV1EngineError.sdk(result, "Unable to create Apple realtime bridge")
        }
        handle = created
    }

    deinit { fv1_apple_realtime_destroy(handle) }

    func configure(inputRate: Double, outputRate: Double) throws {
        let result = fv1_apple_realtime_configure_rates(handle, inputRate, outputRate)
        if result != FV1_SDK_OK { throw FV1EngineError.sdk(result, "Unable to configure Apple audio rates") }
    }

    func load(program: Data) throws {
        guard program.count == Int(FV1_SDK_PROGRAM_BYTES) else { throw FV1EngineError.invalidProgramSize(program.count) }
        let result = program.withUnsafeBytes { bytes -> fv1_sdk_result in
            guard let base = bytes.bindMemory(to: UInt8.self).baseAddress else { return fv1_sdk_result(FV1_SDK_ERROR_INVALID_ARGUMENT) }
            return fv1_apple_realtime_load_program(handle, base, bytes.count)
        }
        if result != FV1_SDK_OK { throw FV1EngineError.sdk(result, "Unable to load realtime program") }
    }

    func setPots(_ pots: [Float]) {
        guard pots.count == 3 else { return }
        fv1_apple_realtime_set_pots(handle, pots[0], pots[1], pots[2])
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
        _ = fv1_apple_realtime_process_test_generator(handle, frames)
    }

    func reset() throws {
        let result = fv1_apple_realtime_reset(handle, 1)
        if result != FV1_SDK_OK { throw FV1EngineError.sdk(result, "Unable to reset realtime engine") }
    }

    func configureAndPrime(inputRate: Double, outputRate: Double, primeFrames: UInt32 = 256) throws {
        try configure(inputRate: inputRate, outputRate: outputRate)
        fv1_apple_realtime_prime_silence(handle, primeFrames)
    }

    func process(left: UnsafePointer<Float>, right: UnsafePointer<Float>, frames: Int) {
        _ = fv1_apple_realtime_process_planar_input(handle, left, right, frames)
    }

    func render(left: UnsafeMutablePointer<Float>, right: UnsafeMutablePointer<Float>, frames: Int) {
        fv1_apple_realtime_render_planar_output(handle, left, right, frames)
    }

    func stats() -> fv1_apple_realtime_stats_v1 {
        var value = fv1_apple_realtime_stats_v1()
        fv1_apple_realtime_stats_v1_init(&value)
        fv1_apple_realtime_get_stats(handle, &value)
        return value
    }

    func scope(maxFrames: Int = 1024) -> ([Float], [Float]) {
        var left = [Float](repeating: 0, count: maxFrames)
        var right = [Float](repeating: 0, count: maxFrames)
        let count = left.withUnsafeMutableBufferPointer { l in
            right.withUnsafeMutableBufferPointer { r in
                fv1_apple_realtime_copy_scope(handle, l.baseAddress, r.baseAddress, maxFrames)
            }
        }
        return (Array(left.prefix(Int(count))), Array(right.prefix(Int(count))))
    }
}
