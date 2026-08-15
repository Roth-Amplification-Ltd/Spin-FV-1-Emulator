use std::ffi::{c_char, c_void, CStr};

const FV1_SDK_OK: i32 = 0;
const FV1_SDK_ERROR_INVALID_ARGUMENT: i32 = -1;
const FV1_SDK_PROGRAM_BYTES: usize = 512;

#[repr(C)]
struct Config {
    struct_size: u32,
    abi_version: u32,
    virtual_sample_rate: f64,
    delay_model: u32,
    flags: u32,
    reserved: [u32; 8],
}

#[repr(C)]
struct Snapshot {
    struct_size: u32,
    abi_version: u32,
    acc: i32,
    pacc: i32,
    lr: i32,
    regs: [i32; 64],
    delay_pointer: u32,
    sin_lfo: [i32; 2],
    cos_lfo: [i32; 2],
    ramp_lfo: [i32; 2],
    program_counter: u32,
    first_run: u8,
    sample_active: u8,
    reserved8: [u8; 2],
    sample_counter: u64,
    instruction_counter: u32,
    reserved32: [u32; 7],
}

#[link(name = "fv1-sdk")]
extern "C" {
    fn fv1_sdk_get_version_string() -> *const c_char;
    fn fv1_sdk_get_capabilities() -> u64;
    fn fv1_sdk_engine_config_v1_init(config: *mut Config);
    fn fv1_sdk_compile_spinasm_v1(source: *const c_char, source_size: usize,
        output: *mut u8, output_capacity: usize, report: *mut c_void,
        diagnostic: *mut c_char, diagnostic_capacity: usize) -> i32;
    fn fv1_sdk_engine_create_v1(config: *const Config, engine: *mut *mut c_void) -> i32;
    fn fv1_sdk_engine_destroy(engine: *mut c_void);
    fn fv1_sdk_engine_load_program(engine: *mut c_void, program: *const u8, size: usize) -> i32;
    fn fv1_sdk_engine_set_pot(engine: *mut c_void, index: u32, value: f32) -> i32;
    fn fv1_sdk_engine_process_interleaved_f32(engine: *mut c_void, input: *const f32, output: *mut f32, frames: usize) -> i32;
    fn fv1_sdk_snapshot_v1_init(snapshot: *mut Snapshot);
    fn fv1_sdk_engine_get_snapshot_v1(engine: *const c_void, snapshot: *mut Snapshot) -> i32;
}

fn main() {
    let source = b"RDAX ADCL, 1.0\nWRAX DACL, 0\nRDAX ADCR, 1.0\nWRAX DACR, 0\n";
    let mut program = [0u8; FV1_SDK_PROGRAM_BYTES];
    let mut diagnostic = [0i8; 512];
    let compile = unsafe { fv1_sdk_compile_spinasm_v1(source.as_ptr() as *const c_char, source.len(), program.as_mut_ptr(), program.len(), std::ptr::null_mut(), diagnostic.as_mut_ptr(), diagnostic.len()) };
    assert_eq!(compile, FV1_SDK_OK);

    let mut config: Config = unsafe { std::mem::zeroed() };
    unsafe { fv1_sdk_engine_config_v1_init(&mut config) };
    let mut engine: *mut c_void = std::ptr::null_mut();
    assert_eq!(unsafe { fv1_sdk_engine_create_v1(&config, &mut engine) }, FV1_SDK_OK);
    assert!(!engine.is_null());
    assert_eq!(unsafe { fv1_sdk_engine_load_program(engine, program.as_ptr(), program.len()) }, FV1_SDK_OK);
    assert_eq!(unsafe { fv1_sdk_engine_set_pot(engine, 3, 0.5) }, FV1_SDK_ERROR_INVALID_ARGUMENT);
    let mut audio = [0.25f32, -0.25, 0.125, -0.125];
    assert_eq!(unsafe { fv1_sdk_engine_process_interleaved_f32(engine, audio.as_ptr(), audio.as_mut_ptr(), 2) }, FV1_SDK_OK);
    assert!((audio[0] - 0.25).abs() < 2e-6);

    let mut snapshot: Snapshot = unsafe { std::mem::zeroed() };
    unsafe { fv1_sdk_snapshot_v1_init(&mut snapshot) };
    assert_eq!(unsafe { fv1_sdk_engine_get_snapshot_v1(engine, &mut snapshot) }, FV1_SDK_OK);
    assert_eq!(snapshot.sample_counter, 2);
    assert_ne!(unsafe { fv1_sdk_get_capabilities() }, 0);

    let version = unsafe { CStr::from_ptr(fv1_sdk_get_version_string()) }.to_string_lossy();
    println!("Rust FFI host OK: FV1SDK {}, sample {}", version, snapshot.sample_counter);
    unsafe { fv1_sdk_engine_destroy(engine) };
}
