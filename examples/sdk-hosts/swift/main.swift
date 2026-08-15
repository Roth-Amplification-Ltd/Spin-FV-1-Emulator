import FV1SDK

let source = "RDAX ADCL, 1.0\nWRAX DACL, 0\nRDAX ADCR, 1.0\nWRAX DACR, 0\n"
var program = [UInt8](repeating: 0, count: Int(FV1_SDK_PROGRAM_BYTES))
var report = fv1_sdk_compile_report_v1()
fv1_sdk_compile_report_v1_init(&report)
var diagnostic = [CChar](repeating: 0, count: 512)
let compileResult: fv1_sdk_result = source.utf8CString.withUnsafeBufferPointer { src in
    program.withUnsafeMutableBufferPointer { image in
        diagnostic.withUnsafeMutableBufferPointer { diag in
            fv1_sdk_compile_spinasm_v1(src.baseAddress, source.utf8.count,
                                       image.baseAddress, image.count,
                                       &report, diag.baseAddress, diag.count)
        }
    }
}
if compileResult != FV1_SDK_OK { fatalError("SpinASM compile failed") }

var config = fv1_sdk_engine_config_v1()
fv1_sdk_engine_config_v1_init(&config)
var engine: OpaquePointer? = nil
if fv1_sdk_engine_create_v1(&config, &engine) != FV1_SDK_OK || engine == nil { fatalError("engine creation failed") }
defer { fv1_sdk_engine_destroy(engine) }

let loadResult = program.withUnsafeBufferPointer { image in
    fv1_sdk_engine_load_program(engine, image.baseAddress, image.count)
}
if loadResult != FV1_SDK_OK { fatalError("program load failed") }
if fv1_sdk_engine_set_pot(engine, 3, 0.5) != FV1_SDK_ERROR_INVALID_ARGUMENT {
    fatalError("intentional invalid POT did not return INVALID_ARGUMENT")
}

var audio: [Float] = [0.25, -0.25, 0.125, -0.125]
let processResult = audio.withUnsafeMutableBufferPointer { buffer in
    fv1_sdk_engine_process_interleaved_f32(engine, buffer.baseAddress, buffer.baseAddress, 2)
}
if processResult != FV1_SDK_OK { fatalError("process failed") }
if abs(audio[0] - 0.25) > 0.000002 { fatalError("output mismatch") }

var snapshot = fv1_sdk_snapshot_v1()
fv1_sdk_snapshot_v1_init(&snapshot)
if fv1_sdk_engine_get_snapshot_v1(engine, &snapshot) != FV1_SDK_OK || snapshot.sample_counter != 2 {
    fatalError("snapshot failed")
}
if fv1_sdk_get_capabilities() == 0 { fatalError("capability discovery failed") }
print("Swift host OK: FV1SDK \(String(cString: fv1_sdk_get_version_string())) (\(report.instruction_count) instructions, sample \(snapshot.sample_counter))")
