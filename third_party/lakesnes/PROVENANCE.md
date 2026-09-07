# LakeSnes reference core

Source: https://github.com/elzo-d/LakeSnes
Revision: `9db90b86e46a377609305e298dd92d71cd1d4c8a`
Retrieved: 2026-09-07
License: MIT; see LICENSE.txt.

The `snes/` directory is copied from this revision. CMake renames
`cpu_runOpcode` only while compiling `cpu.c`, allowing our host to substitute
explicitly reconstructed game code at selected program counters. The baseline
always calls the original CPU implementation. No ROM data is included here.

This pinned original core is used as a stable CPU-translation baseline. Its
repository is archived. Matching it does not independently validate its hardware
accuracy; later hardware-fidelity checks should also use bsnes or MesenCE.

Local fixes (applied equally to baseline and hybrid execution):
- `snes_other.c`: bounds-check header size exponents and use unsigned shifts.
  UBSan found an oversized shift while probing a false header in this ROM.
- `dsp.c`: replace negative signed BRR sample shifts with bounded multiplication
  and explicit rounding. UBSan found these while decoding the opening audio.
- `dsp.c`: serialize the output ring buffer and sample offset. A split/resume
  regression exposed differing PCM with otherwise identical machine state.
  `snes_other.c` uses local state version `0x445a0001`; old raw states are not
  compatible with this expanded format.
- `statehandler.c`: cast bytes to unsigned 32-bit values before shifting while
  loading 32-bit state fields. UBSan exposed signed promotion overflow when
  restoring a checkpoint; stored values and file encoding are unchanged.
