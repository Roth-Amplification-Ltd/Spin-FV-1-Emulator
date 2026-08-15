#ifndef FV1_SDK_H
#define FV1_SDK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * FV1SDK public C ABI candidate.
 *
 * Phase 6B is the stabilization/review phase. ABI major 1 is intentionally
 * being exercised from C, C++, Swift, Objective-C, Rust and Python-style FFI
 * hosts before it is declared frozen. The implementation behind these opaque
 * handles remains private C++ and may change freely.
 */
#define FV1_SDK_ABI_VERSION_MAJOR 1u
#define FV1_SDK_ABI_VERSION_MINOR 0u
#define FV1_SDK_ABI_VERSION ((FV1_SDK_ABI_VERSION_MAJOR << 16) | FV1_SDK_ABI_VERSION_MINOR)
#define FV1_SDK_PROGRAM_BYTES 512u
#define FV1_SDK_REGISTER_COUNT 64u
#define FV1_SDK_DELAY_WORDS 32768u

/* Stable architectural register indices for snapshots/debuggers. */
enum {
    FV1_SDK_REG_SIN0_RATE  = 0x00,
    FV1_SDK_REG_SIN0_RANGE = 0x01,
    FV1_SDK_REG_SIN1_RATE  = 0x02,
    FV1_SDK_REG_SIN1_RANGE = 0x03,
    FV1_SDK_REG_RMP0_RATE  = 0x04,
    FV1_SDK_REG_RMP0_RANGE = 0x05,
    FV1_SDK_REG_RMP1_RATE  = 0x06,
    FV1_SDK_REG_RMP1_RANGE = 0x07,
    FV1_SDK_REG_POT0       = 0x10,
    FV1_SDK_REG_POT1       = 0x11,
    FV1_SDK_REG_POT2       = 0x12,
    FV1_SDK_REG_ADCL       = 0x14,
    FV1_SDK_REG_ADCR       = 0x15,
    FV1_SDK_REG_DACL       = 0x16,
    FV1_SDK_REG_DACR       = 0x17,
    FV1_SDK_REG_ADDR_PTR   = 0x18,
    FV1_SDK_REG0           = 0x20,
    FV1_SDK_REG31          = 0x3f
};

#if defined(_WIN32)
#  define FV1_SDK_CALL __cdecl
#else
#  define FV1_SDK_CALL
#endif

#if defined(_WIN32) && defined(FV1_SDK_SHARED)
#  if defined(FV1_SDK_BUILDING)
#    define FV1_SDK_API __declspec(dllexport)
#  else
#    define FV1_SDK_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define FV1_SDK_API __attribute__((visibility("default")))
#else
#  define FV1_SDK_API
#endif

typedef struct fv1_sdk_engine fv1_sdk_engine;

/* Fixed-width ABI scalar types. Do not use C enum storage across the binary
 * boundary: -fshort-enums and foreign-language FFIs must not be able to change
 * the size of a public result/control value. */
typedef int32_t fv1_sdk_result;
enum {
    FV1_SDK_OK = 0,
    FV1_SDK_ERROR_INVALID_ARGUMENT = -1,
    FV1_SDK_ERROR_INVALID_PROGRAM = -2,
    FV1_SDK_ERROR_BAD_STATE = -3,
    FV1_SDK_ERROR_IO = -4,
    FV1_SDK_ERROR_UNSUPPORTED = -5,
    FV1_SDK_ERROR_OUT_OF_MEMORY = -6,
    FV1_SDK_ERROR_COMPILE = -7,
    FV1_SDK_ERROR_INTERNAL = -8
};

typedef uint32_t fv1_sdk_delay_model;
enum {
    FV1_SDK_DELAY_REFERENCE_16 = 0,
    FV1_SDK_DELAY_FULL_24 = 1
};

/* Feature discovery is intentionally a bitset rather than a growing enum API.
 * Unknown bits must be ignored by consumers. */
#define FV1_SDK_CAP_CORE_PROCESSING      UINT64_C(1) << 0
#define FV1_SDK_CAP_SPINASM_COMPILER     UINT64_C(1) << 1
#define FV1_SDK_CAP_SNAPSHOT             UINT64_C(1) << 2
#define FV1_SDK_CAP_DELAY_INSPECTION     UINT64_C(1) << 3
#define FV1_SDK_CAP_RESOURCE_ANALYSIS    UINT64_C(1) << 4
#define FV1_SDK_CAP_DEBUG_STEPPING       UINT64_C(1) << 5
#define FV1_SDK_CAP_STATE_DIGEST         UINT64_C(1) << 6
#define FV1_SDK_CAP_PLANAR_F32           UINT64_C(1) << 7
#define FV1_SDK_CAP_INTERLEAVED_F32      UINT64_C(1) << 8
#define FV1_SDK_CAP_PROGRAM_READBACK     UINT64_C(1) << 9

typedef struct fv1_sdk_version_info_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t sdk_version_major;
    uint32_t sdk_version_minor;
    uint32_t sdk_version_patch;
    uint32_t reserved0;
    uint64_t capabilities;
    uint32_t program_bytes;
    uint32_t register_count;
    uint32_t delay_words;
    uint32_t reserved[9];
} fv1_sdk_version_info_v1;

typedef struct fv1_sdk_engine_config_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    double virtual_sample_rate;
    uint32_t delay_model;
    uint32_t flags;
    uint32_t reserved[8];
} fv1_sdk_engine_config_v1;

