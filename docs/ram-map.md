# Recovered RAM roles — Japanese Rev 1

This is an evidence-backed partial map. DP-relative offsets below are added to
the CPU direct-page register. The ordinary NMI path sets DP to zero. Absolute
addresses use the data bank; the reviewed game paths use the low-memory mirror.
Names describe observed use and are not recovered original source identifiers.

| Address / offset | Width | Observed role | Evidence routine |
|---|---|---|---|
| DP+29 | byte | Cached display brightness / forced blank | 828F, 8299 |
| DP+2F, DP+31 | words | Independent RNG seeds, next = 5*seed+17 | 82A3, 82B5 |
| DP+33, DP+35 | words | BG2 horizontal / vertical scroll | 82FB |
| DP+37, DP+39 | words | BG1 horizontal / vertical scroll | 82FB |
| DP+43, DP+45 | words | Current controller 1 / 2 buttons | 82D4 |
| DP+47, DP+49 | words | Newly pressed controller 1 / 2 buttons | 82D4 |
| DP+4B, DP+4D | words | Previous controller 1 / 2 buttons | 82D4 |
| DP+55 | word | Offset after last used four-byte sprite record | 86A0 |
| DP+62 | byte | HDMA channel mask; restored after uploads; bits cleared with #$E7 in scene entry | 8324, 8B28 |
| DP+A8 | word | Deferred OAM address, uploaded then cleared | 8324 |
| DP+28 | byte | NMITIMEN shadow (OR #$81 enable / AND #$0E disable) | 8275, 8282 |
| DP+00..02 | bytes | Scratch: multiply product / long pointer | 86FC, 88EA |
| DP+03 | byte | Pointer-table index | 88EA |
| DP+06..08 | long | Source pointer for resolve/copy helpers | 88EA, 88C5 |
| DP+BA bit 7 | bit | Skip initial HDMA disable when set | 8324 |
| 0200–03FF | 512 bytes | Palette shadow, 256 two-byte entries | 8324, 86B6 |
| 0400–061F | 544 bytes | OAM shadow: sprite records and high table | 8324, 86A0 |
| 070F, 0711, 0713 | words | Transfer bookkeeping; reset after queue drain; $0713 advanced by 9B79 | 8324, 9B79 |
| 0715 | byte | Palette dirty: nonzero triggers upload, then cleared | 8324 |
| 0800 onward | eight-byte entries | VRAM DMA queue, stops at first inactive entry | 8324, 8711, 8724, 8B28, 9B36, 9B79 |
| 0B01,Y / 0B1C,Y | bytes | Actor attribute / type index for table 8AE5 | 8AD6 |
| 0D00–0D3F | 64 bytes | Control bytes cleared every fourth offset | 8B15 |
| 0D85 | byte | Packed attribute nibble from table $028584 | 86C8 |
| 01A0 / 01A1 / 01BD | bytes | Actor/map attribute selectors for nibble helper | 86C8 |
| 01D2 / 01D3 | bytes | Palette-table indices for dual load | 8921 |
| DP+04 / DP+05 | bytes | VRAM CPU-copy remainder length / page count | 8732 |
| DP+06 / DP+07 | bytes | VRAM CPU-copy destination word address | 8732 |
| DP+24 | byte | Scene-mode index (bit7 cleared) for ($8B54,X) dispatch | 8B28 |
| 0700 | byte | Scene setup marker written #$20 before $06F14D | 8DFE |
| 0702–0706 | bytes | Nested scene pointer fields from $06F2DE table | 8E26 |
| 01E7 | byte | Optional remap from table $02E41B when $01A0 < $30 | 8E26 |
| DP+10..12 | long | Resolved palette source for index-load copy | 8954 |
| 7E4800–7E4BFF | 1 KiB | Working buffer filled with $FFFF | 8996 |
| 0D7D | word | Cleared after $7E4800 fill | 8996 |
| DP+04..06 / DP+07 | long / byte | Descriptor pointer and copy length for queue enqueue | 9B36 |
| 0880 onward | bytes | Payload scratch filled by descriptor enqueue | 9B36 |
| 0713 / 0714 | bytes | Running DMA source offset advanced by enqueue | 9B79 |


| DP+83 / DP+85 | word / byte | Compressed-stream pointer offset / bank for C559 | 90C1, C559 |
| DP+86 / DP+88 | word / byte | Decompress dest base $9000 / bank $7E | C559, C68C |
| DP+89 | word | Decompressed byte length (DMA size / rearrange end) | C559, C68C, 90E6 |
| DP+8B / DP+8C | byte / byte | Flag byte / remaining flag bits for decompress | C559 |
| DP+8D / DP+8F | byte/word | Backref length / distance scratch | C559, C68C |
| DP+95..A4 | 16 bytes | Tile rearrange scratch (planar shuffle) | C68C |
| 7E9000 onward | bytes | Decompress / rearrange working buffer | C559, C68C, 90E6 |
| 1309–1310 | 8 bytes | Slot ID table; $80 clears / sentinel at $1309 | 04:85B6 |
| 01DE / 01E0 | words | Scroll snapshot copy of $01A2 / $01A4 | 8E76 |
| 01A1 | byte | Scene flag set to $80 in mode[1] path | 8E76 |
| 01C0 / 01C4 | words | Dual copies of $01A2 when A∈{$33,$35,$37} | 02:D812 |
| 01C2 / 01C6 | words | Dual copies of $01A4 | 02:D812 |
| 01C8 / 01CC | words | Dual copies of DP+$33 (BG2 H scroll) | 02:D812 |
| 01CA / 01CE | words | Dual copies of DP+$35 (BG2 V scroll) | 02:D812 |
| DP+$05 / $06 / $08 | bytes | Tiled enqueue width / height scratch | 9BBF |
| DP+$03 / $04 | word | Tiled enqueue VRAM dest ( += $20 per row ) | 9BBF |
| DP+$09 / $0A | word | Tiled enqueue source cursor (starts $0A00) | 9BBF |
| 0A00 onward / 0711 | bytes / word | Word-pair scratch / write cursor for tiled enqueue | 9BBF |

| DP+$33/$34 | word (split) | Clamped BG2 H scroll from $01A2 via $0703 | 06:F14D |
| DP+$35/$36 | word (split) | Clamped BG2 V scroll from $01A4 via $0702 | 06:F14D |
| 0702 / 0703 | bytes | Map extent counters (DEC→limit) for scroll clamp | 06:F14D |
| 0B07,X / 0B09,X | word / byte | Actor position accumulators updated from stream | 00:89B8 |
| 0B0A,X / 0B0C,X / 0B0E,X | word / word / byte | Actor field copies / second accumulators | 00:89B8 |
| 0B0F,X / 0B11,X | word / byte | Actor field copies from stream | 00:89B8 |

| 1648 | byte | Scene-entry gate; bit7 selects $01D8 specialty compares | 03:FB8D |
| 01D8 | byte | Specialty compare ($48 early RTL / $32/$36/$42 JMP handlers) | 03:FB8D |
| 01D6 bit6 | bit | When set, JMP $FD3D from scene-entry dispatch | 03:FB8D |
| 01A0 | byte | Scene-entry selector ($05 mode dispatch / $18 JMP $FC93 / else RTL) | 03:FB8D |

| 01A6 / 01A8 | words | Camera scroll snapshot of DP+$33/$35 (mode0/2) | 8B7A, 8E96 |
| 01AA / 01AC | words | Camera scroll snapshot of $01A2/$01A4 (mode0/2) | 8B7A, 8E96 |
| 0D66 / 0725 | bytes | Cleared during scene-mode[0] VRAM wipe prefix | 8B7A |
| DP+$61 | byte | Cleared after palette wipe in mode[0] prefix | 8B7A |
| DP+$62 | byte | HDMA enable mirror; mode[2] clears bits 3–4 | 8E96 |
| 0C40 | byte | Non-zero skips mode[2] scroll snapshot | 8E96 |
| 01D7 | byte | Scene-entry deep-path gate (bit7/bit6 specialty) | 03:FBDA |
| 1661 | byte | Compared to $0B / $0A in scene-entry deep path | 03:FBDA |
| 15FA | byte | Specialty compare $80 selects JMP $FC47 | 03:FBDA |

| DP+$0A / $0C | words | Actor world Y / X for OAM writeback | 00:849C |
| DP+$12 / $14 | words | Camera-relative Y / X (world − scroll) | 00:849C |
| DP+$0E | byte | On-screen Y clip threshold (often $F0) | 00:849C |
| DP+$04 / $06 / $08 / $09 / $10 | bytes | OAM size/xhigh scratch / temp X / attr / clipped Y | 00:849C |
| DP+$55 | word | Next free OAM shadow index ($0400) | 00:849C, 00:85E9, 86A0 |
| 0400–061F | (see above) | OAM shadow written by actor walk | 00:849C |
| 8589 / 8599 | ROM | OAM high-table set/clear nibble masks | 00:849C, 00:85E9 |
| DP+$0A / $0C | bytes (low) | Screen-space Y / X bases for parallel OAM | 00:85E9 |
| DP+$10 / $12 | bytes | Attr OR-mask / tile addend (entries $85E9/$85EF) | 00:85E9 |

VRAM entry layout:

| Byte offset | Meaning |
|---|---|
| 0 | VMAIN value; bit 7 also marks entry occupied; cleared on consumption |
| 1–2 | Destination VRAM word address |
| 3–4 | Source address low word |
| 5 | Source bank |
| 6–7 | Transfer byte count |

The upload routine uses DMA1 for OAM, DMA0 for a dirty palette, and DMA2 for
queued VRAM transfers. Tests verify register writes against original code;
the differential replay also exercises actual DMA side effects in the core.
Queue capacity remains to be recovered; producers at 9B36/9B79 are now mapped. Do not infer a
capacity from the test queue lengths or apply new bounds that change game logic.

| DP+$0A/$0B | word (split) | Queue fill: DP+$00/$01 ±8 when $0700 bit4 | 06:EF11 |
| DP+$04/$05 | word | Tile-fetch result / scratch (cleared on F082 exit) | 06:F089 |
| DP+$07/$08/$09 | long | Map-data long pointer built during tile fetch | 06:F089 |
| DP+$71 | byte | Raw map tile index from [DP+$04] | 06:F089 |
| 0736 | word | Masked map word ($3FDE) staging | 06:F089 |
| 0704 / 0706 | word / byte | Map base offset / bank for tile pointer | 06:F089 |
| 0707 / 0709 | word / byte | Secondary map base / bank after ×2 index | 06:F089 |
| $06F1EE,X | word table | Bank-6 tile→pointer table (X = tile×2) | 06:F089 |

| 01BD | byte | Specialty actor/attr id written $9E/$9F when $01A0 is $15/$14 | 8B7A |
| 0D87 | byte | Cleared during $01A0 specialty setup | 8B7A |
| 01BB | byte | Actor-slot base index; $94E7 uses <<5 as X stride | 94E7 |
| 0B00,X / 7E3800,X | byte | Actor slot flags cleared by $94E7 unless $0B01,X==$54 | 94E7 |
| DP+$37..$3A | bytes | BG1 scroll shadows cleared by $94B2 / $9485 tail | 9485 |
| 121F | byte | Set to $07 by scene specialty $FC47 | FC47 |
| 01E2 / 01D2 | bytes | Cleared on $FC74 success path before SEC/RTL | FC74 |
| 166C | word | Compared to #$0800 by $E340 early gate | E340 |

| 01D6 | byte | When ==$60, $A6CB clears it and runs $E419/$E4F4 | A6CB |
| 01BC | byte | Cleared by $A6CB success path; nibble-masked in mode0 BMI path | A6CB, 8B7A |
| 0707 / 0709 | word / byte | Decompress source pointer set by mode0/2 tails | 8B7A, 8E96 |
| DP+$83 / $85 | word / byte | Secondary long-pointer set before $C559 | 8B7A, 8E96 |
| 074C | word | Set to $FFFF on mode0 BMI path | 8B7A |
| 01E7 bit4 | bit | Selects mode2 decompress pointer bank/addr | 8E96 |
| 0C40 / 0C36 / 0C37 / 0C42 / 0C35 | bytes | Cleared by scene wipe $E837 | E837 |
| 10C1 / 10C2 / 0726 / 0727 / 0EA1 / 01EE | bytes | Cleared by scene wipe $E837 | E837 |
| 0EA3 / 0EA0 | bytes | Seeded $15 / $01 by $E340 continue path | E340 |
| 1000,Y | bytes | Cleared stride-6 by $9255 via $8B07 | 9255 |

| 1648 / 01D8 | byte / byte | $EF29 early-exit when $1648 bit7 set and $01D8==$04 | EF29 |
| 0B00,Y bit7/6 | bits | Occupied / sticky; weak clear when bit7 set and bit6 clear | EF29, E42A |
| 0B12/13/18/19,Y | bytes | Cleared on weak occupied actors before $E42A | EF29 |
| 0B1C,Y | byte | Actor type index; scales MVN dest/src by <<5 | E42A, E461, E4D1 |
| 7E2800+type<<5 | 32 bytes | Per-type actor backup slot written by $E42A / read by $E461 | E42A, E461 |
| 7E4D00–7E4E1F | 288 bytes | Full actor-table backup; cleared by $EFC4, restored by $EF29 MVN | EF29, EFC4 |
| DP+$10 | byte | Occupied-slot count ($EF29) / restore count ($E4F4) | EF29, E4F4 |
| 0190,X | bytes | Collected high-actor types ($0B00>=$C0), $FF terminated | E4D1, E4F4 |
| 0D9B bit6 | bit | Enables wipe-gate $E942 body; cleared when taken | E942 |
| 0170 bit3 | bit | Wipe-gate predicate with $01A0==$09 and $01BB>=$08 | E942 |

| 0B17 / 0B1B,Y | bytes | Attr bytes from $02B21D/$02B214 ×$0E product | E8EE |
| 0B0E,Y | byte | Set to $01 when type $54; else loaded from [DP+$00],Y | E8EE |
| 0B00,Y=$C0 / 0B01=$54 / 0B1C=$0C | bytes | Wipe-actor identity seeded by $BB46 | BB46 |
| 0B03/0A=$0033 / 0B05/0C=$0006 | words | Wipe-actor X/Y coords | BB46 |
| 7E29A0–7E2ABF | 288 bytes | Cleared by $EFD7; source for $E36B→$0B00 MVN | EFD7, E36B |
| 7E3A00–7E3B1F | 288 bytes | Cleared by $EFEA; dest for $E36B←$0B00 MVN | EFEA, E36B |
| 7E2B00–7E2B9F | 160 bytes | Source for $E36B→$0E00 MVN | E36B |
| 7E3B20 / 7E2AC0 | bytes | $01BB backup / restore around MVN body | E36B |
| 7E2AE0 / 7E2AEC / 7E2AED | bytes | Restore $1648 / $0C36 / $0C37 | E36B |
| 7E2AF0–7E2AF7 / 7E2AE2–7E2AEB | words | Scroll-word backup/restore ($01A2/$A4/$AA/$AC) + $10C1 | E36B |
| 018F | byte | Swapped with $01BC by $E32E epilogue | E32E |
| 0C41 / 0EA2 | bytes | Seeded $49 / $20 by $E36B tail | E36B |
| 01D7 bit6 | bit | Cleared by $E36B before JMP $E32E | E36B |

