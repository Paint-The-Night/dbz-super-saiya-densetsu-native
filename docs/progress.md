# Progress — 8 September 2026

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
| 00:8DFA–8E25 | JSL $8E26 trampoline + scene setup chaining 88AE / 8921 / 8908 / 86C8 | 44 | 14 |
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
| 00:8DAC–8DE3 | Actor-slot walk: type×3, $8DE4 word, JSL $01C9F9/$8324/$8AF1 | 56 | 30 |
| 06:EAD4–EB51 | Palette-index select → $88EA/$88C5 (alt $06F876 when idx==3) | 126 | 54 |
| 06:EB52–EBC0 | Gfx-index select → stream $02E489, C559 decompress, 90E6 DMA | 111 | 50 |
| 00:8F46–8FB1 | Scene-mode body after $90C1: EAD4/EB52, PPU seeds, $0B12 clear | 108 | 39 |
| 00:8C84–8D2A | Post-$8C6D mode[0] continuation through $8DAC/$85B6 into RTL | 167 | 63 |
| 00:8D4E–8DAB | Specialty select: $1303 sync via $04844A; $01A1/$01E7 → index → $0485A1 | 94 | 47 |
| 00:877E–87E8 | Specialty table fill from $028484/$0284E4; JSL $87E9/$87F1; BMI alt $87D9 | 107 | 45 |
| 06:EBC1–EC09 | Scroll-table blit: $06EC0A[DP+$4F]→[$00] bank $0C copy $20 bytes to $0280; $0715=1 | 73 | 37 |
| 04:85A1–85B5 | Specialty command leaf: STA $1306, STZ $1308; A==$88 → XBA→$1318 | 21 | 12 |
| 04:844A–848F | Specialty sync from $1303 bit7 via $8490/$84B9 then $85A1 (#$86/#$87) | 70 | 31 |
| 00:87E9–87F8 | Wrappers: LDA $01D2/$01D3 then JSL $87F9; RTL | 16 | 6 |
| 00:87F9–8886 | Specialty gfx stream walk → $0283C4/$C559/$C68C/$90E6 (idx≥$26 via $90EC) | 142 | 71 |
| 04:8490–84B8 | APU boot: JSL $84E2, seed $188000, JSL $85F1, clear $2140–43 | 41 | 14 |
| 04:84B9–84E1 | APU boot alt: same with $19B8A4 | 41 | 14 |
| 04:84E2–8513 | APU handshake to $BBAA via $2140/$4210 | 50 | 20 |
| 04:85F1–8673 | APU block transfer from [DP+$73] through $2140–42 | 131 | 68 |
| 00:8FB2–9082 | Scene continuation after $8F46: gates, APU wait, specialty/$85A1, JSLs; RTL | 209 | 77 |
| 03:F021–F03F | Actor-slot scan for type≥$C0 matching DP+$00 at $0B1C,Y; CLC/RTL or walk | 31 | 14 |
| 03:F06F–F08C | Tiny leaf: $F021 + type $3A/$0C36 gate → rewrite type #$39 | 30 | 13 |
| 04:8514–85A0 | APU command drain: echo $1306→$2140; shift $1309 queue vs $1311 | 141 | 57 |
| 01:C535–C583 | Flag rewrite from $01C584 (or #$84 override); enables bank-1 map | 79 | 36 |
| 01:B61E–B65C | WRAM $7E3000 clear/fill via $B647; enables bank-1 map | 63 | 28 |
| 01:C9F9–CA97 | Actor gfx stream select/bit-test; alt $CA34; dual $CAAB queue around $8711 | 159 | 66 |
| 01:CA98–CAAA | Decompress setup: $0D77/$0D79 → DP+$83/$85; JSL $C559/$C68C | 19 | 7 |
| 01:CAAB–CACA | VRAM queue entry fill at X ($7E bank, len #$0100) | 32 | 13 |
| 01:B65D–B674 | After $B647: zero $7E3000,X down until X==DP+$0E | 24 | 11 |
| 01:B675–B6AB | Alt LDX#0; $B67A gate+$8711 queue seed $2C00→$3000 | 55 | 21 |
| 01:B6AC–B6BE | Fill $7E4000.. with #$00FF words (X=$07FE..0) | 19 | 9 |
| 01:B6BF–B6EC | Queue seed $6800→$4000 len #$0800; STZ $0D74 | 46 | 17 |
| 00:9083–90C0 | After $8FB2 early JMPs: JSLs, optional $7E4800, wait, BRA $907E | 62 | 20 |
| 03:FD64–FE0D | First JSL from $8FB2: gate/seed scrolls/$0EA3; compact $0190; $E4F4 | 170 | 72 |
| 00:A956–A960 | Wait $4212 bit7 clear then set; RTS ($A95B mid-entry) | 11 | 5 |
| 01:B6ED–B6FF | Fill $7E8000.. with #$00FF words (X=$0FFE..0) | 19 | 9 |
| 01:B700–B72A | VRAM queue seed $6000→$8000 len #$1000 bank $7E | 43 | 16 |
| 01:F802–F83C | $0D9B rewrite or Y=0..4 slot prep via $0594A1 | 59 | 32 |
| 03:F375–F39D | Gate $1648/$01D8/$0173; $1303=#$82; JSL $844A/$85A1 | 41 | 16 |
| 03:FE0E–FE29 | Snapshot $1648/$01D8/$01D9/$01DC → $07BD..$07C0; STZ $01D9 | 28 | 10 |
| 00:A961–A99A | Mode-7 matrix load DP+$76..$7B → $211B–$2120 after $A956/$B960 | 58 | 22 |
| 04:8674–869D | Frame-tail clears + JSLs; DP+$24→$25 | 42 | 14 |
| 01:F4B2–F53A | After $F802: $F53B prep, slot walk $F983/$FB2A/$9D2A, $FA1A | 137 | 59 |
| 01:F983–F99F | $0E01,Y×2 + $02A597 long-ptr word → DP+$00 | 29 | 15 |
| 01:FB2A–FBB2 | Expand [DP+$00] into $7E5000,X via $01FBB3; term #$80 | 137 | 71 |
| 00:B960–B9A4 | Mode-7 HDMA ch6/7 + CGRAM DMA trigger; RTS | 69 | 26 |
| 00:AB00–AB2B | After $82FB: DP+$3B..$42 → BG3/BG4 scroll latches | 44 | 18 |
| 04:8938–8973 | Queue seed $0000→$0000/#$8000/$7F; INC $0715; $8324/$049EF2 | 60 | 21 |
| 04:89E2–8A4E | Mode-7 PPU/window preset + $12xx seeds; JSR ($8A4F,X) | 109 | 40 |
| 01:B72B–B7B9 | $0D66 table dispatch via $02E0F2 into $B55A / typed seeds | 143 | 70 |
| 01:F660–F67A | Type gate: SEC if $0E01,Y in {$10,$11,$4C,$4D,$4F} else CLC | 27 | 15 |
| 01:F67B–F691 | Decompress setup DP+$83=#$EB84; VMAIN; JSL $C559/$C68C | 23 | 9 |
| 01:F53B–F65F | First JSL from $F4B2: dual $F67B DMA, $F660 path / bulk+$0E0F fill | 293 | 115 |
| 04:8974–89E1 | VRAM-queue sibling after $8938 gated on $122A; optional $B67A | 110 | 46 |
| 01:B55A–B61D | WRAM $7E3000 pattern fill from DP+$00/$01/$02; clear $07B4 | 196 | 92 |
| 04:871E–8802 | Open $8674 callee: $121B→bank-4 ptr tables, $C559/$C68C, stream JSLs, $0200 blit; PHP/PLP | 229 | 103 |
| 01:FA1A–FA85 | F4B2 tail: adjust $0E0F via $01FD79; fill $0E0F,$60.. from $01FC87 | 108 | 52 |
| 01:C885–C8AA | Copy 16 words [DP+$00] bank$09 → $0300+(DP+$03&$FE)<<4 | 38 | 23 |
| 01:C8C8–C8E4 | $F53B SEC-path: $02A284[DP+$04]→DP+$00; JSL $C885 | 29 | 15 |
| 01:FC00–FC22 | F4B2/FBBC helper: $0E0F/$0E10,Y → DP+$00 scale word | 35 | 21 |
| 01:FAE9–FB29 | FBBC helper: table $02:AB20[A] words → $7E4000,X | 65 | 35 |
| 01:FBBC–FBFF | F4B2 optional path: $F660/$FC00/$FAE9 then $8AF1 walk | 68 | 32 |
| 01:C987–C9F8 | F53B SEC sibling: BPL RTL / ASL → $C9F9/$C9D9/$CA34 | 114 | 57 |
| 04:889E–88C5 | Fill $7F0000 with $FF/$00FF via [DP+$73] | 40 | 20 |
| 04:8411–8449 | Copy $7E9000→$7F0000 with 64-byte row skip +$C0 | 57 | 32 |
| 04:83B2–8410 | Bitplane unpack DB:(DP+$86) → $7F0001,X | 95 | 54 |
| 04:8282–82EB | Stream unpack [DP+$73]→[DP+$76],Y via ($82A3,X) | 106 | 56 |
| **Total** | **One hundred forty-six complete routines plus a reset prefix** | **10251** | **4629** |

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
worklist contains 13,237 instruction variants. Of those, 4629 instruction sites
are covered by reviewed C replacements: **34.97% of the discovered worklist**.
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
The current inventory is 10251 ROM bytes / 4629 instruction sites.

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