typedef struct fv1_sdk_snapshot_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t acc;
    int32_t pacc;
    int32_t lr;
    int32_t regs[FV1_SDK_REGISTER_COUNT];
    uint32_t delay_pointer;
    int32_t sin_lfo[2];
    int32_t cos_lfo[2];
    int32_t ramp_lfo[2];
    uint32_t program_counter;
    uint8_t first_run;
    uint8_t sample_active;
    uint8_t reserved8[2];
    uint64_t sample_counter;
    uint32_t instruction_counter;
    uint32_t reserved32[7];
} fv1_sdk_snapshot_v1;

typedef struct fv1_sdk_resource_report_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t used_instructions;
    uint32_t worst_case_path;
    uint32_t static_delay_reads;
    uint32_t static_delay_writes;
    uint32_t dynamic_delay_reads;
    uint32_t highest_static_delay_address;
    uint32_t general_registers_used;
    uint32_t pots_used;
    uint32_t sine_lfos_used;
    uint32_t ramp_lfos_used;
    uint32_t skip_instructions;
    uint32_t opcode_histogram[32];
    uint32_t reserved[8];
} fv1_sdk_resource_report_v1;

typedef struct fv1_sdk_compile_report_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t instruction_count;
    uint32_t highest_delay_address;
    uint32_t error_line;
    uint32_t error_column;
    uint32_t diagnostic_bytes_required; /* includes trailing NUL when nonzero */
    uint32_t diagnostic_bytes_written;  /* excludes trailing NUL */
    uint32_t reserved[8];
} fv1_sdk_compile_report_v1;

/* Version/capability discovery. Returned strings are immutable process-lifetime
 * data owned by the SDK. */
FV1_SDK_API uint32_t FV1_SDK_CALL fv1_sdk_get_abi_version(void);
FV1_SDK_API uint64_t FV1_SDK_CALL fv1_sdk_get_capabilities(void);
FV1_SDK_API const char* FV1_SDK_CALL fv1_sdk_get_version_string(void);
FV1_SDK_API const char* FV1_SDK_CALL fv1_sdk_result_string(fv1_sdk_result result);
FV1_SDK_API void FV1_SDK_CALL fv1_sdk_version_info_v1_init(fv1_sdk_version_info_v1* info);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_get_version_info_v1(fv1_sdk_version_info_v1* info);

/* Versioned structure initialization helpers. */
FV1_SDK_API void FV1_SDK_CALL fv1_sdk_engine_config_v1_init(fv1_sdk_engine_config_v1* config);
FV1_SDK_API void FV1_SDK_CALL fv1_sdk_snapshot_v1_init(fv1_sdk_snapshot_v1* snapshot);
FV1_SDK_API void FV1_SDK_CALL fv1_sdk_resource_report_v1_init(fv1_sdk_resource_report_v1* report);
FV1_SDK_API void FV1_SDK_CALL fv1_sdk_compile_report_v1_init(fv1_sdk_compile_report_v1* report);

/* Engine lifetime. Opaque handles must be destroyed by the same SDK that
 * created them. Passing handles between different FV1SDK library instances is
 * unsupported. */
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_create_v1(const fv1_sdk_engine_config_v1* config,
                                                     fv1_sdk_engine** out_engine);
FV1_SDK_API void FV1_SDK_CALL fv1_sdk_engine_destroy(fv1_sdk_engine* engine);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_reset(fv1_sdk_engine* engine, uint32_t clear_delay_ram);

/* Program/control boundary. Program images are the standard 512-byte,
 * big-endian FV-1 EEPROM representation. */
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_load_program(fv1_sdk_engine* engine,
                                                        const uint8_t* program,
                                                        size_t program_size);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_get_program(const fv1_sdk_engine* engine,
                                                       uint8_t* output_program,
                                                       size_t output_capacity);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_set_pot(fv1_sdk_engine* engine,
                                                   uint32_t index,
                                                   float value);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_set_pots(fv1_sdk_engine* engine,
                                                    float pot0, float pot1, float pot2);

/* Realtime processing boundary.
 *
 * Once the engine has been created and a program loaded, these functions do
 * not allocate, lock, touch the filesystem, log, or call GUI facilities.
 * Input/output buffers may alias only as exact in-place left/right or exact
 * in-place interleaved buffers; partial overlap is not supported. */
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_process_sample_f32(fv1_sdk_engine* engine,
                                                              float input_left,
                                                              float input_right,
                                                              float* output_left,
                                                              float* output_right);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_process_planar_f32(fv1_sdk_engine* engine,
                                                              const float* input_left,
                                                              const float* input_right,
                                                              float* output_left,
                                                              float* output_right,
                                                              size_t frames);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_process_interleaved_f32(fv1_sdk_engine* engine,
                                                                   const float* input_stereo,
                                                                   float* output_stereo,
                                                                   size_t frames);

/* Non-mutating inspection. These are not promised realtime-safe unless the
 * individual function explicitly says so. */
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_get_snapshot_v1(const fv1_sdk_engine* engine,
                                                           fv1_sdk_snapshot_v1* snapshot);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_read_delay_word(const fv1_sdk_engine* engine,
                                                           uint32_t address,
                                                           int32_t* value);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_analyze_program_v1(const fv1_sdk_engine* engine,
                                                              fv1_sdk_resource_report_v1* report);

/* Native SpinASM compiler. This is intentionally non-realtime. It does not
 * return heap ownership across the ABI: the caller supplies the 512-byte
 * output image and optional diagnostic text buffer. */
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_compile_spinasm_v1(const char* source_utf8,
                                                       size_t source_size,
                                                       uint8_t* output_program,
                                                       size_t output_capacity,
                                                       fv1_sdk_compile_report_v1* report,
                                                       char* diagnostic_utf8,
                                                       size_t diagnostic_capacity);

#ifdef __cplusplus
}
#endif

#endif /* FV1_SDK_H */
