# FV-1 Instruction Conformance Matrix

This matrix tracks **implementation confidence**, not marketing-level silicon-equivalence. `Diff` means
the production and independent reference engines are compared instruction-by-instruction under random
and real-program vectors. `Silicon` identifies areas that still need a real chip to close ambiguity.

| Instruction/family | Production | Independent reference | Diff | Current oracle/confidence | Silicon work still useful |
|---|:---:|:---:|:---:|---|---|
| RDA | ✓ | ✓ | ✓ | documented syntax/11-bit coefficient + regression vectors | exact delay storage precision |
| RMPA | ✓ | ✓ | ✓ | project encoding + executable contract | **yes: encoding/precision audit** |
| WRA | ✓ | ✓ | ✓ | documented syntax + regression vectors | exact delay storage precision |
| WRAP | ✓ | ✓ | ✓ | documented semantics + regression vectors | exact delay storage precision |
| RDAX | ✓ | ✓ | ✓ | documented register arithmetic | boundary/rounding confirmation |
| RDFX / LDAX | ✓ | ✓ | ✓ | documented filter/load semantics | boundary/rounding confirmation |
| WRAX | ✓ | ✓ | ✓ | documented register write/scale | boundary/rounding confirmation |
| WRHX | ✓ | ✓ | ✓ | documented high-pass form | boundary/rounding confirmation |
| WRLX | ✓ | ✓ | ✓ | documented low-pass form | boundary/rounding confirmation |
| MAXX / ABSA | ✓ | ✓ | ✓ | documented absolute-value comparison | minimum-negative corner |
| MULX | ✓ | ✓ | ✓ | documented register multiply | exact rounding corner |
| LOG | ✓ | ✓ | ✓ | documented LOG2/16 behavior; current approximation | **yes: transfer/edge map** |
| EXP | ✓ | ✓ | ✓ | documented paired exponent behavior; current approximation | **yes: transfer/edge map** |
| SOF | ✓ | ✓ | ✓ | documented scale+offset | rounding/saturation edges |
| AND / CLR | ✓ | ✓ | ✓ | direct 24-bit logical semantics | low risk |
| OR | ✓ | ✓ | ✓ | direct 24-bit logical semantics | low risk |
| XOR / NOT | ✓ | ✓ | ✓ | direct 24-bit logical semantics + explicit NOT test | low risk |
| SKP / JMP / NOP | ✓ | ✓ | ✓ | conditions/forward skip + RUN tests | event-order corner confirmation |
| WLDS | ✓ | ✓ | ✓ | documented SIN generator setup | **yes: exact recurrence/update timing** |
| WLDR | ✓ | ✓ | ✓ | documented RAMP generator setup | **yes: exact recurrence/update timing** |
| JAM | ✓ | ✓ | ✓ | ramp reset semantics | exact phase/reset timing |
| CHO RDA | ✓ | ✓ | ✓ | documented role/flags + project internal split | **yes: address/fraction/interpolation corners** |
| CHO SOF | ✓ | ✓ | ✓ | documented role/flags + project internal split | **yes: coefficient/interpolation corners** |
| CHO RDAL | ✓ | ✓ | ✓ | documented LFO-read role | exact scaling/phase corners |

## Reading the matrix

Differential agreement catches implementation drift and many ordinary coding mistakes, but if both
models embody the same incorrect interpretation of an ambiguous document, both can still agree and be
wrong relative to silicon. That is why RMPA, delay precision, LOG/EXP and CHO/LFO details stay visibly
marked for later hardware closure.

Any future hardware discrepancy must become a minimal failing vector in this matrix/test hierarchy
before the production model is changed.
