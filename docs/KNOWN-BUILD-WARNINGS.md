# Known external build/packaging warnings

Project compiler warnings are release failures. Linux product and Phase 6C
configure with `FV1_WARNINGS_AS_ERRORS=ON`.

The packaging warning auditor permits only these external diagnostics:

- linuxdeploy may fail to locate Debian copyright metadata for some runtime
  libraries or staged files when resolving package ownership.
- linuxdeploy-plugin-qt may report that no Qt translation directory exists.
  FV-1 Lab currently ships no localized Qt translation pack.
- `objdump` may report that the installed SpeexDSP separate debug-info CRC does
  not match. This is a host debug-package mismatch, not an FV-1 Lab binary
  warning.
- linuxdeploy reports that `NO_STRIP=1` is active. FV-1 Lab sets this
  intentionally because linuxdeploy otherwise emits repeated RPATH/strip
  warnings.

The AppImage VERSION deprecation and missing-AppStream warnings are deliberately
not allowlisted. `linux.sh` sets `LINUXDEPLOY_OUTPUT_VERSION` and
`LDAI_NO_APPSTREAM=1`; if either warning returns, the audit should fail.
