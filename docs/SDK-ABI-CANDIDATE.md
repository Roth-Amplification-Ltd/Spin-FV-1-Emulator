# FV-1 SDK ABI v1 Candidate Snapshot

Status: **Phase 6B candidate — NOT FROZEN**.

This file records the concrete ABI surface under review before Phase 6C. Changes are still allowed,
but they must be deliberate and accompanied by updates to the ABI regression fixtures.

## Binary conventions

- C linkage; no exported C++ ABI.
- Windows calling convention: `__cdecl`.
- `fv1_sdk_result` is signed 32-bit; `fv1_sdk_delay_model` is unsigned 32-bit.
- Opaque engine pointer ownership remains inside the SDK.
- Supported release target architectures for the v1 freeze are 64-bit desktop targets; the C API
  remains source-portable elsewhere but the recorded structure fixture below is the 64-bit contract.
- Structure initializer helpers must be used; reserved fields remain zero.

## 64-bit structure-layout fixture

| Structure | Size | Key offset |
|---|---:|---:|
| `fv1_sdk_version_info_v1` | 80 | capabilities follows 24-byte fixed prefix |
| `fv1_sdk_engine_config_v1` | 56 | `virtual_sample_rate` = 8 |
| `fv1_sdk_snapshot_v1` | 352 | `sample_counter` = 312 |
| `fv1_sdk_resource_report_v1` | 212 | fixed candidate record |
| `fv1_sdk_compile_report_v1` | 64 | `diagnostic_bytes_required` = 24 |
| `fv1_sdk_trace_v1` | 80 | `sample_index` = 40 |
| `fv1_sdk_state_digest_v1` | 72 | fixed candidate record |

`tests/sdk/test_sdk_abi_layout.c` enforces this candidate layout and is deliberately compiled with
`-fshort-enums` on GCC/Clang to prove that public result/control widths do not depend on enum storage.

## Exported-symbol candidate

The canonical machine-readable list is `cmake/fv1-sdk-symbols.txt`. Linux shared-library tests require
the actual dynamic export set to match that manifest exactly; the macOS exported-symbol list is kept
in `cmake/fv1-sdk.exports`.

```text
fv1_sdk_compile_report_v1_init
fv1_sdk_compile_spinasm_v1
fv1_sdk_debug_begin_sample
fv1_sdk_debug_finish_sample
fv1_sdk_debug_step_instruction_v1
fv1_sdk_engine_analyze_program_v1
fv1_sdk_engine_config_v1_init
fv1_sdk_engine_create_v1
fv1_sdk_engine_destroy
fv1_sdk_engine_get_program
fv1_sdk_engine_get_snapshot_v1
fv1_sdk_engine_get_state_digest_v1
fv1_sdk_engine_load_program
fv1_sdk_engine_process_interleaved_f32
fv1_sdk_engine_process_planar_f32
fv1_sdk_engine_process_sample_f32
fv1_sdk_engine_read_delay_word
fv1_sdk_engine_reset
fv1_sdk_engine_set_pot
fv1_sdk_engine_set_pots
fv1_sdk_get_abi_version
fv1_sdk_get_capabilities
fv1_sdk_get_version_info_v1
fv1_sdk_get_version_string
fv1_sdk_opcode_name
fv1_sdk_register_name
fv1_sdk_resource_report_v1_init
fv1_sdk_result_string
fv1_sdk_snapshot_v1_init
fv1_sdk_state_digest_v1_init
fv1_sdk_trace_v1_init
fv1_sdk_version_info_v1_init
```

## Evolution rule

After freeze, existing symbols, calling conventions, numeric result values, record sizes/offsets,
ownership rules, realtime guarantees, and semantic meanings may not be changed within ABI major 1.
New functionality should be additive through new symbols/capability bits, unused reserved fields where
semantically safe, or explicitly versioned new record/function entry points.
