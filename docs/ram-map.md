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

| DP+$12 / $13 | word | Actor-type×1 scratch for ×3 index in slot walk | 00:8DAC |
| $008DE4 | words | Per-slot (Y>>4) parameter words for $01C9F9 | 00:8DAC |
| $008DF6 | bytes | $0D59-indexed values written to $01E3 | 00:8C84 |
| 01E3 | byte | Scene specialty / mode seed (from $8DF6 or #$04) | 00:8C84 |
| 0D55 / 0D57 | words | Written #$0001 when $01BC bit0 clear in mode[0] cont. | 00:8C84 |
| 0D59 | byte | Index into $008DF6 for $01E3 | 00:8C84 |
| DP+$00 / $02 | long | Scratch pointer preset $00C000 before $01C535 | 00:8C84 |
| 01BC bits | nibble/bits | Low nibble / bit0 / bit4 via $01E7 / bit6 select palette&gfx idx | 06:EAD4, 06:EB52 |
| 01E7 | byte | Bit0/bit4 contribute fixed palette/gfx indices 5/2/6 | 06:EAD4, 06:EB52 |
| 1648 / 164B | bytes | When bit7 set, use $164B as index; else $01A0→$02E44B | 06:EAD4, 06:EB52 |
| $02E44B | bytes | Scene→index table | 06:EAD4, 06:EB52 |
| $02E47B | bytes/words | Palette idx (m8) / packed idx word (m16) | 06:EAD4, 06:EB52 |
| $02E4AA | 3-byte ents | Palette long-pointer table (bank $02) | 06:EAD4 |
| $02E489 | 3-byte ents | Compressed-stream long pointers (addr at +0, bank at +2) | 06:EB52 |
| DP+$25 / $26 / $27 | bytes | Scene body seeds (#$03 / 0 / #$02) | 00:8F46 |
| 0716 / 0D66 / DP+$61 | bytes | Written #$61 / #$23 / #$01 in scene body | 00:8F46 |
| 0B12,X | bytes | Cleared for X=0..$100 step $20 | 00:8F46 |
| 07BC / 07BB / 0C40 | bytes | If $07BC bit7 set: clear it and copy $07BB→$0C40 | 00:8F46 |

| 1303 | byte | Specialty mode; bit7 set when $01E7 bit3 forces resync via $04844A | 00:8D4E |
| 01D2 / 01E2 | bytes | Specialty table low from $028484 or $01D4&7 override | 00:877E |
| 01D3 | byte | Specialty table high from $028484+1 or $0284E4[$01BD] | 00:877E |
| 01D4 | byte | If bit7 set, low 3 bits override $01D2/$01E2 | 00:877E |
| $028484 | bytes | Pair table indexed by $01A0<<1 | 00:877E |
| $0284E4 | bytes | Alt table indexed by $01BD when $01A1 bit7 set | 00:877E |

| DP+$4F | byte | Scroll blit index; low3 must be 0; (>>2)&6 indexes $06EC0A | 06:EBC1 |
| $06EC0A | words | Four base offsets ($D140/$D1C0/$D1E0/$D1C0) + bank #$0C | 06:EBC1 |
| $0280 | bytes | Destination of $20-byte long-pointer blit | 06:EBC1 |
| 0715 | byte | Set to #$01 after successful EBC1 blit | 06:EBC1 |
| 1306 | byte | Specialty command id from A at $85A1 entry | 04:85A1 |
| 1308 | byte | Cleared by $85A1 | 04:85A1 |
| 1318 | byte | High A (after XBA) when command == $88 | 04:85A1 |
| 1304 / 1305 | bytes | Cleared during $844A bit7 sync | 04:844A |
| 1302 | byte | Selects $85A1 arg #$86 (zero) vs #$87 (nonzero) | 04:844A |
| DP+$1E / $1F | bytes | Stream index / specialty id during $87F9 | 00:87F9 |
| DP+$22 | word | Saved Y cursor across decompress/DMA in $87F9 | 00:87F9 |
| DP+$20 | word | VRAM dest from $0283C4+2 for $87F9 | 00:87F9 |
| $028302 | words | Specialty-id → stream pointer table (bank 2) | 00:87F9 |
| $0283C4 | long pairs | Per-index source word + VRAM dest word | 00:87F9 |
| $028470 | long pairs | Alt DMA length/source when stream idx ≥ $26 | 00:87F9 |

| DP+$73 / $75 | long | APU transfer source pointer (bank in $75) | 04:8490, 04:85F1 |
| 1300 | word/byte | APU transfer phase; 0 triggers bank bump on size-0 | 04:8490, 04:85F1 |
| $2140–$2143 | APU ports | Handshake/transfer mailbox ($BBAA / $CC / data) | 04:84E2, 04:85F1 |
| $4210 | PPU status | BIT NMI-flag wait during APU handshake | 04:84E2 |
| $4200 | NMITIMEN | Cleared around APU boot transfer | 04:8490 |

| 0C35 | byte | Bit7 cleared in scene continuation $8FB2 | 00:8FB2 |
| 0C41 | byte | Set #$20 when $1648 clear and $01AE negative | 00:8FB2 |
| 0C40 | byte | Nonzero → early JMP $9083 from $8FB2 | 00:8FB2 |
| 01BC | byte | Bit5 set when bit0 was set during $8FB2 | 00:8FB2 |
| $2132 | COLDATA | Written #$E0 in $8FB2 | 00:8FB2 |
| 0D5F / 0D67 | bytes | Seeded #$10 / polled until bit7 set in $8FB2 wait | 00:8FB2 |
| 1303 | byte | Written #$82 before $04844A in $8FB2 | 00:8FB2 |
| 073C | byte | Set #$02 before $01F455 when $01D6 bit6 clear and $01AE neg | 00:8FB2 |
| DP+$00 | byte | Search key for $03F021 actor $0B1C,Y compare | 03:F021 |
| 0B00,Y / 0B1C,Y | bytes | Type / match field walked by $F021 | 03:F021 |
| 0B01,Y | byte | Type; $F06F rewrites $3A→$39 | 03:F06F |
| 0C36 / 0C37 | bytes | Gate / cleared on $F06F rewrite | 03:F06F |
| 1306 / 1307 / 1308 | bytes | APU command / last echoed / retry counter | 04:8514 |
| 1318 | byte | Extra byte to $2143 when command == $88 | 04:8514 |
| 1309–1310 | bytes | 8-byte APU ID queue (shifted by $8514) | 04:8514 |
| 1311 / 1313 | bytes | Last ID sent to $2141 / handshake phase | 04:8514 |
| $2140 / $2141 / $2143 | APU | Command echo / queue ID / $88 payload | 04:8514 |
| 01B2 | byte | Loop count (DEC→DP+$00) for $01C535 | 01:C535 |
| $0100,Y / $0101,Y | bytes | Index / rewritten flag byte | 01:C535 |
| $01C584 | bytes | Flag-template table indexed by $0100,Y | 01:C535 |
| 0EA3 | byte | When ==5 and index==$1A → force template #$84 | 01:C535 |
| DP+$10 | byte | #$80/#$40 mask from $0061 for bit0 OR | 01:C535 |
| 0D66 / $0061 | bytes | Early-exit / path select for $01B61E | 01:B61E |
| $7E3000,X | words | Cleared or filled #$3D40 by $B61E/$B647 | 01:B61E |
| 0D75 | byte | Cleared at end of $01C9F9 | 01:C9F9 |
| 0D77 / 0D79 | word/byte | Gfx stream pointer (word + bank) for $C9F9/$CA98 | 01:C9F9 |
| DP+$03 | byte | Nonzero → VRAM window $9100 else $9000 in $C9F9 | 01:C9F9 |
| DP+$10 / $14 | bytes | Bitmask AND operands from tables in $C9F9 | 01:C9F9 |
| DP+$12 | word | Actor type (from $8DAC) indexing $C9F9 tables | 01:C9F9 |
| $02A3B4 / $02A552 | long | Stream pointer tables (word+bank) by type×3 | 01:C9F9 |
| $02A57C / $02A595 | bytes | Bit fields vs $039B9B[type&7] | 01:C9F9 |
| $0800,X queue | bytes | Filled by $CAAB (occ/src/dest/bank/len) | 01:CAAB |

| DP+$0E | word | Saved X bound for $B65D zero-fill after $B647 | 01:B65D |
| $7E3000,X | words | Zeroed by $B65D down to DP+$0E | 01:B65D |
| 0D66 | byte | ==$1D → early RTL for $B67A (same gate as $B61E) | 01:B67A |
| $0800,X queue | bytes | $B67A seeds occ/$2C00/$3000/$7E/#$0800 | 01:B67A |
| $7E4000,X | words | Filled #$00FF by $B6AC | 01:B6AC |
| $0800,X queue | bytes | $B6BF seeds occ/$6800/$4000/$7E/#$0800 | 01:B6BF |
| 0D74 | byte | Cleared by $B6BF after queue seed | 01:B6BF |
| 0EA1 | byte | Set #$80 in $9083 path | 00:9083 |
| 0C41 | byte | Negative → $8996 + STA #$2137 at $7E4800 in $9083 | 00:9083 |
| 0D67 / 0D7D | bytes | Wait flag / cleared word beside $7E4800 seed | 00:9083 |
| 0171 / 0173 | bytes | Bit3 set / OR #$30 during $FD64 continue | 03:FD64 |
| 0EA3 / 15FA / 15F1 | bytes | Type seed / gate / $4F–$50 vs $68–$69 select | 03:FD64 |
| 01AA/01A6/01AC/01A8 | words | Scroll seeds #$07B8/#$0738/#$0358/#$02E0 | 03:FD64 |
| 0190,X / 01BB | bytes | Compacted type list / recount after shift | 03:FD64 |
| 0C40 | byte | Cleared by $FD64 continue path | 03:FD64 |
| $4212 | STAT78 | BIT bit7 wait in $A95B | 00:A95B |

| $7E8000,X | words | Filled #$00FF by $B6ED | 01:B6ED |
| $0800,X queue | bytes | $B700 seeds occ/$6000/$8000/$7E/#$1000 | 01:B700 |
| 0D9B | byte | If negative, $F802 rewrites #$40 and RTL | 01:F802 |
| $0E14,Y / $0E16,Y | word/byte | Slot prep outputs from $F802←$0594A1 | 01:F802 |
| 0173 bit7 / 1303 | bit/byte | Cleared / set #$82 by $F375 success | 03:F375 |
| 07BD / 07BE / 07BF / 07C0 | bytes/word | Snapshot of $1648/$01D8/$01D9/$01DC by $FE0E | 03:FE0E |
| 01D9 | byte | Cleared by $FE0E after snapshot | 03:FE0E |
| DP+$76..$7B | bytes | Mode-7 A/D/X/Y source for $A961→$211B–$2120 | 00:A961 |
| $211B–$2120 | M7 regs | Matrix + center written by $A961 | 00:A961 |
| $420C / $0062 / $0055 | HDMA/scratch | Cleared by frame-tail $8674 | 04:8674 |
| 133D / 12DC | bytes | Cleared by $8674 before DP+$24→$25 | 04:8674 |

| DP+$20 | word | Slot-walk X cursor advanced +#$0100 per $F4B2 iter | 01:F4B2 |
| $0E00,Y bit6 | flag | Occupied → $F983/$FB2A (Y<$60) or $9D2A (Y=$60/$80) | 01:F4B2 |
| DP+$00/$02 | long | $F983 builds bank-$02 pointer from $02A597 | 01:F983 |
| $7E5000,X | bytes | $FB2A expands [DP+$00] records; term #$80 | 01:FB2A |
| $01FBB3,X | table | Nibble→attr for $FB2A | 01:FB2A |
| $4360–$4375 | HDMA | Mode-7 channels programmed by $B960 | 00:B960 |
| DP+$3B..$42 | bytes | BG3/BG4 scroll sources for $AB00 | 00:AB00 |
| $2111–$2114 | BG scrolls | Written by $AB00 | 00:AB00 |
| $0800,X queue | bytes | $8938 seeds occ/$0000/$0000/$7F/#$8000 | 04:8938 |
| 0715 | byte | Inc by $8938 after queue seed | 04:8938 |
| $2105/$2101/$212C | PPU | Mode-7/obj/TM presets by $89E2 | 04:89E2 |
| $1200..$1218 | words | Mode-7 matrix/center seeds by $89E2 | 04:89E2 |
| 0D66 | byte | $B72B dispatch index (==$29 early RTL) | 01:B72B |
| $02E0F2,X | long words | Table pairs for $B72B → DP+$00/$02 | 01:B72B |
| 01B3 | byte | Typed seed written on $0D66=$0B path | 01:B72B |

| $0E01,Y | byte | Type tested by $F660 gate ({$10,$11,$4C,$4D,$4F} -> SEC) | 01:F660 |
| DP+$83/$85 | long | $F67B seeds #$EB84 / #$08 before $C559 | 01:F67B |
| $2115 | VMAIN | Set #$80 by $F67B | 01:F67B |
| $0D01 / $0D7A / $0D75 | byte/word/byte | $F53B SEC-path seeds before $C987 | 01:F53B |
| $7F0000,X | bytes | $F53B bulk copy of $0E01,Y (or 0) then DMA bursts | 01:F53B |
| $0E0F,Y | words | $F53B multiply-table fill from $01FC2D | 01:F53B |
| DP+$00 | word | $FC00 scale result from $0E0F/$0E10,Y | 01:FC00 |
| DP+$02/$03 | byte/byte | $FBBC temp (table+4)&$FE / zero before $FC00 | 01:FBBC |
| DP+$10 | byte | $FAE9 saved table index (entry A) | 01:FAE9 |
| DP+$00/$02 | long | $FAE9 stream pointer (bank $02) after table lookup | 01:FAE9 |
| DP+$03/$05 | long | $FAE9 temp $02:($AB20+A*2) then length (A+4)*2 | 01:FAE9 |
| $7E4000,X | words | $FAE9 blit destination | 01:FAE9 |
| $01FD10,X | byte | $FBBC per-type table byte (PHA → $FAE9) | 01:FBBC |
| $0D75 bit7 | flag | $C987 BPL early RTL when clear | 01:C987 |
| $0D75 bits6..4 | flags | $C987 ASL dispatch → $C9F9 / $C9D9 / $CA34 | 01:C987 |
| $0D40,Y | byte | $C987 XBA/ADC scratch written per nibble index | 01:C987 |
| $02A4EF,X | long | $C9D9 stream pointer table (word+bank) | 01:C9D9 |
| $02A58F,X | word | $C9D9 bitmask table before BRA $CA17 | 01:C9D9 |
| 122A | byte | $8974 gate/counter (<$10 queue; ==$10 -> $B67A; >$10 -> #$80) | 04:8974 |
| $0800,X queue | bytes | $8974 seeds occ / ($122A&$1F)<<10/<<11 / $7F / #$0800 | 04:8974 |
| 0715 / $212C | byte/reg | Inc when $122A hits 2; TM=#$11 when hits 4 | 04:8974 |
| DP+$00/$01/$02 | bytes/word | Width / height / $7E3000 index for $B55A | 01:B55A |
| $7E3000,X | words | Pattern tiles #$2832-2836 / #$2801 / #$283B-283D | 01:B55A |
| 07B4 | byte | Optional mid-tile gate; cleared by $B55A epilogue | 01:B55A |

| 121B | byte | Frame-gfx index for $871E (bit7 picks $90FD vs $8FEF table) | 04:871E |
| DP+$00/$02 | long | $871E table/stream pointers (bank $04) | 04:871E |
| $0083/$0085 | long | $871E decompress stream seeds before $C559 | 04:871E |
| DP+$73/$75/$78 | long/byte | $871E APU/stream + palette-source pointer; copy to $0200 | 04:871E |
| $0200,Y | words | $871E palette-word blit from [DP+$73] | 04:871E |
| $0E0F,Y | words | $FA1A adjusts via $01FD79 then fills from $01FC87 | 01:FA1A |
| $01FD79,X / $01FCA7,X / $01FC87,X | tables | $FA1A adjust/fill sources | 01:FA1A |
| DP+$00/$02 | long | $C8C8 builds bank-$02 then $C885 forces bank $09 | 01:C8C8 |
| DP+$03 | byte | $C885 palette-slot index (×16 into $0300) | 01:C885 |
| $0300,X | words | $C885 16-word palette copy destination | 01:C885 |

| DP+$73/$75 | long | $889E fill destination ($0000,$7F) | 04:889E |
| $7F0000,Y | bytes/words | $889E $FF then $00FF fill | 04:889E |
| DP+$73/$75 | long | $8411 source $7E9000 | 04:8411 |
| $7F0000,X | bytes | $8411 dest with +$C0 every 64 bytes | 04:8411 |
| DP+$86/$88/$89 | long/len | $83B2 source pointer + length | 04:83B2 |
| DP+$91..$94 / $8F / $8D / $73/$75 | scratch | $83B2 bitplane shift / end / stride | 04:83B2 |
| $7F0001,X | bytes | $83B2 unpacked bitplane dest | 04:83B2 |
| DP+$73/$76/$79/$7B | longs/words | $8282 stream src/dst/count/addend | 04:8282 |
| $82A3,X | word table | $8282 mode vectors $82AB/$82C0/$82D5/$82EB | 04:8282 |

| $0D91 | byte | $A99B BMI gate into $A9B1 | 00:A99B |
| DP+$48 bit4 | flag | $A99B BIT gate (with $0D91) before RTS | 00:A99B |
| DP+$7C | byte | $A9B1 nonzero → compare loop; zero → JMP $A153 | 00:A99B |
| $7004E0,X / $AAD9,Y | bytes | $A9C1 password-style compare window | 00:A99B |
| $0300,X | words | $FA86 clears $001E bytes of palette shadow | 01:FA86 |
| $7F2800,X | long×6 | $FA86 slot pointers (word+bank) | 01:FA86 |
| $01FADF,X | words | $FA86 dest offsets into $0200 ($0120/$0140/$0160/$0040/$0060) | 01:FA86 |
| 0715 | byte | $FA86 increments on completion | 01:FA86 |
| $7F0000,X | byte | $BE54 per-slot type before $BEAB | 05:BE54 |
| $05BEA1,X | words | $BE61 dest base table ($0000..$0020 step 8) | 05:BE54 |
| DP+$86/$88 / DP+$12 | long/word | $BE61/$BEAB pointer + scaled index | 05:BE54 |
| $1ECB06/$08/$09,X | tables | $BE61 pack words + $BEAB scale source | 05:BE54 |
| $7F2800,Y | bytes | $BE61 writes packed triple after DB=$7F | 05:BE54 |
