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
    let delayPointer: UInt32
    let dacLeft: Int32
    let dacRight: Int32
    let programCounter: UInt32
    let sampleCounter: UInt64
    let instructionCounter: UInt32
    let firstRun: Bool
    let sampleActive: Bool

    let registers: [Int32]
    let pots: [Int32]
    let sineLFO: [Int32]
    let cosineLFO: [Int32]
    let rampLFO: [Int32]
}

struct FV1DebugTrace {
    let pcBefore: UInt32
    let pcAfter: UInt32
    let rawInstruction: UInt32
    let opcode: UInt8
    let opcodeName: String
    let skipped: Bool
    let sampleFinished: Bool
    let accBefore: Int32
    let accAfter: Int32
    let paccAfter: Int32
    let lrAfter: Int32
    let sampleIndex: UInt64
    let instructionIndex: UInt32
    let outputLeft: Float?
    let outputRight: Float?
}

struct FV1DelayWord: Identifiable {
    let address: UInt32
    let value: Int32

    var id: UInt32 { address }

    var normalized: Double {
        Double(value) / 8_388_608.0
    }
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
    case compile(
        line: UInt32,
        column: UInt32,
        diagnostic: String
    )
    case invalidProgramSize(Int)
    case debugger(String)

    var errorDescription: String? {
        switch self {
        case let .sdk(code, message):
            return "FV-1 SDK error \(code): \(message)"

        case let .compile(
            line,
            column,
            diagnostic
        ):
            return "SpinASM compile error at \(line):\(column): \(diagnostic)"

        case let .invalidProgramSize(size):
            return "FV-1 program must be 512 bytes (received \(size))."

        case let .debugger(message):
            return message
        }
    }
}

final class FV1Engine {
    private var handle: OpaquePointer?

    static let virtualSampleRate = 32_768.0

    init() throws {
        var config =
            fv1_sdk_engine_config_v1()
        fv1_sdk_engine_config_v1_init(
            &config
        )
        config.virtual_sample_rate =
            Self.virtualSampleRate

        var created: OpaquePointer?
        try Self.check(
            fv1_sdk_engine_create_v1(
                &config,
                &created
            )
        )

        guard created != nil else {
            throw FV1EngineError.sdk(
                fv1_sdk_result(
                    FV1_SDK_ERROR_INTERNAL
                ),
                "SDK returned a null engine"
            )
        }

        handle = created
    }

    deinit {
        fv1_sdk_engine_destroy(
            handle
        )
    }

    static var versionString: String {
        String(
            cString:
                fv1_sdk_get_version_string()
        )
    }

    static var abiVersion: UInt32 {
        fv1_sdk_get_abi_version()
    }

    static func compile(
        _ source: String
    ) throws -> FV1CompileResult {
        var program = [UInt8](
            repeating: 0,
            count:
                Int(FV1_SDK_PROGRAM_BYTES)
        )

        var report =
            fv1_sdk_compile_report_v1()
        fv1_sdk_compile_report_v1_init(
            &report
        )

        var diagnostic = [CChar](
            repeating: 0,
            count: 1024
        )

        let result: fv1_sdk_result =
            source.utf8CString
                .withUnsafeBufferPointer {
                    sourceBuffer in

                    program
                        .withUnsafeMutableBufferPointer {
                            imageBuffer in

                            diagnostic
                                .withUnsafeMutableBufferPointer {
                                    diagnosticBuffer in

                                    fv1_sdk_compile_spinasm_v1(
                                        sourceBuffer.baseAddress,
                                        source.utf8.count,
                                        imageBuffer.baseAddress,
                                        imageBuffer.count,
                                        &report,
                                        diagnosticBuffer.baseAddress,
                                        diagnosticBuffer.count
                                    )
                                }
                        }
                }

        if result != FV1_SDK_OK {
            let message =
                diagnostic
                    .withUnsafeBufferPointer {
                        pointer -> String in

                        guard let base =
                            pointer.baseAddress else {
                            return "Unknown compiler error"
                        }

                        return String(
                            cString: base
                        )
                    }

            if result
                == FV1_SDK_ERROR_COMPILE {
                throw FV1EngineError.compile(
                    line: report.error_line,
                    column:
                        report.error_column,
                    diagnostic: message
                )
            }

            throw FV1EngineError.sdk(
                result,
                message
            )
        }

        return FV1CompileResult(
            program: Data(program),
            instructionCount:
                report.instruction_count,
            highestDelayAddress:
                report.highest_delay_address
        )
    }

    func load(
        program: Data
    ) throws {
        guard program.count
            == Int(
                FV1_SDK_PROGRAM_BYTES
            ) else {
            throw FV1EngineError
                .invalidProgramSize(
                    program.count
                )
        }

        let result =
            program.withUnsafeBytes {
                bytes -> fv1_sdk_result in

                guard let base =
                    bytes
                        .bindMemory(
                            to: UInt8.self
                        )
                        .baseAddress else {
                    return fv1_sdk_result(
                        FV1_SDK_ERROR_INVALID_ARGUMENT
                    )
                }

                return fv1_sdk_engine_load_program(
                    handle,
                    base,
                    program.count
                )
            }

        try Self.check(result)
    }

