# Progress — 7 September 2026

Target: a faithful, portable reconstruction of Japanese Rev 1 game logic in C,
with a native Mac application first. The current executable is a hybrid:
compiled replacements run alongside an interpreter for unrecovered code.

## Reconstructed and verified

| Original region | Behavior | ROM bytes | Instruction sites |
|---|---|---:|---:|
| 00:8000–800D | Reset prefix | 14 | 9 |
| 00:828F–82A2 | Forced blank on/off | 20 | 10 |
| 00:82A3–82C6 | Two RNG streams | 36 | 22 |
| 00:82D4–82FA | Both controller ports and button transitions | 39 | 20 |
| 00:82FB–8323 | Four background scroll words | 41 | 17 |
| 00:8324–83CB | Frame graphics DMA uploads | 168 | 67 |
| 00:86A0–86B5 | Hide unused sprites | 22 | 12 |
| 00:86B6–86C7 | Clear palette shadow | 18 | 9 |
| 00:8282–828E | Disable selected interrupts/display control | 13 | 7 |
| 00:8460–8470 | Update brightness/timing display state | 17 | 7 |
| 00:83E4–8401 | Initialize display-transition state | 30 | 13 |
| 00:8433–845F | Finalize display transition state | 45 | 19 |
| 00:83CC–83DB | Dispatch display-transition states | 16 | 8 |
| 00:8471–849B | Update mosaic/window register state | 43 | 22 |
| **Total** | **Fifteen complete routines plus a reset prefix** | **522** | **242** |

These are original code-region sizes, not C source sizes. Unsupported CPU modes
and interrupts fall back to the baseline. The program accepts only the pinned
ROM hash. Reconstructed routines preserve bus accesses and interrupt sampling
at instruction boundaries, allowing mixed execution while larger subsystems
are recovered.

The desktop host supplies windowing, keyboard input, sound, battery saves,
deterministic input replays, frame captures, and per-frame differential checks.
The Mac package includes SDL and license notices, without a ROM or game assets.
The current package targets Apple Silicon/macOS 26.0; other builds remain untested.

## Validation evidence

- Release and ASan/UBSan isolated tests pass: 131,072 RNG cases, 1,024 display
  cases, 32 reset cases, 262,144 controller cases, and 512 scroll cases.
  Each compares CPU state and ordered bus events against actual ROM instructions.
- `artifacts/controller-replay/report.json`: 3,000 frames of the scripted opening
  and new-game flow pass serialized-state, framebuffer, and PCM comparison.
  Final scene is the Kame House overworld with the action menu open.
- That replay executes 15,763,010 native and 23,408,706 interpreted steps;
  controller polling contributes 53,460 native steps and scrolling 45,475.
- `artifacts/sanitize-controller/report.json`: 300 boot frames pass differential
  verification under address/undefined-behavior sanitizers.
- `artifacts/package-manifest.json`: hashes of packaged files. Packaging also
  verifies the ad-hoc signature and checks SDL's dependencies.
- `artifacts/packaged-replay/`: the bundled executable ran 3,000 frames with a
  real SDL window and audio output enabled. Differential checks pass; all frame
  trace records, final machine state, and final framebuffer are byte-identical
  to the headless replay. Replay input is now exclusive so live keyboard buttons
  cannot contaminate a recording. This verifies audio submission, not acoustic
  output quality.

Runtime artifacts are ignored by Git and can be regenerated using the commands
in `prototype.md`. The reference instance shares the same hardware core; an
independent hardware-fidelity comparison with another emulator is still needed.

## How far from complete?

The project now has a reproducible scoped tracker. The pinned static-analysis
worklist contains 13,237 instruction variants. Of those, 180 instruction sites
are covered by reviewed C replacements: **1.36% of the discovered worklist**.
Run `python3 tools/progress.py` to recalculate this figure. If the local
SNESRecomp manifest exists, the script sums it directly; a public clone uses the
same checked-in probe total.

This is deliberately not whole-game completion: static discovery found 388
function variants and 375 distinct entry addresses, but it did not prove that
the ROM's complete code/data map was found. The missing code is therefore absent
from the denominator. About 40.24% of instruction executions in the long replay
are native, mostly because the RNG routine runs frequently; that is workload
coverage, not source completion.

