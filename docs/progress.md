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
| 00:8402–8432 | Transition timing state updates | 49 | 25 |
| 00:89AC–89B7 | Initialize WRAM cursor state | 12 | 5 |
| 00:8AF1–8AFB | Add 0x20 to Y coordinate | 11 | 7 |
| 00:8AFC–8B06 | Add 0x20 to X coordinate | 11 | 7 |
| 00:8B07–8B0D | Increment Y six times | 7 | 7 |
| 00:8B0E–8B14 | Increment X six times | 7 | 7 |
| 00:8275–8281 | Enable NMI/auto-joypad interrupt bits | 13 | 7 |
| 00:86FC–8710 | Mode-7 hardware multiply readback | 21 | 9 |
| 00:8711–8723 | Find free VRAM DMA queue entry | 19 | 13 |
| 00:8724–8731 | Clear next VRAM queue occupied flag | 14 | 12 |
| 00:8887–88AD | Program DMA1 VRAM transfer from DP+$02 | 39 | 16 |
| 00:88C5–88D7 | Copy long-pointer words into palette shadow | 19 | 11 |
| 00:88EA–8907 | Resolve 3-byte pointer table; set palette dirty | 30 | 18 |
| 00:8AD6–8AE4 | Actor-type table lookup into $0B01,Y | 15 | 7 |
| 00:8B15–8B26 | Clear $0D00 stride-4 control bytes | 18 | 10 |
| 00:88D8–88E9 | Preset pointer $06F945; resolve via 88EA | 18 | 8 |
| 00:88AE–88C4 | Preset pointer $06F876; indices; fall into 88C5 | 23 | 9 |
| 00:8908–8920 | Copy $09D200 words into palette shadow $02C0 | 25 | 11 |
| 00:86C8–86FB | Actor attribute nibble into $0D85 | 52 | 26 |
| 00:8732–877D | CPU word copy from [DP+$00] into VRAM | 76 | 37 |
| 00:8921–8953 | Dual palette load via 88D8 into $0300 | 51 | 23 |
| 00:8B28–8B53 | HDMA-mask clear, wipe $0800 queue, scene dispatch | 44 | 20 |
| 00:8996–89AB | Fill $7E4800 with $FFFF; clear $0D7D | 22 | 10 |
| 00:9B36–9B78 | VRAM queue enqueue from [DP+$04] descriptor | 67 | 29 |
| 00:9B79–9BBE | VRAM queue enqueue from DP+$00/$02; advance $0713 | 70 | 29 |
| 00:8954–8995 | Palette index load via $06F945 / 8AF1 into $0300 | 66 | 37 |
| 00:8E26–8E75 | Scene/actor pointer build into $0702–$0706 | 80 | 39 |
| 00:8DFE–8E25 | Scene setup chaining 88AE / 8921 / 8908 / 86C8 | 40 | 13 |
| 00:8D2B–8D4D | Hardware multiply ×5 then dual table lookup | 35 | 17 |
| 00:8E76–8E93 | Scene-mode[1] scroll snapshot; call 8D2B; JMP 8B7A | 32 | 11 |
| 00:8B7A–8C83 | Scene-mode[0]: JSLs, specialty, BMI paths through $8C6D into $C559/$C68C/$90E6 | 266 | 91 |
| 00:8E96–8F45 | Scene-mode[2]: HDMA/PPU/$8887 then $01E7 pointer select into $C559/$C68C/$90E6 | 176 | 68 |
| 00:90C1–90E5 | Scene graphics: decompress, rearrange, DMA, palette | 37 | 13 |
| 00:90E6–9101 | DMA0 from $7E9000 using length DP+$89 | 28 | 11 |
| 00:946F–9484 | Palette words from $08DD44 via 88C5 | 22 | 9 |
| 00:C559–C5EC | Bit-packed decompress into $7E9000 | 148 | 83 |
| 00:C68C–C706 | 16-byte tile rearrange via DP scratch + MVN | 123 | 57 |
| 04:85B6–85F0 | Eight-slot ID allocator / clearer at $1309 | 59 | 33 |
| 02:D812–D843 | Conditional scroll snapshot into $01C0–$01CE | 50 | 20 |
| 00:9BBF–9C4A | Tiled VRAM queue enqueue from [DP+$00] via $0A00 | 140 | 70 |
| 06:F14D–F1EB | Scroll clamp from $0702/$0703; loop $06EF11 + $8324 | 159 | 78 |
| 00:89B8–8A06 | Actor-field accumulate from [DP+$00],Y into $0B07,X | 79 | 40 |
| 06:EF11–EF9A | VRAM queue fill from scroll; loop $06F089 ×32 | 138 | 66 |
| 06:F082–F14C | Tile-word fetch ($F089) with $F082 bounds early-exit | 203 | 107 |
| 03:FB8D–FBD9 | Scene-entry early-exit / dispatch (enables bank-3 map) | 77 | 34 |
| 03:FBDA–FC32 | Scene-entry deeper mode-0/2 handlers through RTL | 89 | 36 |
| 00:849C–8586 (+85B9/85CF) | Actor→OAM writeback + alt entries | 285 | 156 |
| 00:85E9–869D (+85EF) | Parallel screen-space actor→OAM writeback | 181 | 102 |
| 00:9485–94E6 | Mode-0 PPU register preset; $94B2 clears scrolls/windows then RTS | 98 | 38 |
| 00:94E7–9518 | Actor-slot clear loop ($01BB<<5 stride) unless type $54 | 50 | 27 |
| 03:FC33–FC73 | Specialty prologue ($1661/$1648/$15FA/PLA) falling into HDMA/flag/JSL body | 65 | 25 |
| 03:FC74–FC92 | Scene-entry predicate CLC/SEC RTL on $1648/$01D8/$15FA | 31 | 16 |
| 03:E340–E36A | Early gate + continue: JSL $E837/$E419/$E4D1; seed $0EA3/$0EA0; clear $01E7 bit2 | 43 | 16 |
| 03:A6CB–A6E0 | If $01D6==$60 clear it, JSL $E419/$E4F4, clear $01BC; RTL | 22 | 8 |
| 03:9255–9266 | Clear $1000 stride-6 via $8B07 until Y==$00C0; RTL | 18 | 7 |
| 06:E837–E8ED | Scene wipe: STZ cascade, actor/slot clear loops, JSL $EF29; RTL | 183 | 66 |
| 03:EF29–EFD6 | Actor backup/restore: gate, weak-field clear, $EFA5/$EFC4 helpers, MVN $7E4D00↔$0B00 | 174 | 78 |
| 03:E419–E460 | Walk actors; $E42A MVN slot→$7E2800+type<<5 | 72 | 39 |
| 03:E461–E48F | Restore one slot from $7E2800+type<<5 via MVN $00,$7E | 47 | 31 |
| 03:E4D1–E513 | Collect high-actor types into $0190; restore via $E461 | 67 | 27 |
| 06:E942–E968 | Wipe gate: $0D9B/$01A0/$0170/$01BB predicates then JSL $03BB46 or RTL | 39 | 16 |
| 03:E32E–E33F | Swap $01BC↔$018F via DP+$00; SEC/RTL (epilogue of $E36B) | 18 | 8 |
| 03:E36B–E418 | MVN $0B00↔$7E3A00/$7E29A0 and $0E00←$7E2B00; long-abs restore; JMP $E32E | 174 | 58 |
| 06:E8EE–E941 | Actor attr fill via ×$0E tables $02B21D/$02B214; type $54 / $8A28 paths | 84 | 36 |
| 03:BB46–BB86 | Wipe-actor seed at Y ($0B00=$C0, type $54, coords) | 65 | 25 |
| 03:EFD7–F020 | Clear siblings after $EFC4: wipe $7E29A0/$7E3A00/$0B00/$0E00 | 74 | 36 |
| **Total** | **Eighty-two complete routines plus a reset prefix** | **5092** | **2345** |

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
worklist contains 13,237 instruction variants. Of those, 2345 instruction sites
are covered by reviewed C replacements: **17.72% of the discovered worklist**.
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
The current inventory is 5092 ROM bytes / 2345 instruction sites.

The isolated suite now also covers interrupt-enable, VRAM queue find/mark,
PPU multiply, DMA1 setup, pointer resolve, palette-shadow copy, actor-table
lookup, and $0D00 clear helpers, in addition to the prior RNG/controller/scroll/
graphics/display-transition cases. The upload
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