    func setPots(
        _ values: [Float]
    ) throws {
        guard values.count == 3 else {
            throw FV1EngineError.sdk(
                fv1_sdk_result(
                    FV1_SDK_ERROR_INVALID_ARGUMENT
                ),
                "Three POT values are required"
            )
        }

        try Self.check(
            fv1_sdk_engine_set_pots(
                handle,
                values[0],
                values[1],
                values[2]
            )
        )
    }

    func reset(
        clearDelay: Bool = true
    ) throws {
        try Self.check(
            fv1_sdk_engine_reset(
                handle,
                clearDelay ? 1 : 0
            )
        )
    }

    func snapshot()
        throws -> FV1ChipSnapshot {
        var raw =
            fv1_sdk_snapshot_v1()
        fv1_sdk_snapshot_v1_init(
            &raw
        )

        try Self.check(
            fv1_sdk_engine_get_snapshot_v1(
                handle,
                &raw
            )
        )

        var registerTuple = raw.regs

        let registers: [Int32] =
            withUnsafeBytes(
                of: &registerTuple
            ) { bytes in
                let values =
                    bytes.bindMemory(
                        to: Int32.self
                    )

                return Array(
                    values.prefix(
                        Int(
                            FV1_SDK_REGISTER_COUNT
                        )
                    )
                )
            }

        func reg(
            _ index: UInt32
        ) -> Int32 {
            let value = Int(index)
            guard registers.indices
                .contains(value) else {
                return 0
            }
            return registers[value]
        }

        return FV1ChipSnapshot(
            acc: raw.acc,
            pacc: raw.pacc,
            lr: raw.lr,
            addressPointer:
                reg(
                    UInt32(
                        FV1_SDK_REG_ADDR_PTR
                    )
                ),
            delayPointer:
                raw.delay_pointer,
            dacLeft:
                reg(
                    UInt32(
                        FV1_SDK_REG_DACL
                    )
                ),
            dacRight:
                reg(
                    UInt32(
                        FV1_SDK_REG_DACR
                    )
                ),
            programCounter:
                raw.program_counter,
            sampleCounter:
                raw.sample_counter,
            instructionCounter:
                raw.instruction_counter,
            firstRun:
                raw.first_run != 0,
            sampleActive:
                raw.sample_active != 0,
            registers: registers,
            pots: [
                reg(
                    UInt32(
                        FV1_SDK_REG_POT0
                    )
                ),
                reg(
                    UInt32(
                        FV1_SDK_REG_POT1
                    )
                ),
                reg(
                    UInt32(
                        FV1_SDK_REG_POT2
                    )
                )
            ],
            sineLFO:
                Self.tuple2(raw.sin_lfo),
            cosineLFO:
                Self.tuple2(raw.cos_lfo),
            rampLFO:
                Self.tuple2(raw.ramp_lfo)
        )
    }

    func resources()
        throws -> FV1ResourceSummary {
        var report =
            fv1_sdk_resource_report_v1()
        fv1_sdk_resource_report_v1_init(
            &report
        )

        try Self.check(
            fv1_sdk_engine_analyze_program_v1(
                handle,
                &report
            )
        )

        return FV1ResourceSummary(
            usedInstructions:
                report.used_instructions,
            worstCasePath:
                report.worst_case_path,
            staticDelayReads:
                report.static_delay_reads,
            staticDelayWrites:
                report.static_delay_writes,
            dynamicDelayReads:
                report.dynamic_delay_reads,
            highestStaticDelayAddress:
                report.highest_static_delay_address,
            generalRegistersUsed:
                report.general_registers_used,
            potsUsed:
                report.pots_used,
            sineLFOsUsed:
                report.sine_lfos_used,
            rampLFOsUsed:
                report.ramp_lfos_used,
            skipInstructions:
                report.skip_instructions
        )
    }

    /*
     * Offline debugger execution. This FV1Engine instance is owned solely by
     * FV1WorkspaceModel's inspector and never races the realtime audio engine.
     */
    func debugStepInstruction(
        inputLeft: Float,
        inputRight: Float
    ) throws -> FV1DebugTrace {
        let before = try snapshot()

        if !before.sampleActive {
            try Self.check(
                fv1_sdk_debug_begin_sample(
                    handle,
                    inputLeft,
                    inputRight
                )
            )
        }

        var rawTrace =
            fv1_sdk_trace_v1()
        fv1_sdk_trace_v1_init(
            &rawTrace
        )

        try Self.check(
            fv1_sdk_debug_step_instruction_v1(
                handle,
                &rawTrace
            )
        )

        var outputLeft: Float?
        var outputRight: Float?

        if rawTrace.sample_finished != 0 {
            var left: Float = 0
            var right: Float = 0

            try Self.check(
                fv1_sdk_debug_finish_sample(
                    handle,
                    &left,
                    &right
                )
            )

            outputLeft = left
            outputRight = right
        }

        return Self.trace(
            rawTrace,
            outputLeft: outputLeft,
            outputRight: outputRight
        )
    }

