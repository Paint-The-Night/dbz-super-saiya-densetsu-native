# Progress — 8 September 2026

Target: a faithful, portable reconstruction of Japanese Rev 1 game logic in C,
with a native Mac application first. The current executable is a hybrid:
compiled replacements run alongside an interpreter for unrecovered code.

## Integration check — 8 September 2026

The 56-commit reconstruction advance through `eb21a9b` builds locally and passes
the native and checkpoint equivalence suites. A compiler warning exposed an
incorrectly grouped face-byte assertion at `$05:DA64`; it now checks the complete
merge result over all 65,536 input-byte pairs. The expanded native suite also
passes with address and undefined-behavior sanitizers enabled.

The opening checkpoint plus `research/scout/battle-route.inputs` passes 1,800
frames with every state, video, and audio comparison matching the reference.
The local report is `artifacts/grok-integration-battle/report.json`: 20,555,315
native steps and 3,178,685 interpreted steps. These counts describe this route,
not overall game completion. Earlier milestone sections below retain their
historical coverage and artifact references.

The README tracker can be refreshed with
`python3 tools/progress.py --update-readme`; `--check-readme` detects stale text
and runs through CTest.
This checks agreement with the maintained inventory, not the inventory's
completeness or the fidelity of untested game routes.

## Reconstructed and verified

The `$00:98D5–990E` actor-slot loop adds 27 instruction sites and 58 bytes.
Its `$01:C91F` graphics setup callee and `$01:C943` pointer helper are now native;
`$01:C95A` still uses interpreter fallback. The loop reconstruction covers
selectors, counter update, and long-call boundaries. Differential cases
exercise all eight slot indices, all selector paths, both ROM mirrors, unaligned
direct page, stack restoration, and palette-counter overflow. The extended
`battle_equivalence` test recreates the opening checkpoint from reset and then
checks the documented battle route, so it needs no archived checkpoint.

