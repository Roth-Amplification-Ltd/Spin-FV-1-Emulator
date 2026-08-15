#include <fv1/sdk.h>
#include <fv1/sdk_debug.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

_Static_assert(sizeof(fv1_sdk_result) == 4, "result ABI scalar changed");
_Static_assert(sizeof(fv1_sdk_delay_model) == 4, "delay-model ABI scalar changed");
_Static_assert(offsetof(fv1_sdk_engine_config_v1, virtual_sample_rate) == 8, "config layout changed");
_Static_assert(offsetof(fv1_sdk_compile_report_v1, diagnostic_bytes_required) == 24, "compile-report layout changed");
_Static_assert(offsetof(fv1_sdk_trace_v1, sample_index) == 40, "trace layout changed");

int main(void) {
#if UINTPTR_MAX == UINT64_MAX
    /* ABI-v1 candidate layout on the 64-bit targets used by Linux, modern
       Windows and modern macOS. These values are deliberately regression
       tested before the freeze so compiler/packing changes cannot slip in. */
    if (sizeof(fv1_sdk_version_info_v1) != 80u ||
        sizeof(fv1_sdk_engine_config_v1) != 56u ||
        sizeof(fv1_sdk_snapshot_v1) != 352u ||
        sizeof(fv1_sdk_resource_report_v1) != 212u ||
        sizeof(fv1_sdk_compile_report_v1) != 64u ||
        sizeof(fv1_sdk_trace_v1) != 80u ||
        sizeof(fv1_sdk_state_digest_v1) != 72u) {
        fprintf(stderr, "FV1SDK ABI-v1 candidate structure layout changed\n");
        return 1;
    }
#endif
    printf("FV1SDK ABI layout OK: result=%zu config=%zu snapshot=%zu trace=%zu\n",
           sizeof(fv1_sdk_result), sizeof(fv1_sdk_engine_config_v1),
           sizeof(fv1_sdk_snapshot_v1), sizeof(fv1_sdk_trace_v1));
    return 0;
}
