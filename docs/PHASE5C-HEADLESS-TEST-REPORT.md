# Phase 5C Headless Test Report

Date: 2026-08-14

## Scope

This report covers the hardware-independent FV-1 model-hardening pass: explicit state/time,
independent reference model, differential conformance, specification-derived instruction checks,
randomized program comparison, sanitizer execution and differential fuzzing.

No physical FV-1 silicon was connected. Therefore this report demonstrates internal conformance and
software robustness only; all `SILICON-PENDING` items in the Hardware Emulation Contract remain open.

## Normal headless CTest

Result: **18/18 PASS**.

```text
fv1-core-tests                      PASS
fv1-numeric-boundary-tests          PASS
fv1-instruction-contract-tests      PASS
fv1-conformance-tests               PASS
fv1-phase2-tests                    PASS
fv1-audio-host-tests                PASS
fv1-debugger-tests                  PASS
fv1-recorder-tests                  PASS
fv1-validation-tests                PASS
compile-steal-this-bank             PASS
spinasm-diagnostics                 PASS
cli-inspect-gravity-clerk           PASS
render-steal-this-bank              PASS
phase2-render-src                   PASS
fv1-live-help                       PASS
phase5-validation-cli               PASS
phase5b-validation-pack-cli         PASS
phase5c-demo-bank-conformance       PASS
```

A Qt-enabled build is expected to add `fv1-lab-smoke`, giving 19 tests on the Linux GUI workstation.

## Differential randomized corpus

`fv1-conformance-tests` runs 48 deterministic generated programs for six samples each, alternating
`FV1_DELAY_REFERENCE_16` and `FV1_DELAY_FULL_24`. It compares production/reference state after every
executed instruction and complete state/delay digests at sample boundaries.

The aggregate corpus is required to execute every implemented raw opcode family `0x00..0x14`; the
test fails if any family receives zero execution coverage.

## Real-program differential corpus

All eight bundled Steal This DSP programs pass production/reference conformance:

- 55 Gallon Saint
- Last Known Copy
- Ghost Spring
- Gravity Clerk
- Cold Case
- Municipal Lung
- Reverse Witness
- Data Felon

These effects are broad system-level regressions, not physical-silicon golden vectors.

## Clang ASan + UBSan

The complete 18-test headless suite was executed in two CTest groups under AddressSanitizer and
UndefinedBehaviorSanitizer. Both groups passed with no sanitizer diagnostics.

## libFuzzer smoke campaign

The opt-in differential fuzzer was built with Clang/libFuzzer + ASan + UBSan and run for 1,000
mutations from a valid FV-1 seed corpus. The run completed without a crash or production/reference
divergence and expanded coverage/features during the campaign.

This is a smoke campaign, not the long-duration Phase-6 soak target.

## Acceptance

Phase 5C model-hardening infrastructure is ready for Linux workstation acceptance. The remaining work
before platform/API freeze is extended soak/fuzz/error-path testing and, when hardware exists,
physical closure of the explicitly marked silicon-pending behaviors.