Graphics setup adds 30 sites / 59 bytes, with 8,192 differential combinations
covering all selector bytes and actor slots, both ROM mirrors and direct-page
alignments, plus two long-return cases. Tests execute the real nested JSR/RTS
and compare every instruction's CPU state and ordered bus transactions before
the upload call at `$00:85F5`.

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
| 00:A99B–AAD8 | Post-$AB00 Mode-7 epilogue: gate/compare + WRAM fill/HDMA/$AE38/VRAM poke RTS | 318 | 140 |
| 01:FA86–FAE8 | Gap leaf: clear $0300; copy $7F2800 slots via $01FADF into $0200; INC $0715 | 99 | 45 |
| 00:AE38–AE69 | Mode-7 VRAM DMA helper (ch4 $7E9000); mid-entries $AE5A/$AE61 | 50 | 20 |
| 05:C029–C07A | Close $BE54: stream walk via $158060,X; $96F3 paths through RTS | 82 | 42 |
| 05:BE54–BECA | Bank-5 enable: five-slot pack via $BE61 + scale helper $BEAB (to $C029) | 119 | 61 |
| 00:C5ED–C68B | Bit-packed decompress + WMDATA stream (callee of $96F3) | 159 | 86 |
| 00:C707–C781 | Tile rearrange via DP scratch + MVN into bank $7F (callee of $96F3) | 123 | 57 |
| 05:BECB–C010 | Post-$BE54 stream: $BEAB/$C029, VRAM-queue seed, WMDATA opcodes + $C00A | 326 | 145 |
| 01:F9A0–F9AB | Thin wrap: JSL $F983, LDX#0, JSL $FB2A; RTL (callee of $03:C246) | 12 | 4 |
| 05:C07B–C0EC | Actor palette via $1ECB06/08/09; $CE4E override; $0300/$16E1 copy; INC $0715 | 114 | 55 |
| 03:C232–C24A | Seed $7F0000/$0E01=#$05; JSL $BE54/$FA86/$F9A0; RTL | 25 | 8 |
| 05:9AFF–9B36 | Sole $BECB caller prefix: table Y, scene gates, optional $BECB, early RTL | 56 | 26 |
| 05:9B37–9B88 | Post-$9AFF prep: actor fields, $05C021 ptr, JSR $9B91, bounds → $9626/$85B9 | 82 | 34 |
| 05:9B91–9C0A | Adjust leaf: $0021,Y scale via $059C0B/$059C17 into DP+$0A/$0C; RTS | 122 | 59 |
| 05:C0ED–C115 | Dual-slot palette push $153B/$153C → JSR $C116; RTL | 41 | 17 |
| 05:C116–C1BB | Slot copy/blend $15E1,X→$0200,X (or $05C1B8 add); INC $0715; RTS | 166 | 74 |
| 05:9626–96D6 | OAM/placement walker from [DP+$00] into $0400/$0600 via $0085A9; RTL | 177 | 102 |
| 05:96D7–96E1 | X += $20 (16-bit) then RTS | 11 | 7 |
| 05:96E2–96EC | Y += $20 (16-bit) then RTS | 11 | 7 |
| 05:96ED–9700 | Extends $96F3: STY $86 / bank $7F then VMAIN+$C5ED/$C707; RTS | 20 | 8 |
| 05:B572–B581 | Type&$7F → $0D8200,X table byte&$7F; RTS | 16 | 10 |
| 05:C1C8–C1ED | Actor flag predicate via $B572 → #$80/#$81/#$82/#$83; RTS | 38 | 20 |
| 05:C1EE–C272 | Timer/anim mask via $059B89/$05C273; mid $C20D; AND [$00] words; RTL | 133 | 65 |
| 05:E38E–E3CB | Sole $9626 JSL caller: $1546 gate, $05E40E ptrs, $E3CC scale, JSL $9626 | 62 | 28 |
| 05:E3CC–E3F1 | Scale helper: $05E400,X × DP+$03 via $4202/$03; ± $05E3F2,X from $4216 | 38 | 15 |
| 05:86DE–871C | Dual $B110/$B045 prologue + dual $C1C8 → $153B/$153C; tick DP+$AB; sync $1480/$14A0 | 63 | 25 |
| 1D:8807–8842 | Post-$8803 trampoline: RNG-seed $1558–$155D when $1559,X clear; enables bank-1D | 60 | 25 |
| 05:8857–8886 | C07B caller leaf: CPY gates; optional $C98F/$CA07/$C07B; seed $AB/$AC/$B6; JSL $93A9; RTL | 48 | 20 |
| 05:B0B1–B0BD | Dual $C07B palette push for $1400/$1440; RTS | 13 | 5 |
| 1D:8843–8905 | Timer/$1570 walker: BMI gate, slot walk, table $1D8968/69/6B, scale, JSR $8906; RTL | 195 | 96 |
| 05:9768–97C8 | Clear $80 markers via $96E2; copy $17C6→$0140; seed $0738; JSL $0091BD; RTL | 97 | 38 |
| 05:8116–81EA | Dual $C07B then optional $7F wipe/$E018/$F9B0; $0D00 clear; bank-1D JSL chain; RTL | 213 | 75 |
| 05:90CC–9114 | $14E4/$AC gate; REP seed $1400 words; SEP+$C07B; JSL $9488; AB seed; RTL | 73 | 30 |
| 05:B045–B0B0 | Seed $1421/$1461 via $B0BE/$B0F9+$B582; clear vel bytes; falls into $B0B1 | 108 | 40 |
| 05:B110–B11C | Clear 32 bytes at $0020,Y (X=#$001F..0); RTS | 13 | 7 |
| 1D:8906–8967 | Walker callee: $1092B1,X ptr; optional DP+$0C negate; $85B9 or STZ $1570; RTS | 98 | 47 |
| 05:B0BE–B0F8 | Seed $1420/$1422/$1432/$1424 from $1401/$B7/$14D5/$01D6; optional $B00A; RTS | 59 | 23 |
| 05:B0F9–B10F | Seed $1460/$1462/$1472/$1464 from $1441; RTS | 23 | 9 |
| 05:B582–B593 | Type&$7F → $0D8200,X bit7 via EOR #$80; RTS (sibling of $B572) | 18 | 11 |
| 05:9488–9490 | If $071B nonzero PLA×3 then RTL; else RTL (JSL early-exit) | 9 | 6 |
| 1D:8A77–8A84 | Y=#$1400, or #$1440 when $14C2 bit0 set; RTS | 14 | 6 |
| 05:97C9–9882 | JSL $9768; mask $0B00 via $96D7 walk; scene gates → DP+$24/$0716; RTL | 186 | 78 |
| 05:CA10–CA88 | Actor slot update via $CA90/$DEC3/$DA64; $1440 tail; RTL | 121 | 55 |
| 05:9115–9200 | DP+$AC phase machine; JSL $9488/$CA10/$DFE3; multiple RTL / JMP $97C9 | 236 | 89 |
| 05:B00A–B044 | BMI-path callee of $B0BE: seed $17EA..$17F3; JSL $1C806C/$1C8136; RTS | 59 | 20 |
| 05:9883–98C7 | JMP $C41F; seed $17DD/$17E1/$14FF/$17E5 + DP+$B0 from $01E7/$01D6/$17D2; RTS | 69 | 27 |
| 05:C41F–C44F | Scene index from $14C1 via $05C450; optional INC $1220 / JSL $0485A1; RTL | 49 | 21 |
| 05:90A5–90B2 | Carry predicate: SEC; $15FA==#$80 RTS else LSR $01E7×2; RTS | 14 | 8 |
| 05:DA64–DA7B | Face-byte merge at $0022,Y via DP+$05/$04; RTS | 24 | 11 |
| 05:C9FA–CA06 | Clear $1502..$1505; RTS (callee of $C98F/$DFE3) | 13 | 5 |
| 05:DFE3–DFF2 | DP+$B8&$C0 gate: PLA×3 RTL or JSR $C9FA; RTL | 16 | 10 |
| 05:C98F–C9F9 | Store A→DP+$B7; clear battle fields; optional JSL $05870B; JSR $C9FA; RTS | 107 | 43 |
| 1D:8A85–8AA1 | Scan WRAM list $1D0100 for A; RTL #$FF if missing | 29 | 14 |
| 1D:8AA2–8AB2 | Append A into list at $0100,$01B2; INC $01B2; RTL | 17 | 10 |
| 01:F692–F6DB | Fill $0E00.. from $02BF0B[$0EA3×5]; stride +$20; JSL $01B8DE; RTL | 74 | 32 |
| 05:CA8D–CAF7 | $CA8D JSR $DD51 trampoline + jump-table body; JMP ($CAFA,X) | 109 | 55 |
| 05:DEC3–DF5F | Script walker [$00] via $002C/$0030,Y; shared $DF51 advance leaf | 157 | 75 |
| 01:F719–F7F1 | F692 sibling: scale $0E00 slots from $02B214×$0E; stride +$20; RTL | 217 | 91 |
| 05:B6B2–B6CD | Slot Y select pair: #$1400/#$1440 from $14C2 bit0 (B6C0 inverted) | 28 | 12 |
| 05:DF60–DFC1 | Opcode-$22 face adjust via $B6C0/$05DFC2/$1D8FEC; JSR $95F6; RTS | 98 | 46 |
| 05:E102–E116 | $1546 BMI gate; $1540×2 → JMP ($E117,X) handler table | 21 | 11 |
| 05:93A9–9487 | Seed $7E3000 grid + $1640 face ids; optional $14D2 remap; JML $0089AC | 223 | 91 |
| 05:A435–A52D | Battle face/setup probe with nested $A4DF/$A4F5 leaves | 249 | 102 |
| 1D:8AB3–8ABC | Clear list byte $1D0101,X then JML $01C639 | 10 | 3 |
| 1D:8ABD–8B45 | Flight/input face nudge on $01D6/$01E7/$15FA/$4F/$50/$1482..; RTL | 137 | 63 |
| 05:95F6–9625 | Face-class → APU #$1C/#$1D via $0485B6; closes $DF60 | 48 | 23 |
| 05:B124–B130 | $14C2 bit0 invert+3 → DP+$B6; first $A435 callee | 13 | 7 |
| 05:BDDC–BE11 | $1700&3 gate via $B6C0; SEC early or SBC $14CF/$14D1 | 54 | 25 |
| 05:A9F4–AA0A | Scan $05AA0B; ASL bit into DP+$00 or STZ | 23 | 11 |
| 05:9491–94A0 | PHX; $A9F4; mask-clear $14D2; PLX; finishes $93A9 remap | 16 | 8 |
| 05:D521–D571 | ($CAFA,0)/(,6)/(,2) + $D531 sibling + $D566 helper | 81 | 39 |
| 05:F3AA–F40F | ($E117,0) — $F998/$1546 tails through RTS | 102 | 39 |
| 05:A52E–A580 | Deepen $A435: $B6C0/$B582 gates; optional $1C8136; RTS | 83 | 32 |
| 1D:8B46–8B7B | After $8ABD: $0C35 gates via $8AA2/$0594A1; RTL | 54 | 21 |
| 05:F998–F9AF | Clear $7F0000/$7F0800 word pairs; unblocks $F3AA | 24 | 11 |
| 05:F91E–F951 | VRAM-queue seed $0800 + $1541 length pick sibling | 52 | 21 |
| 05:F952–F997 | Mirror-XOR $7F0000 rows (EOR #$4000) via $96D7 stride | 70 | 33 |
| 1D:8B7C–8BDB | Post-$8B7B: $14D8 gate, copy $1D8BDC→$1400, JSL $8AB3; RTL | 96 | 43 |
| 05:DD51–DD68 | Wrap leaf: INC A → $0035/$0036,Y; clear $002B,Y; RTS | 24 | 11 |
| 05:DD69–DDD0 | ± $14C9/$14CA into $002E/$24 or $002F/$26 (four ± helpers) | 104 | 44 |
| 05:DFF3–E017 | Scale helper: STX DP+$00; $14C8→×; JSL $86FC; fold → $14CB/$14C9 | 37 | 17 |
| 05:F548–F54E | ORA #$02 into DP+$B0; RTS (callee of $F410) | 7 | 4 |
| 05:D7F4–D81D | ($CAFA,6)+$D802/$D817 face ORA/#$80 CMP tails → JMP $CA8D | 42 | 18 |
| 05:D81E–D848 | ($CAFA,8) seed $002C/$2D/$30; optional $148C; JMP $DD51 | 43 | 18 |
| 05:D849–D857 | ($CAFA,A) $0030,Y==$FF → LDA #0 / JMP $DD51 else RTS | 15 | 7 |
| 05:D6D9–D6E9 + D6EB–D7F3 | ($CAFA,4)/(,E) + $D6F6/$D73A/$D6D9 gap: $DFF3/$DD9D/$DDB7 → $DD51/$CA8D | 283 | 127 |
| 05:D572–D60E | Pre-($CAFA,12): $D572/$D5B9 clamp + $D5FB wrap of $D60F | 157 | 74 |
| 05:D60F–D6D8 | ($CAFA,12) $D60F + $D626…$D681/$D69C siblings → $D767/$D6E2/$D748 | 202 | 101 |
| 05:D858–D8D8 | ($CAFA,10)/(,14): $D873+$D858 stream pick; $D8A2; $D8B3 face seed | 129 | 60 |
| 05:F410–F47F | ($E117,2): $F998 stream/$F952/$F91E/$F548; JSL $1D89D1; RTS | 112 | 47 |
| 05:A581–A5F6 | $A52E fail/alt: type gates; RTS for #$36/#$37/#$5E; else JMP $A8E9/$A907 | 118 | 49 |
| 05:A5F7–A8E8 | Continue $A581 type gates; JMP $A8E9/$A907 / multi-RTS | 754 | 324 |
| 05:A8E9–A90C | Y face-shrink via $14CC/$14CD; STX $1527; PLA×2; RTS | 36 | 23 |
| 05:A90D–A93C | Mask bit into $152D,X / $0019,Y via $A93D/$B6C0; RTS | 48 | 21 |
| 05:A93D–A960 | Face/slot index from $14C2/$14C3/$14C5 → X; RTS | 36 | 20 |
| 05:B11D–B123 | Alt $B124 entry — $14C2&1 without invert; BRA $B12B | 7 | 3 |
| 05:D8DB–DA40 | ($CAFA,16..) $D8DB/$D955/$D970/$D9A6/$DA03 + siblings → $CA8D | 358 | 155 |
| 05:E089–E0B0 | A→X index $1D92B2; ptr $05E0B1 → DP+$83/$85; JMP $96ED | 40 | 19 |
| 05:F480–F547 | ($E117,4/6) $1547 phase; $E089/$F8B2/$F998/$F91E; fall $F548 | 200 | 88 |
| 05:F85E–F885 | Clear scroll pair by $1541; STZ $1546; RTS | 40 | 16 |
| 05:F8A1–F8B1 | Type gate #$54/#$55/#$3D keep A else #0; RTS | 17 | 9 |
| 05:F8B2–F90F | Expand [DP+$00] tiles into $7F0000,Y via [$04]; RTS | 94 | 53 |
| 05:F910–F91D | JSR $F91E then $0806=#$0D80; RTS | 14 | 6 |
| 1D:89D1–89F7 | Copy $1D8A0D→$1570 (89D1) / sparse $1D89F8 (89DF); RTL | 39 | 19 |
| 05:F54F–F684 | ($E117,8): $1546 phase #$80→$F609 / #$82 DMA / else AND $B0; RTS | 310 | 123 |
| 05:F685–F6A7 | Helper: seed $0808..$0810 via $F6A8-adjusted Y; RTS | 35 | 13 |
| 05:F6A8–F6B8 | Helper: if DP+$B7 < #$12, Y += #$0040; RTS | 17 | 10 |
| 05:F6B9–F6EE | Helper: optional Y+=#$80; stream $10:8CF0,X into [DP+$00],Y; RTS | 54 | 29 |
| 05:F6EF–F76A | ($E117,A) + X stubs → $F998/$F8B2/$F91E/$F93D/$F85E; RTS | 124 | 53 |
| 05:F76B–F82A | ($E117,E..) stubs + $F782/$F7CA; call $F82B/$F8A1/$F000; $F820 falls in | 192 | 82 |
| 05:F000–F069 | ($E117) prep: $1546 #$80/#$81 → $E089/$F91E/$B6B2 queue seed; RTS | 106 | 49 |
| 05:95C9–95F5 | Face index via $B6B2 + $1D8C67,X → $121B..$121E / DP+$24; RTS | 45 | 20 |
| 05:DA8D–DA9B | ($CAFA,36): #$82 → $95C9; $15E8=#$02; LDA #0 / JMP $DD51 | 15 | 6 |
| 05:F82B–F85D | Setup: $F998; ptr $05F886,X → DP+$83/$85; JSR $E0AB; fall $F85E | 51 | 21 |
| 05:DA45–DA63 | Face nibble gate → $DA64/$DA6D; JMP $CA8D | 31 | 14 |
| 05:DA7C–DA8C | Store DP+$04 → $0031,Y; optional $DA64; JMP $CA8D | 17 | 7 |
| 05:D485–D520 | ($CAFA,22) $D48F INY/BIT score + $D485/$D48A leaves; CMP $01/$34,X → $CA8D | 156 | 76 |
| 05:DA9C–DB2A | ($CAFA,38) $14CD==4 face seed via $95E4; else #1→$CA8D; $D8A2/$DD51 | 143 | 69 |
| 05:F06A–F0E9 | ($E117,40/48) $1546/#$80 early $155C seed; else $1558/$155C±6/$F386/$155A | 128 | 60 |
| 05:F386–F3A1 | m16 A adjust via $14C7&2 + $05F3A2[($14D4&$FF)<<1]; RTS | 28 | 16 |
| 05:F0EA–F118 | ($E117,E8/EA) scroll prep sibling — #$9C/#$82→$1558; early $155A/$155C | 47 | 20 |
| 05:F119–F13A | ($E117,3E) $F998; fill $7F0280.. with #$0001; $F91E/$F93D/$F85E | 34 | 14 |
| 05:F13B–F164 | ($E117,114+) $F998; #$83EC/#$8464 → $EAA4 (now in $EA8C); optional $F952 | 42 | 18 |
| 05:EA8C–EACF | ($E117,60+) $F998/$1540-#$30/$17969C + stream expand until #$FFFF/#$FFFE; $F91E/$F93D/$F85E | 68 | 34 |
| 05:F167–F352 | ($E117,20..) phase/DMA via $F353/$F364/$F386/$8711; $F29C tile path | 492 | 215 |
| 05:F353–F363 | Y-slot gate: DP+$B9==#$81 → CPY #$1400 else #$1440; RTS | 17 | 10 |
| 05:F364–F385 | Scale: [$12]×$1548 via $4202/03; wrap; LDY $4216; RTS | 34 | 14 |
| 05:D274–D305 | ($CAFA,8E) face-slot swap via $B6C0/$B6B2/$C07B; $15F1 snap; $1400↔$1440; JMP $DD51 | 146 | 55 |
| 05:D306–D32F | ($CAFA,7C) DP+$05=#$02/$DBF3; $01EF tier→$1540; $F85E/$E073; JMP $DC48 | 42 | 18 |
| 05:D330–D3D5 | ($CAFA,7A)/(,8C@$D3C1) actor scan $0B00; copy/scale; $C07B/$B11D; JSL $1D8583/$859B; JMP $DD51 | 166 | 70 |
| 05:DB2B–DC3A | ($CAFA,42..) $DB2B/$DB65/$DB6F/$DB8E/$DB99/$DBB0/$DBC6 + $DBF3/$DC1E helpers | 272 | 116 |
| 05:DC3B–DD50 | ($CAFA,50..) $DC48 (unblocks $D306) /$DC7C/$DC93/$DC9C/$DCB2/$DCC2/$DCDE/$DD29 + $DD02 | 278 | 122 |
| 05:DDD1–DE25 | Helpers $DDD1 pair-SBC + $DDF4 remap (early RTS; deep path separate) | 85 | 44 |
| 05:DE26–DE62 + DEA1–DEB1 | Deep $DDF4: $05DE63,X pair-table + $B6B2/$B572 match; $DEA1→$05DEB2 | 78 | 38 |
| 05:EED4–EF1F | ($E117,44) INC $1546 phase; $05EF20,X → $155A/$155C | 76 | 31 |
| 05:EF26–EFFF | ($E117,42/46/4A) EF26 nudge + EF5A/EFCC phase; shared $EFB0/$EFC0/$EFC6 | 218 | 92 |
| 05:ECE4–ED45 | ($E117,4C) #$80 seed / m16 $155A+=2,$155C-=3 → $1558 | 98 | 44 |
| 1D:8583–85DE | Face→$0D0x queue seed ($8583/$859B); remap FF/FE; JML $01C987 | 92 | 37 |
| 05:E018–E088 | $E018/$E01E queue seed + mid-entry $E073 ($1540→$1502,X) — $D306/$DB99 callee | 113 | 51 |
| 05:B594–B5C3 | Type remap $B594/$B5AA (09↔10 / 0F↔11 / 42↔4D); PHX/TXA/PLX | 48 | 24 |
| 05:D3D6–D484 | ($CAFA,9C/@$D408)/(,98@$D44E)/(,74@$D45F) + $D3D6/$D3E9; $B594/$1D8583 paths | 175 | 77 |
| 05:EA52–EA8B | ($E117) stream prologues $EA52/$EA6F → $F998 + JMP $EAA4 | 58 | 28 |
| 05:EAD0–EB30 | ($E117) $EAD0/$EAEB/$EB08 queue-stream via $96ED/$F91E/$F93D/$F85E | 97 | 47 |
| 05:EB31–ECD3 | ($E117,4E/E6/F8) $EB36/$EC61 large phase/scroll oscillator + $05ECD4 | 419 | 184 |
| 05:ED46–ED87 | ($E117,D0/5E) #$83/#$80 ÷3 remainder → $1558 + coord seed | 66 | 32 |
| 05:EDB4–EDFA | ($E117,5C) phase/$B6B2 class → $05EDFB pair into $155A/$155C | 71 | 29 |
| 05:EE01–EE55 | ($E117,5A) #$82-phase; DP+$B7 gates → $05EE56 pair | 85 | 35 |
| 05:E921–E97A | ($E117,D8) dual-bank scroll seed + m16 $1562/$1564 drift | 90 | 35 |
| 05:EE5E–EED3 | ($E117,11A/118) EE5E/EE99 seeds; shared $EEBD $155A-=2 drift | 118 | 50 |
| 05:E97B–E9A1 | ($E117,D4) INC phase; ORA #$84 → $1560 + Y seeds | 39 | 17 |
| 05:E9A2–E9D3 | ($E117,D2) bit1→$155C; #$92/$155E; DP+$51 gate | 50 | 21 |
| 05:E9D4–EA51 | ($E117,CC/CA) E9D4/EA0E seeds; shared $EA2D m16 ± drift | 126 | 55 |
| 05:ED88–EDB3 | ($E117,136) EDB4 sibling — #$81-phase → $1560 bank | 44 | 20 |
| 05:E8DC–E920 | ($E117,DA) #$80 seed + ORA #$40; m16 $155C-=4 $155A-=2 | 69 | 28 |
| 05:E87F–E8DB | ($E117,DE) early seed; else #$93 gate + m16 ± drift | 93 | 41 |
| 05:E845–E87E | ($E117,E2) DP+$00/#$80 phase + optional INC ≥#$9E | 58 | 26 |
| 05:E7FF–E844 | ($E117,E4) m16 (DP+$51>>2)÷3 via $4204/06 + $B6B2 class | 70 | 31 |
| 05:E6D6–E7FE | ($E117,EC/116) #$80 stream/$F998; #$81 $F685; else ±3 $17DD | 297 | 134 |
| 05:E6B1–E6D5 | ($E117,EE) INC phase; #$81-bank $1560 + coords | 37 | 17 |
| 05:E61B–E6B0 | ($E117,F4/F2/F0) E61B nudge + E655/E659/$E68A dual seed | 150 | 61 |
| 05:E5AA–E61A | ($E117,FA) #$80 seed / $B6B2±3 drift + phase clamp | 113 | 51 |
| 05:E565–E5A9 | ($E117,FA-1) INC phase; #$80 seed / bit2 toggle / ≥#$C0 clear | 69 | 30 |
| 05:E24F–E25F | ($E117,122..) $E24F STZ; $E253 $1541→$1511/$1521; join STZ | 17 | 7 |
| 05:E260–E2BB | ($E117,12A/132) #$80 dual $1D8807 seed / $1558 wrap ADC; double-INC | 93 | 35 |
| 05:E2BD–E2F3 | ($E117,120) INC phase; #$91 + $05E2F4[X] pair (DP+$B7==#$28) | 55 | 21 |
| 05:E2F8–E31E | ($E117,11E) INC phase; (A>>2)&1|#$82 → $1560 + coords | 39 | 17 |
| 05:E31F–E341 | ($E117,11C) INC phase; #$A7/$1560 + Y #$24/#$88 | 35 | 14 |
| 05:E342–E38D | ($E117,102/104) Y=#$8004/#$8406 → DP; #$80 seed/$F000; else bump | 76 | 32 |
| 05:E446–E564 | ($E117,FC) multi-phase dual-bank seed + m16 ±4 pinch/expand | 287 | 110 |
| 00:98D5–990E | Scan eight actor slots, select palette data, dispatch bank-1 helpers | 58 | 27 |
| 01:C91F–C942 | Resolve actor graphics pointer and prepare upload arguments | 36 | 16 |
| 01:C943–C959 | Shared actor graphics table selector (JSR/RTS) | 23 | 14 |
| 00:91B6–91BC | Scene-mode thunk: set DP+$61 and jump to FC D3 | 7 | 3 |
| 00:943F–9443 | Scene-table leaf delegating to bank-04 PPU preset | 5 | 2 |
| 00:952B–952F | Scene-table leaf delegating to bank-02 handler | 5 | 2 |
| 00:9530–9534 | Scene-table leaf delegating to bank-04 handler | 5 | 2 |
| 00:95A9–95B6 | Scene-mode[15] flag setup and bank-00:8E96 delegation | 14 | 6 |
| 00:9444–9458 | Scene-table PPU preset wrapper and bank-04 delegation | 21 | 9 |
| 00:9459–946E | Scene-table reset wrapper with dual bank calls | 22 | 8 |
| 00:97E9–97FC | Scene-state seed leaf | 20 | 10 |
| 00:9735 | Scene leaf return | 1 | 1 |
| 00:9AC8 | Scene call stub | 4 | 1 |
| 00:9AF5 | Scene call stub | 4 | 1 |
| 00:9AFA | Scene call stub | 4 | 1 |
| 00:FCD8 | Scene call stub | 4 | 1 |
| 00:9AEC–9AF4 | Scene call/return wrapper | 9 | 3 |
| 00:9AE3–9AE7 | Scene call wrapper | 8 | 2 |
| 02:F463 | Actor leaf return | 1 | 1 |
| 00:FCE2–FCE6 (M/X variants) | Scene call/return wrapper | 8 | 4 |
| 03:F39E/F39F/F3A0/F4AD/F560/F582/F5A2 | Bank-3 leaf returns | 7 | 7 |
| 00:99AD–99C1 | Scene post-processing continuation | 21 | 8 |
| 00:991D–9926 | Scene continuation branch | 13 | 4 |
| 00:987A–9883 | Scene flag branch | 11 | 4 |
| 01:8481 | Actor long-jump stub | 4 | 1 |
| 05:9390 (M/X variants) | Actor leaf return | 2 | 2 |
| 04:8C4F | Audio leaf return | 1 | 1 |
| 01:EDF4 | Actor call stub | 4 | 1 |
| 01:EC4C | Actor call stub | 4 | 1 |
| 01:CD1E | Actor call stub | 4 | 1 |
| 01:CD27 | Actor call stub | 4 | 1 |
| 01:CD4B | Actor call stub | 4 | 1 |
| 01:CD8A | Actor call stub | 4 | 1 |
| 01:EDF9 | Actor leaf return | 1 | 1 |
| 00:99C2 | Scene leaf return | 1 | 1 |
| 00:99C3–99CB | Scene transition trampoline | 7 | 3 |
| **Total** | **Three hundred twenty-four complete routines plus a reset prefix** | **26780** | **11942** |



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
worklist contains 13,237 instruction variants. Of those, 11942 instruction sites
are covered by reviewed C replacements: **89.54% of the discovered worklist**.
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
The current inventory is 26780 ROM bytes / 11942 instruction sites.

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

## Scene-wrapper increment — 8 September 2026

The `$00:9AFF–9B03` scene wrapper is now native. Its two long-call boundaries
match the interpreter exactly: the first calls the shared `$00:9735` return leaf,
and the continuation calls bank-3 `$E8AA`. Differential tests cover both entry
sites and preserve the ROM's stack and bank transitions. This adds 2 verified
instruction sites and 8 ROM bytes.

The bank-3 selector load at `$03:E8AA` is also native, with a direct CPU and
bus-equivalence test before its still-interpreted setup call. This adds 1 site
and 3 ROM bytes.

The bank-3 `$F246` leaf return now shares the verified RTL handler used by the
neighboring scene exits, adding 1 site and 1 ROM byte.

Thirteen bank-1 entry prefixes that begin with the shared absolute `$073C`
status load are native and differentially tested; their local continuations
remain interpreter-backed. This adds 13 verified sites and 39 ROM bytes.

The bank-1 `$D433` entry prefix now performs its verified absolute `$072E` load
natively, adding 1 site and 3 ROM bytes.

Two small bank-1 helpers are native: `$A3DB–A3E2` initializes the actor upload
slot, and `$D00B–D012` advances its timer pair. Differential tests cover both
complete leaf paths, adding 8 sites and 17 ROM bytes.

The bank-1 `$CEBA–CEC5` actor reset leaf is native, including both long-call
boundaries, the `$0725` clear, and its RTL. Tests add 4 verified sites and 12
ROM bytes.

The bank-1 `$A4EB–A4F2` table-entry prefix is native, covering both long-call
boundaries, the `$0D66` selector load, and the local JSR boundary. This adds 3
verified instruction sites and 10 ROM bytes.

The `$00:AE29–AE37` mode-7 preparation leaf is native, covering pointer setup,
two existing decompression calls, output-index loading, and RTS. Direct tests
cover each instruction boundary and the complete return behavior. This adds 6
verified sites and 18 ROM bytes.

The `$01:8092–80A1` actor-index resolver is native, including table indexing,
long-ROM lookup, result storage, and RTL. Differential coverage exercises the
complete path and return stack. This adds 8 verified sites and 23 ROM bytes.

The matching `$01:8241–824F` actor-index resolver is native, including its
second table lookup, `$0D5E` result store, and RTL. Differential coverage adds
7 verified sites and 15 ROM bytes.

The bank-1 `$BBF7–BC03` actor-table resolver is native, including indexed
long-ROM lookup, Y result transfer, and RTL. Differential coverage adds 7
verified sites and 13 ROM bytes.

The bank-3 `$958A–9599` actor-coordinate prefix is native, covering coordinate
copies, compact table selection, and its local JSR boundary. Differential tests
cover the six data-moving boundaries; this adds 7 verified sites and 20 ROM bytes.

The bank-1 `$BA10–BA29` flag gates are native, including both conditional
helper calls and return paths. Differential tests cover all 12 instruction
sites and branch conditions, adding 12 sites and 26 ROM bytes.

The bank-1 `$CE57–CE6C` status-reset leaf is native, including both helper
calls, masked active-bit update, and RTL. Differential tests cover all 8 sites,
adding 8 verified sites and 22 ROM bytes.

The bank-1 `$CCEF–CD01` status-counter prefix is native, covering its helper
call, counter increment, compare/branch gate, and two dispatch calls.
Differential tests cover all 7 manifest-listed sites, adding 7 verified sites
and 22 ROM bytes.

The bank-1 `$CE96–CEAC` actor-status dispatch is native, including the status
nibble gate, conditional helper path, dispatch calls, and RTL. Differential
tests cover all 8 manifest-listed sites, adding 8 verified sites and 22 ROM
bytes.

The bank-3 `$A3A8–A3B8` status-mask gate is native across its six assigned
sites, including both conditional paths and actor dispatch calls. Differential
tests add 6 verified sites and 17 ROM bytes; the later `$A3C4` continuation
remains outside this entry’s assigned slice.

The bank-1 `$CEAC–CEB8` status-counter prefix is native across its five
unclaimed sites, including the helper call, increment, compare, and carry gate.
The later `$CEBA–CEC5` leaf remains covered by its existing native entry.
Differential tests add 5 verified sites and 14 ROM bytes.

The bank-3 `$F7DE–F7E6` phase marker leaf is native, covering both status stores,
the `$80` phase value, and RTL. Differential tests cover all 4 manifest-listed
sites, adding 4 verified sites and 9 ROM bytes.

The bank-3 `$E8AA–E8AF` setup wrapper now covers both manifest sites: selector
load and the shared `$8000` subroutine call. Differential tests add 2 verified
sites and 6 ROM bytes.

The bank-3 `$F5F6–F5FD` setup prefix is native across its three assigned sites:
helper call, status load, and shared `$8000` call. Differential tests add 3
verified sites and 10 ROM bytes.

The bank-3 `$953E–954B` status/setup prefix is native across its six assigned
sites, including the carry gate, shared `$958A` call, and indexed load. The
manifest’s later continuation remains truncated; differential tests add 6
verified sites and 16 ROM bytes.

The bank-3 `$E655–E66A` bounded copy loop is native, including its indexed
span, `BNE` exit, `BCC` repeat, and both long-return paths. Differential tests
cover all 10 manifest-listed sites, adding 10 verified sites and 22 ROM bytes.

The bank-3 `$E757–E768` eight-byte long-memory copy loop is native, including
the indexed loads/stores, 16-bit loop counter, carry gate, and RTS. Differential
tests cover all 7 manifest-listed sites, adding 7 verified sites and 18 ROM
bytes.

The bank-4 `$8E74–8E83` masked counter decrement is native, covering its
absolute load, mask, zero/carry gates, decrement, and RTS. Differential tests
cover all 7 manifest-listed sites, adding 7 verified sites and 16 ROM bytes.

The bank-3 `$B7CB–B7D6` scratch-index wrapper is native, covering the 16-bit
index load, direct-page store, helper call, and RTL. Differential tests cover
all 5 manifest-listed sites, adding 5 verified sites and 12 ROM bytes.

The bank-3 `$B7F4–B7FF` sibling wrapper is native with its alternate scratch
index constant. Differential tests cover all 5 manifest-listed sites, adding
5 verified sites and 12 ROM bytes.

The bank-4 `$8E3C–8E49` 16-bit counter clamp is native, covering its absolute
load, mask, direct-page compare, carry gate, increment, and RTS. Differential
tests cover all 6 manifest-listed sites, adding 6 verified sites and 16 ROM
bytes.

The bank-3 `$E769–E78C` sibling long-memory copy loops are native, covering
both indexed spans, their 16-bit counters, carry gates, and RTSs. Differential
tests cover all 14 manifest-listed sites, adding 14 verified sites and 36 ROM
bytes.
