# Reference Notes

The Phase-1 implementation is written against publicly documented FV-1 behavior and tested against independent open-source implementations/program corpora. External emulator source is used as behavioral reference material, not copied into `libfv1-core`.

Primary architecture references:

- Spin Semiconductor FV-1 Architecture Overview: https://www.spinsemi.com/knowledge_base/arch.html
- Spin Semiconductor FV-1 Instructions and Syntax: https://www.spinsemi.com/knowledge_base/inst_syntax.html
- Spin Semiconductor Programming the FV-1: https://www.spinsemi.com/knowledge_base/pgm_quick.html
- Spin Semiconductor Demo Board / crystal-rate notes: https://www.spinsemi.com/knowledge_base/demo_board.html

Useful independent comparison implementations:

- eh2k/fv1-emu
- eh2k/vcvrack-fv1-emu
- p-kai-n/spnsim
- audiofab/fv1-vscode simulator/debugger

Fidelity differences are tracked explicitly in the README instead of assuming any one software implementation is authoritative over hardware.
