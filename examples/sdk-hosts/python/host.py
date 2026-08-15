#!/usr/bin/env python3
import ctypes
import os
import sys

dll_dir_cookie = None
if os.name == "nt" and os.environ.get("FV1_SDK_DLL_DIR") and hasattr(os, "add_dll_directory"):
    dll_dir_cookie = os.add_dll_directory(os.environ["FV1_SDK_DLL_DIR"])
lib = ctypes.CDLL(os.environ["FV1_SDK_LIBRARY"])

FV1_SDK_OK = 0
FV1_SDK_PROGRAM_BYTES = 512

class Config(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("virtual_sample_rate", ctypes.c_double),
        ("delay_model", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32 * 8),
    ]

class CompileReport(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("instruction_count", ctypes.c_uint32),
        ("highest_delay_address", ctypes.c_uint32),
        ("error_line", ctypes.c_uint32),
        ("error_column", ctypes.c_uint32),
        ("diagnostic_bytes_required", ctypes.c_uint32),
        ("diagnostic_bytes_written", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32 * 8),
    ]

class Snapshot(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32), ("abi_version", ctypes.c_uint32),
        ("acc", ctypes.c_int32), ("pacc", ctypes.c_int32), ("lr", ctypes.c_int32),
        ("regs", ctypes.c_int32 * 64), ("delay_pointer", ctypes.c_uint32),
        ("sin_lfo", ctypes.c_int32 * 2), ("cos_lfo", ctypes.c_int32 * 2),
        ("ramp_lfo", ctypes.c_int32 * 2), ("program_counter", ctypes.c_uint32),
        ("first_run", ctypes.c_uint8), ("sample_active", ctypes.c_uint8),
        ("reserved8", ctypes.c_uint8 * 2), ("sample_counter", ctypes.c_uint64),
        ("instruction_counter", ctypes.c_uint32), ("reserved32", ctypes.c_uint32 * 7),
    ]

lib.fv1_sdk_get_capabilities.restype = ctypes.c_uint64
lib.fv1_sdk_get_version_string.restype = ctypes.c_char_p
lib.fv1_sdk_engine_config_v1_init.argtypes = [ctypes.POINTER(Config)]
lib.fv1_sdk_compile_report_v1_init.argtypes = [ctypes.POINTER(CompileReport)]
lib.fv1_sdk_compile_spinasm_v1.argtypes = [
    ctypes.c_char_p, ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
    ctypes.POINTER(CompileReport), ctypes.c_char_p, ctypes.c_size_t]
lib.fv1_sdk_compile_spinasm_v1.restype = ctypes.c_int
lib.fv1_sdk_engine_create_v1.argtypes = [ctypes.POINTER(Config), ctypes.POINTER(ctypes.c_void_p)]
lib.fv1_sdk_engine_create_v1.restype = ctypes.c_int
lib.fv1_sdk_engine_destroy.argtypes = [ctypes.c_void_p]
lib.fv1_sdk_engine_load_program.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
lib.fv1_sdk_engine_load_program.restype = ctypes.c_int
lib.fv1_sdk_engine_set_pot.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_float]
lib.fv1_sdk_engine_set_pot.restype = ctypes.c_int
lib.fv1_sdk_snapshot_v1_init.argtypes = [ctypes.POINTER(Snapshot)]
lib.fv1_sdk_engine_get_snapshot_v1.argtypes = [ctypes.c_void_p, ctypes.POINTER(Snapshot)]
lib.fv1_sdk_engine_get_snapshot_v1.restype = ctypes.c_int
lib.fv1_sdk_engine_process_interleaved_f32.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float), ctypes.c_size_t]
lib.fv1_sdk_engine_process_interleaved_f32.restype = ctypes.c_int

source = b"RDAX ADCL, 1.0\nWRAX DACL, 0\nRDAX ADCR, 1.0\nWRAX DACR, 0\n"
program = (ctypes.c_uint8 * FV1_SDK_PROGRAM_BYTES)()
report = CompileReport()
lib.fv1_sdk_compile_report_v1_init(ctypes.byref(report))
diag = ctypes.create_string_buffer(512)
if lib.fv1_sdk_compile_spinasm_v1(source, len(source), program, len(program), ctypes.byref(report), diag, len(diag)) != FV1_SDK_OK:
    raise SystemExit(f"compile failed: {diag.value.decode()}")

cfg = Config()
lib.fv1_sdk_engine_config_v1_init(ctypes.byref(cfg))
engine = ctypes.c_void_p()
if lib.fv1_sdk_engine_create_v1(ctypes.byref(cfg), ctypes.byref(engine)) != FV1_SDK_OK:
    raise SystemExit("create failed")
try:
    if lib.fv1_sdk_engine_load_program(engine, program, len(program)) != FV1_SDK_OK:
        raise SystemExit("load failed")
    if lib.fv1_sdk_engine_set_pot(engine, 3, 0.5) != -1:
        raise SystemExit("intentional invalid POT did not return INVALID_ARGUMENT")
    audio = (ctypes.c_float * 4)(0.25, -0.25, 0.125, -0.125)
    if lib.fv1_sdk_engine_process_interleaved_f32(engine, audio, audio, 2) != FV1_SDK_OK:
        raise SystemExit("process failed")
    if abs(audio[0] - 0.25) > 2e-6 or abs(audio[1] + 0.25) > 2e-6:
        raise SystemExit("output mismatch")
    snap = Snapshot()
    lib.fv1_sdk_snapshot_v1_init(ctypes.byref(snap))
    if lib.fv1_sdk_engine_get_snapshot_v1(engine, ctypes.byref(snap)) != FV1_SDK_OK or snap.sample_counter != 2:
        raise SystemExit("snapshot failed")
    caps = lib.fv1_sdk_get_capabilities()
    if caps == 0:
        raise SystemExit("capability discovery failed")
    print(f"Python ctypes host OK: FV1SDK {lib.fv1_sdk_get_version_string().decode()} ({report.instruction_count} instructions, sample {snap.sample_counter})")
finally:
    lib.fv1_sdk_engine_destroy(engine)
