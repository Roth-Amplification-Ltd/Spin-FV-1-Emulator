import Foundation

struct FV1CompileResult {
    let program: Data
    let instructionCount: UInt32
    let highestDelayAddress: UInt32
}

struct FV1ChipSnapshot {
    let acc: Int32
    let pacc: Int32
    let lr: Int32
    let addressPointer: Int32
    let dacLeft: Int32
    let dacRight: Int32
    let programCounter: UInt32
    let sampleCounter: UInt64
    let instructionCounter: UInt32
    let pots: [Int32]
}

struct FV1ResourceSummary {
    let usedInstructions: UInt32
    let worstCasePath: UInt32
    let staticDelayReads: UInt32
    let staticDelayWrites: UInt32
    let dynamicDelayReads: UInt32
    let highestStaticDelayAddress: UInt32
    let generalRegistersUsed: UInt32
    let potsUsed: UInt32
    let sineLFOsUsed: UInt32
    let rampLFOsUsed: UInt32
    let skipInstructions: UInt32
}

enum FV1EngineError: LocalizedError {
    case sdk(Int32, String)
    case compile(line: UInt32, column: UInt32, diagnostic: String)
    case invalidProgramSize(Int)

    var errorDescription: String? {
        switch self {
        case let .sdk(code, message): return "FV-1 SDK error \(code): \(message)"
        case let .compile(line, column, diagnostic):
            return "SpinASM compile error at \(line):\(column): \(diagnostic)"
        case let .invalidProgramSize(size): return "FV-1 program must be 512 bytes (received \(size))."
        }
    }
}

final class FV1Engine {
    private var handle: OpaquePointer?
    static let virtualSampleRate = 32_768.0

    init() throws {
        var config = fv1_sdk_engine_config_v1()
        fv1_sdk_engine_config_v1_init(&config)
        config.virtual_sample_rate = Self.virtualSampleRate
        var created: OpaquePointer?
        try Self.check(fv1_sdk_engine_create_v1(&config, &created))
        guard created != nil else { throw FV1EngineError.sdk(FV1_SDK_ERROR_INTERNAL, "SDK returned a null engine") }
        handle = created
    }

    deinit { fv1_sdk_engine_destroy(handle) }

    static var versionString: String { String(cString: fv1_sdk_get_version_string()) }
    static var abiVersion: UInt32 { fv1_sdk_get_abi_version() }

    static func compile(_ source: String) throws -> FV1CompileResult {
        var program = [UInt8](repeating: 0, count: Int(FV1_SDK_PROGRAM_BYTES))
        var report = fv1_sdk_compile_report_v1()
        fv1_sdk_compile_report_v1_init(&report)
        var diagnostic = [CChar](repeating: 0, count: 1024)
        let result: fv1_sdk_result = source.utf8CString.withUnsafeBufferPointer { src in
            program.withUnsafeMutableBufferPointer { image in
                diagnostic.withUnsafeMutableBufferPointer { diag in
                    fv1_sdk_compile_spinasm_v1(src.baseAddress, source.utf8.count,
                                               image.baseAddress, image.count,
                                               &report, diag.baseAddress, diag.count)
                }
            }
        }
        if result != FV1_SDK_OK {
            let message = diagnostic.withUnsafeBufferPointer { ptr in
                guard let base = ptr.baseAddress else { return "Unknown compiler error" }
                return String(cString: base)
            }
            if result == FV1_SDK_ERROR_COMPILE {
                throw FV1EngineError.compile(line: report.error_line, column: report.error_column, diagnostic: message)
            }
            throw FV1EngineError.sdk(result, message)
        }
        return FV1CompileResult(program: Data(program), instructionCount: report.instruction_count,
                                highestDelayAddress: report.highest_delay_address)
    }

    func load(program: Data) throws {
        guard program.count == Int(FV1_SDK_PROGRAM_BYTES) else { throw FV1EngineError.invalidProgramSize(program.count) }
        let result = program.withUnsafeBytes { bytes -> fv1_sdk_result in
            guard let base = bytes.bindMemory(to: UInt8.self).baseAddress else { return FV1_SDK_ERROR_INVALID_ARGUMENT }
            return fv1_sdk_engine_load_program(handle, base, program.count)
        }
        try Self.check(result)
    }

    func setPots(_ values: [Float]) throws {
        guard values.count == 3 else { throw FV1EngineError.sdk(FV1_SDK_ERROR_INVALID_ARGUMENT, "Three POT values are required") }
        try Self.check(fv1_sdk_engine_set_pots(handle, values[0], values[1], values[2]))
    }

    func reset(clearDelay: Bool = true) throws {
        try Self.check(fv1_sdk_engine_reset(handle, clearDelay ? 1 : 0))
    }

    func snapshot() throws -> FV1ChipSnapshot {
        var snap = fv1_sdk_snapshot_v1()
        fv1_sdk_snapshot_v1_init(&snap)
        try Self.check(fv1_sdk_engine_get_snapshot_v1(handle, &snap))
        var registerTuple = snap.regs
        func reg(_ index: Int) -> Int32 {
            withUnsafeBytes(of: &registerTuple) { raw in
                raw.bindMemory(to: Int32.self)[index]
            }
        }
        return FV1ChipSnapshot(acc: snap.acc, pacc: snap.pacc, lr: snap.lr,
                               addressPointer: reg(Int(FV1_SDK_REG_ADDR_PTR)),
                               dacLeft: reg(Int(FV1_SDK_REG_DACL)), dacRight: reg(Int(FV1_SDK_REG_DACR)),
                               programCounter: snap.program_counter, sampleCounter: snap.sample_counter,
                               instructionCounter: snap.instruction_counter,
                               pots: [reg(Int(FV1_SDK_REG_POT0)), reg(Int(FV1_SDK_REG_POT1)), reg(Int(FV1_SDK_REG_POT2))])
    }

    func resources() throws -> FV1ResourceSummary {
        var report = fv1_sdk_resource_report_v1()
        fv1_sdk_resource_report_v1_init(&report)
        try Self.check(fv1_sdk_engine_analyze_program_v1(handle, &report))
        return FV1ResourceSummary(usedInstructions: report.used_instructions, worstCasePath: report.worst_case_path,
                                  staticDelayReads: report.static_delay_reads, staticDelayWrites: report.static_delay_writes,
                                  dynamicDelayReads: report.dynamic_delay_reads,
                                  highestStaticDelayAddress: report.highest_static_delay_address,
                                  generalRegistersUsed: report.general_registers_used, potsUsed: report.pots_used,
                                  sineLFOsUsed: report.sine_lfos_used, rampLFOsUsed: report.ramp_lfos_used,
                                  skipInstructions: report.skip_instructions)
    }

    private static func check(_ result: fv1_sdk_result) throws {
        guard result == FV1_SDK_OK else {
            let message = fv1_sdk_result_string(result).map { String(cString: $0) } ?? "Unknown SDK error"
            throw FV1EngineError.sdk(result, message)
        }
    }
}
