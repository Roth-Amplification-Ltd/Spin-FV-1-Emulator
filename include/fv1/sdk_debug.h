#ifndef FV1_SDK_DEBUG_H
#define FV1_SDK_DEBUG_H

#include <fv1/sdk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional debugger/introspection module. It operates on the same opaque
 * fv1_sdk_engine handle as the core API but is separated so small realtime
 * hosts do not need to conceptually depend on debugger features. */

typedef struct fv1_sdk_trace_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t pc_before;
    uint32_t pc_after;
    uint32_t raw_instruction;
    uint8_t opcode;
    uint8_t skipped;
    uint8_t sample_finished;
    uint8_t reserved8;
    int32_t acc_before;
    int32_t acc_after;
    int32_t pacc_after;
    int32_t lr_after;
    uint64_t sample_index;
    uint32_t instruction_index;
    uint32_t reserved[7];
} fv1_sdk_trace_v1;

typedef struct fv1_sdk_state_digest_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t architectural_hash;
    uint64_t delay_hash;
    uint64_t sample_counter;
    uint64_t reserved[5];
} fv1_sdk_state_digest_v1;

FV1_SDK_API void FV1_SDK_CALL fv1_sdk_trace_v1_init(fv1_sdk_trace_v1* trace);
FV1_SDK_API void FV1_SDK_CALL fv1_sdk_state_digest_v1_init(fv1_sdk_state_digest_v1* digest);

/* Instruction stepping is a debug/offline operation, not a realtime callback
 * API. Begin one virtual sample, step until sample_finished is set, then finish
 * the sample to retrieve DACL/DACR. */
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_debug_begin_sample(fv1_sdk_engine* engine,
                                                       float input_left,
                                                       float input_right);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_debug_step_instruction_v1(fv1_sdk_engine* engine,
                                                              fv1_sdk_trace_v1* trace);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_debug_finish_sample(fv1_sdk_engine* engine,
                                                        float* output_left,
                                                        float* output_right);
FV1_SDK_API fv1_sdk_result FV1_SDK_CALL fv1_sdk_engine_get_state_digest_v1(const fv1_sdk_engine* engine,
                                                               fv1_sdk_state_digest_v1* digest);
FV1_SDK_API const char* FV1_SDK_CALL fv1_sdk_opcode_name(uint8_t opcode);
FV1_SDK_API const char* FV1_SDK_CALL fv1_sdk_register_name(uint32_t register_index);

#ifdef __cplusplus
}
#endif

#endif /* FV1_SDK_DEBUG_H */