    func debugStepSample(
        inputLeft: Float,
        inputRight: Float
    ) throws -> FV1DebugTrace {
        var state = try snapshot()

        if !state.sampleActive {
            try Self.check(
                fv1_sdk_debug_begin_sample(
                    handle,
                    inputLeft,
                    inputRight
                )
            )
        }

        var last: FV1DebugTrace?

        /*
         * A valid FV-1 program has at most 128 words. One extra iteration is
         * retained as a defensive corrupt-state tripwire.
         */
        for _ in 0...128 {
            var rawTrace =
                fv1_sdk_trace_v1()
            fv1_sdk_trace_v1_init(
                &rawTrace
            )

            try Self.check(
                fv1_sdk_debug_step_instruction_v1(
                    handle,
                    &rawTrace
                )
            )

            if rawTrace.sample_finished != 0 {
                var left: Float = 0
                var right: Float = 0

                try Self.check(
                    fv1_sdk_debug_finish_sample(
                        handle,
                        &left,
                        &right
                    )
                )

                return Self.trace(
                    rawTrace,
                    outputLeft: left,
                    outputRight: right
                )
            }

            last = Self.trace(
                rawTrace,
                outputLeft: nil,
                outputRight: nil
            )

            state = try snapshot()
            if !state.sampleActive {
                break
            }
        }

        if let last {
            throw FV1EngineError.debugger(
                "FV-1 sample did not finish within the 129-instruction debugger guard. Last PC \(last.pcAfter)."
            )
        }

        throw FV1EngineError.debugger(
            "FV-1 debugger could not execute a sample."
        )
    }

    func delayWindow(
        centeredAt center: UInt32,
        count: Int
    ) throws -> [FV1DelayWord] {
        let wordCount =
            UInt32(FV1_SDK_DELAY_WORDS)
        let boundedCount =
            max(
                1,
                min(
                    count,
                    Int(wordCount)
                )
            )

        let half =
            UInt32(boundedCount / 2)

        let start =
            (center
                &+ wordCount
                &- half)
            % wordCount

        var result: [FV1DelayWord] = []
        result.reserveCapacity(
            boundedCount
        )

        for offset in 0..<boundedCount {
            let address =
                (start
                    &+ UInt32(offset))
                % wordCount

            var value: Int32 = 0
            try Self.check(
                fv1_sdk_engine_read_delay_word(
                    handle,
                    address,
                    &value
                )
            )

            result.append(
                FV1DelayWord(
                    address: address,
                    value: value
                )
            )
        }

        return result
    }

    func registerName(
        index: UInt32
    ) -> String {
        guard let pointer =
            fv1_sdk_register_name(
                index
            ) else {
            return String(
                format: "REG%02u",
                index
            )
        }
        return String(
            cString: pointer
        )
    }

    private static func trace(
        _ raw: fv1_sdk_trace_v1,
        outputLeft: Float?,
        outputRight: Float?
    ) -> FV1DebugTrace {
        let name: String
        if let pointer =
            fv1_sdk_opcode_name(
                raw.opcode
            ) {
            name = String(
                cString: pointer
            )
        } else {
            name = "UNKNOWN"
        }

        return FV1DebugTrace(
            pcBefore:
                raw.pc_before,
            pcAfter:
                raw.pc_after,
            rawInstruction:
                raw.raw_instruction,
            opcode:
                raw.opcode,
            opcodeName: name,
            skipped:
                raw.skipped != 0,
            sampleFinished:
                raw.sample_finished != 0,
            accBefore:
                raw.acc_before,
            accAfter:
                raw.acc_after,
            paccAfter:
                raw.pacc_after,
            lrAfter:
                raw.lr_after,
            sampleIndex:
                raw.sample_index,
            instructionIndex:
                raw.instruction_index,
            outputLeft:
                outputLeft,
            outputRight:
                outputRight
        )
    }

    private static func tuple2<T>(
        _ tuple: (T, T)
    ) -> [T] {
        [tuple.0, tuple.1]
    }

    private static func check(
        _ result: fv1_sdk_result
    ) throws {
        guard result == FV1_SDK_OK else {
            let message =
                fv1_sdk_result_string(
                    result
                )
                .map {
                    String(cString: $0)
                }
                ?? "Unknown SDK error"

            throw FV1EngineError.sdk(
                result,
                message
            )
        }
    }
}