The tracker is a better project gauge than the former “under 1%” planning guess,
while still keeping the denominator limitation visible. A whole-game percentage
will become defensible only after a complete code/data map and coverage inventory
exist.

Completion means all game logic reconstructed, complete-game behavioral
verification (including battles, progression, menus, saves and endings), no
fallback interpreter needed for game CPU code, and reproducible platform builds.
Hardware support for rendering/audio may remain an explicit compatibility layer.

## Next milestones

1. Recover the rest of NMI/frame updates and initialization; name RAM variables
   and maintain the original address mapping as evidence.
2. Extend deterministic replays through movement, battles, card selection,
   progression, and save/reload; add reusable state checkpoints.
3. Recover gameplay subsystems as larger, readable C functions, keeping the
   differential harness as the correctness gate.
4. Compare against an independent emulator and package/test additional systems.

The earlier SNESRecomp experiment is retained as research. Its generated archive
is not linked into the active runtime.

## Checkpoint and flight milestone

The host now exports versioned `final.dbzstate` checkpoints and accepts
`--load-checkpoint`. Checkpoints carry the pinned ROM identity, a payload checksum,
and exact audio sample phase. Replay frame numbers restart at 1 after restore.
The state format now includes DSP output history: split-run testing found that
the upstream core omitted it, causing later PCM differences. A sanitizer also
found signed shift overflow in state loading; the byte assembly now uses unsigned
32-bit arithmetic. Both fixes are documented in the vendored provenance record.

Release and sanitizer checkpoint tests pass the 211+149 frame split against an
uninterrupted 360-frame run, comparing every state/video/audio hash. Seven corrupt
or incompatible checkpoint variants exit cleanly with an error. The packaged
executable also passes this checkpoint test (`artifacts/package-checkpoint-test.log`).

Gameplay evidence now includes takeoff from Kame House and northward flight:
`artifacts/kame-flight/` contains 600 resumed frames matching an uninterrupted
3,600-frame run in every state/video/audio hash. Its black final frame is part
of the transition; another 1,200 verified frames reach an event dialogue, and
900 dialogue-advance frames return to the flying overworld menu. These are
gameplay coverage additions, not a verified battle or a new translated subsystem.
`tests/flight-event.inputs` records the combined 5,700-frame route.
The full uninterrupted route also passes differential verification in
`artifacts/flight-event-full/`; its final checkpoint and all final-stage
state/video/audio hashes match the three-stage checkpoint chain exactly.

At that checkpoint milestone the code inventory was 150 ROM bytes / 78 instruction sites.
Next work is a reproducible actual battle and larger game-logic translations.

## Frame graphics reconstruction milestone

`src/native_video.inc` reconstructs the complete frame graphics upload routine,
unused-sprite hiding, and palette-shadow clearing. The new `ram-map.md` records
the verified shadow buffers, flags, and eight-byte VRAM queue entry layout.
The current inventory is 522 ROM bytes / 242 instruction sites.

The isolated suite now covers 398,914 cases, including 1,024 graphics-upload
variants and 513 cases each for sprite cleanup and palette clearing, plus 512
display-control, 512 transition-initializer, 512 transition-finalizer, 32 dispatch, and 512 mosaic cases. The upload
tests vary queue lengths, palette dirtiness, HDMA control, direct-page alignment,
and ROM/data-bank mirrors, comparing every CPU field and ordered bus transaction.
Release and ASan/UBSan suites pass, including checkpoint equivalence.

`artifacts/video-pipeline/` verifies the 5,700-frame flight route. Every machine
state, framebuffer, and PCM hash matches the pre-translation run as well as the
concurrent reference instance. It executes 190,320 upload steps, 4,273,276 sprite
cleanup steps, and 2,568 palette-clear steps natively. This adds real reconstructed
behavior; its frequency is still not an overall completion percentage.

The updated package passes a 600-frame checkpoint-restored takeoff replay in
`artifacts/packaged-video/`, ending at the identical checkpoint as the old build.
The first actual battle remains uncovered; `research/battle-route.md` records
sourced navigation guidance for the next replay.
