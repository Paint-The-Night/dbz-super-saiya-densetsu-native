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

| $7E9000..$7E97FF | WRAM | Cleared then DMA-sourced by $A9E9/$AE38 | 00:A9E9 |
| $7E9640 / $7E9688 / $7E96C8 | WRAM | Mode-7 epilogue pattern + $AAE1 expand | 00:A9E9 |
| $7E6000.. | WRAM | HDMA table copy target from $AAF7 via MVN | 00:A9E9 |
| $4300–$4304 / $420C | DMA/HDMA | Ch0 CGADSUB HDMA during $A9E9 wait loop | 00:A9E9 |
| $4340–$4345 / $420B | DMA | Ch4 VRAM upload programmed by $AE38 | 00:AE38 |
| DP+$3D | word | $A9E9 must equal #$0008 to continue body | 00:A9E9 |
| DP+$82 / DP+$73 / DP+$48 | bytes | Epilogue toggle / frame counter / exit bits | 00:A9E9 |
| $158060,X | long | $C029 source pointer table | 05:C029 |
| DP+$00/$04/$83/$86/$89 | long/words | $C029 stream pointers + $96F3 length | 05:C029 |
| $2115 | VMAIN | Set to #$80 by $96F3 before decompress JSLs | 05:96F3 |

| $2181/$2183 / $2180 | WMADD / WMDATA | $C5ED seeds from DP+$86/$88; streams decompressed bytes | 00:C5ED |
| $7E9000,X | bytes | $C5ED working buffer (same as $C559) alongside WMDATA | 00:C5ED |
| MVN dest bank $7F | bank | $C707 tile rearrange target (vs $C68C → $7E) | 00:C707 |

| DP+$10/$14 | words | $BECB saves Y; slot index for $05C011/$C019/$C021 tables | 05:BECB |
| DP+$86/$88/$16 | long/word | $BECB stream base (bank $7F) + length snapshot before $C029 | 05:BECB |
| $0800,Y | VRAM queue | $BECB seeds occupied entry after $8711/$8724 | 05:BECB |
| $15E9,X | word | $BECB stores queue Y index per slot | 05:BECB |
| $2180/$2181/$2183 | WMDATA/WMADD | $BECB opcode walk + $C00A byte poke | 05:BECB |
| $7F6801/$6802,X | bytes | $BECB post-stream flag OR #$80 when DP+$14≥4 | 05:BECB |
| $158063/$158065,X | long | $BECB secondary stream pointer after second $BEAB | 05:BECB |
| DP+$00 (bank $01) | long | $F9A0 leaves $F983 pointer for $FB2A expand at X=0 | 01:F9A0 |

| DP+$10 / $0020,Y | word/byte | $C07B saves Y; type for $1ECB06 scale | 05:C07B |
| $1ECB06/08/09,X | long | $C07B palette ptr/bank + $0034,Y flag | 05:C07B |
| $0300,X / $16E1,X | words | $C07B 16-word blit; mirror when $20≤X<$60 | 05:C07B |
| 0715 | byte | $C07B increments after palette blit | 05:C07B |
| $7F0000 / $0E01 | byte | $C232 seeds both to #$05 before $BE54 | 03:C232 |
| DP+$14 / $059B89,X | word | $9AFF slot index + Y table before $BECB | 05:9AFF |
| DP+$B7/$B9 | bytes | $9AFF scene-id gates ($34/$35/$49 + sign) | 05:9AFF |
| $0020,Y / $0021,Y | bytes | $9AFF type BMI / $BECB sign gate | 05:9AFF |

| $0022–$0028,Y | bytes/words | $9B37 copies facing/attr/coords into DP+$03/$09/$0C/$0A | 05:9B37 |
| DP+$14 / $05C021,X | word | $9B37 long pointer base (bank forced #$7F) before $9B91 | 05:9B37 |
| DP+$0E | byte | $9B37 stores #$A8 before Y-bounds gate to $9626/$85B9 | 05:9B37 |
| $0021,Y / DP+$20 | byte | $9B91 type→$059C0B flags after −6 clamp | 05:9B91 |
| $0034,Y / $059C17,X | byte/words | $9B91 nibble index; ± adjust DP+$0A/$0C | 05:9B91 |
| $153B / $153C | bytes | $C0ED slot enables ($80 clears); X=#$0120/#$0140 | 05:C0ED |
| DP+$4F | byte | $C116 bit2 selects blend; bits1:0 abort blend to RTS | 05:C116 |
| $15E1,X / $0200,X | bytes | $C116 simple 32-byte palette slot copy | 05:C116 |
| $05C1B8/BA,X | words | $C116 blend addends (epilogue bytes double as table) | 05:C116 |
| DP+$00/$02/$04/$06/$10 | words | $C116 blend scratch (index, loop, channels) | 05:C116 |
| 0715 | byte | $C116 increments after copy/blend | 05:C116 |

| DP+$00 long / DP+$55 | ptr / word | $9626 sprite stream + OAM cursor; records flags,Y,tile,attr,X | 05:9626 |
| DP+$03/$09/$0A/$0C/$0E | bytes | $9626 facing/or-attr / screen Y,X / Y-threshold (often #$A8) | 05:9626 |
| $0400,X / $0600,Y | OAM | $9626 low table + high-table nibble via $0085A9,X masks | 05:9626 |
| DP+$86/$88 | word/byte | $96ED seeds stream ptr bank #$7F before $96F3 | 05:96ED |
| $0D8200,X | byte | $B572 type table after A&$7F | 05:B572 |
| DP+$20,X / DP+$00,X / $01EE | bytes | $C1C8 type → $B572; flag bits → #$80–#$83 | 05:C1C8 |

| $059B89,X / $0029,Y | word/byte | $C1EE/$C20D actor Y + timer (dec / scene-$13/$14 clear) | 05:C1EE |
| $1474 / $0021,Y | bytes | $C1EE scene id gate; optional OR #$80 after timer dec | 05:C1EE |
| $05C273,X / DP+$10 | words | $C1EE ASL(timer) mask table → AND scratch | 05:C1EE |
| $15E9,X / $0803–06,Y | word | $C1EE queue index → long ptr + length for AND walk | 05:C1EE |
| DP+$00 long | ptr | $C1EE AND-mask destination (bank from queue) | 05:C1EE |

| $1546 | byte | $E38E gate (<$8A) and post-loop INC; AND #$7F → DP+$03 scale | 05:E38E |
| $05E40E,X / DP+$00 | words/long | $E38E sprite-stream pointers (bank #$05) before $9626 | 05:E38E |
| $05E400,X / $05E3F2,X | bytes | $E3CC signed scale factor + base offset for ±$4216 | 05:E3CC |
| $4202/$4203/$4216 | mul | $E3CC hardware 8×8 product | 05:E3CC |
| $153B/$153C | bytes | $86DE/$86F5 stores dual $C1C8 results | 05:86DE |
| DP+$AB/$AC | bytes | $86DE INC $AB / STZ $AC after flag push | 05:86DE |
| $1480/$14A0 | bytes | $86DE $69 keep / force #$80 sync | 05:86DE |
| $1429/$1469 | bytes | $86DE seeds #$0F before $B045 | 05:86DE |
| $1558–$155D,X | bytes | $1D:8807 RNG-seeded slot fields when $1559,X was 0 | 1D:8807 |
| $1570,Y / $1571,Y / $1573,Y | bytes/words | $1D:8843 timer + position words; BMI on $1570 | 1D:8843 |
| DP+$0A/$0C | words | $1D:8843 scale scratch (ASL/ROR/×3) before add | 1D:8843 |
| $1D8968/69/6B,X | byte/words | $1D:8843 type + ΔX/ΔY table | 1D:8843 |
| DP+$AB/$AC/$B0/$B6 | bytes | $8857 seeds after optional $C07B | 05:8857 |
| $0B19,Y / $0E19,Y | bytes | $9768 clears #$80 markers via $96E2 walk | 05:9768 |
| $17C6,Y → $0140,Y | bytes | $9768 12-byte copy before scroll seed | 05:9768 |
| $0738/$073A / $071B | word/bytes | $9768 scroll pointer bank #$07; clear $071B | 05:9768 |

| $0020,Y .. +$1F | bytes | $B110 clears 32-byte actor header window | 05:B110 |
| $1421/$1461 | bytes | $B045 seeds #$85/#$97/#$80 via $B0BE/$B0F9/$B582 | 05:B045 |
| $1426/$1466 | words | $B045 #$0100/#$0070 from $B582 Z | 05:B045 |
| $142E–36 / $146E–76 | bytes | $B045 clears paired velocity/scroll temps | 05:B045 |
| $14E4 / DP+$AC/$B7/$AB | bytes | $90CC $E8/$EA gates; INC $AC; STZ $B7; AB=#$82/#$01 | 05:90CC |
| $0020–26,Y | words | $90CC REP seeds #$A40F/#$0042/#$0040/#$0070 before $C07B | 05:90CC |
| $7F0000,X / $0D00,X | bytes | $8116 optional wipe + $0D00 clear before bank-1D JSLs | 05:8116 |
| $1541/$1502–05 / $0D66 | bytes | $8116 scene seeds after dual $C07B | 05:8116 |
| $02C0→$0200 / $212C | words/byte | $8116 palette word copy + TM clear before RTL | 05:8116 |
| DP+$00 long (bank #$10) | ptr | $1D:8906 from $1092B1,X | 1D:8906 |
| DP+$03/$09/$0E / $0C | bytes | $1D:8906 ORA-seeded facing; optional negate of DP+$0C | 1D:8906 |
| $0033 / $17DB | words | $1D:8906 temp swap around JSL $0085B9 | 1D:8906 |
| $1570 | byte | $1D:8906 STZ when DP+$0D==#$80 | 1D:8906 |

| $1420/$1422/$1432/$1424 | bytes/words | $B0BE copies $1401 (or $14D5 when $B7==#$44); seeds attr/face/X | 05:B0BE |
| $1460/$1462/$1472/$1464 | bytes/words | $B0F9 copies $1441; seeds #$0145/#$00C0 partner slot | 05:B0F9 |
| $0D8200,X | byte | $B582 type table bit7 → EOR #$80 (predicate sibling of $B572) | 05:B582 |
| $071B | byte | $9488 nonzero → PLA×3 discard JSL frame then RTL | 05:9488 |
| $14C2 bit0 | flag | $1D:8A77 selects Y=#$1400 vs #$1440 | 1D:8A77 |
| $14C2 bit0 | flag | $05:B6B2 same-polarity / $B6C0 inverted Y=#$1400 vs #$1440 | 05:B6B2 |
| $1640–$1645 / $7E3000 grid | bytes/words | $93A9 face-id seeds and $7E3000 pattern fill before JML $89AC | 05:93A9 |
| $1552–$1557 / DP+$04 | bytes | $DF60 face-adjust seeds after $1D8FEC table scale | 05:DF60 |
| $1540 / $1546 / ($E117,X) | bytes/table | $E102 dispatch index and BMI gate | 05:E102 |
| $1527/$1528 / $14CC–$14D1 | bytes | $A435 probe clears and scene gates | 05:A435 |
| $1D0101,X | byte | $8AB3 list clear before JML $01C639 | 1D:8AB3 |
| $1481–$1485 / DP+$4F/$50 | bytes | $8ABD flight facing / scroll nudge | 1D:8ABD |

| $0B00,X | byte | $97C9 keeps only #$C0 bits via $96D7 walk to #$0120 | 05:97C9 |
| $14F3/$14F4 | byte/word | $97C9 clears after actor mask walk | 05:97C9 |
| DP+$24 / $0716 | bytes | $97C9 scene result &#$7F; always STA #$81 to $0716 | 05:97C9 |
| DP+$20 / $0031,Y / $0021,Y | words/bytes | $CA10 slot scratch; timer tick + #$84/#$83 face | 05:CA10 |
| $1506 / DP+$51/$B8 | bytes | $CA10 ORA #$01; optional $B8==#$C0 → DP+$51 tweak | 05:CA10 |
| DP+$AC / $0726 / $14E4 | bytes/words | $9115 phase + timer; $14E4 word gates $9488 early path | 05:9115 |
| $14C2/$14FC / $0C40 | bytes | $9115 AC==1 special seeds after $AE81/$C07B | 05:9115 |

| $17EA..$17F3 | bytes/words | $B00A seeds from #$0A / $0E03 / $0E0A around JSL $1C806C/$1C8136 | 05:B00A |
| $17DD/$17E1/$14FF/$17E5 | words | $9886 clears/seeds from $01E7/$01D6/$17D2; DP+$B0 ← Y | 05:9886 |
| DP+$24 / $14E8/$14C1 | bytes | $C41F indexes $05C450[$14C1−#$10]; optional $14E9→$14E8 | 05:C41F |
| $15FA / $01E7 | bytes | $90A5 SEC predicate ($15FA==#$80 or LSR $01E7×2) | 05:90A5 |
| $0022,Y / DP+$04/$05 | bytes | $DA64 face merge (slot bit from Y==$1400) | 05:DA64 |
| $1502..$1505 | bytes | $C9FA clears; also via $DFE3/$C98F | 05:C9FA |
| DP+$B7/$B8/$B9 | bytes | $C98F stores type and clears pair/attrs | 05:C98F |
| $01B2 / $1D0100 list | byte / words | $1D:8A85 scan / $8AA2 append | 1D:8A85 |
| $0EA0 / $0E00,Y | byte / records | $01:F692 fills actor slots from $02BF0B×$0EA3 | 01:F692 |
| DP+$00/$02/$04/$06 | long/words | $CA90 bank-$1E stream ptr + opcode operands before ($CAFA,X) | 05:CA90 |
| $0033,Y / $0035,Y | byte/word | $CA90 stream-class select / base addend | 05:CA90 |
| $002C,Y / $0030,Y / $002D,Y | bytes | $DEC3 script gate/cursor/timer; $DF51 advances cursor | 05:DEC3 |
| $0E00,Y fields / DP+$12 | records/word | $F719 fills scale words from $02B214 tables; DP+$12 scale factor | 01:F719 |

| DP+$B6 | byte | $B124: (~$14C2&1)+3 seed used by battle/setup | 05:B124 |
| $14D2 | byte | $9491/$A9F4 bit-clear mask from $05AA0B scan | 05:9491 |
| DP+$B8 | byte | $D521 ORA #$80/#$40 facing/script flags | 05:D521 |
| $1546 / $17DB/$17DD | bytes/words | $F3AA phase and temp seeds around $F998 | 05:F3AA |
| $1700 / $14CF/$14D1 | bytes | $BDDC gate and coordinate subtract | 05:BDDC |
| $0C35 / $14D8/$14DA | bytes | $8B46 flight event list seeds | 1D:8B46 |
| face class at $0021,Y | byte | $95F6 → APU #$1C/#$1D via $0485B6 | 05:95F6 |
| $002B,Y / DP+$04 | byte/byte | $D566 script counter INC + wrap CMP | 05:D566 |
| $0021,Y / DP+$05 | bytes | $D531 face rewrite (optional #$25/#$27) | 05:D531 |
| $7F0000/$7F0800,X | words | $F998 dual clear; $F952 mirror-XOR #$4000 | 05:F998, 05:F952 |
| $0800 queue | bytes | $F91E seeds occ/$6000/$0000/$7F/#$0580; $F93D length pick | 05:F91E |
| $14D8/$14DA/$1414/$1416/$1418 | bytes/words | $8B7C flight event copy/seed before $8AB3 | 1D:8B7C |
| $1D8BDC,X → $1400,X | 32 bytes | $8B7C template blit | 1D:8B7C |
| $0035,Y/$0036,Y / $002B,Y | bytes | $DD51 wrap addend + clear script counter | 05:DD51 |
| $002E,$0024,Y / $002F,$0026,Y | frac/words | $DD69/$DD83/$DD9D/$DDB7 ±$14C9/$14CA move | 05:DD69 |
| $14C8/$14C9/$14CB / DP+$00/$02 | bytes/words | $DFF3 scale via $86FC into $14C9 word | 05:DFF3 |
| DP+$B0 bit1 | byte | $F548 ORA #$02 flag for ($E117,2) | 05:F548 |
| $0021,Y / DP+$04 | bytes | $D7F4 ORA #$80 face from operand | 05:D7F4 |
| $002C/$002D/$0030,Y / $148C | bytes | $D81E script seed + optional APU-ish $83 | 05:D81E |
| $0030,Y | byte | $D849 gate ($FF → wrap via $DD51) | 05:D849 |
| $0032,Y / DP+$04 / $14C9 | face/ops | $D743 scale-step + multiply probe | 05:D743 |
| $7F0000,X stream / $17DB/$17DD | tiles/words | $F410 ($E117,2) tile expand + temps | 05:F410 |
| $14CC / $14CD | bytes | ($CAFA,10) $D873/$D858 stream-index pick before [$00],Y | 05:D873 |
| $0027,Y | byte | ($CAFA,14) $D8B3 face-slot seed (#$01) | 05:D8B3 |
| $17D2 bit0 | byte | $D681/$D6F6 BCS → $D6D9/$D73A alternate scale | 05:D6F6 |
| $17E5 / #$0128 | word | $D5B9 vertical clamp gate before #$0070 store | 05:D5B9 |
| $1484 / $1486 | words | $D69C/$D711 mirror of $0024/$0026,Y after scale | 05:D69C |

| $1527 / DP+$B6 / $152C | word/bytes | $A581 type-alt seeds | 05:A581 |
| $152D,X / $0019,Y | bytes | $A90D mask + ORA #$80 face nudge via $B6C0 | 05:A90D |
| $14C3/$14C5 / DP+$10 | words/byte | $A93D face index → X | 05:A93D |
| $17E6 / $14FF | bytes | $D8DB toggle / $D9A6/$DA03 face flags | 05:D8DB |
| $0024/$0026/$002E/$002F,Y | words/bytes | $D955/$D970/$D90D/$D91F move seeds | 05:D955 |
| $1547 / $1546 | bytes | $F480 ($E117,4/6) phase counter / wrap | 05:F480 |
| DP+$83/$85 | words/byte | $E089 stream ptr from $05E0B1 then $96ED | 05:E089 |
| $7F0000,Y via [$04] | tiles | $F8B2 expand from [DP+$00] | 05:F8B2 |
| $17DB..$17E5 | words | $F85E clear pair selected by $1541 | 05:F85E |
| $1570,X | bytes | $1D89D1/$89DF timer table seed | 1D:89D1 |

| DP+$B7 | byte | $F6A8/$F6B9 Y-adjust gate (< #$12) and $F761 type compare | 05:F6A8 |
| $17DB/$17DC/$17DD | words | $F54F DMA/scroll temps; $F609 seeds #$0100/#$FFF8 | 05:F54F |
| $0800..$0810 queue | bytes | $F54F/#$82 path and $F685 DMA queue seeds | 05:F54F |
| $10:8CF0,X / [DP+$00],Y | stream | $F6B9 tile+attr expand with DP+$03 | 05:F6B9 |
| $10:8ADD,X | words | $F708 tile-list pointers until $0000 | 05:F708 |
| $05:F886,X | long | $F82B builds DP+$83/$85 then JSR $E0AB | 05:F82B |
| $7F1760,X | bytes | $F782 clear loop (16 bytes) when $1546!=#$82 after $F000 | 05:F782 |
| $0801/$0803/$0806 | words | $F000 queue seed from $1540/$1541 + PHY'd Y | 05:F000 |
| $121B/$121C/$121E / $15E8 / DP+$24 | bytes | $95C9 face-table write; $DA8D forces $15E8=#$02 | 05:95C9 |
| $01E7 / $17D2 / DP+$04 | bytes | $D48F ($CAFA,22) BMI/BIT/AND score gates before INY | 05:D48F |
| $14C1 / $14D3 / $14CC / $14E8 / DP+$B7 | bytes | $D4A4/$D4CB/$D503/$D4A9 compare siblings | 05:D48F |
| $0001,X / $0034,X | bytes | $D4D2 after $B6B2→TYX CMP paths | 05:D4D2 |
| $14CD / $121B / $15E8 / $1425/$1465 | bytes | $DA9C ($CAFA,38) face-class seed | 05:DA9C |
| $1558 / $155C / $155A / $155E / $1546 | bytes/words | $F06A ($E117,40/48) scroll prep | 05:F06A |
| $1558 / $155A / $155C | bytes/words | $F0EA ($E117,E8/EA) scroll prep sibling of $F06A | 05:F0EA |
| $7F0280..$7F037F | words | $F119 fill with #$0001 before queue helpers | 05:F119 |
| DP+$00 / $7F0000,X | long/tiles | $EA8C/$EAA4 stream expand until #$FFFF/#$FFFE | 05:EA8C |
| DP+$10/$12/$14/$15/$18/$1A | words | $F167 scratch — index, stream ptr, bank, scale temps | 05:F167 |
| DP+$B9 / Y | byte/index | $F353 slot gate (#$81→$1400 else $1440) | 05:F353 |
| $1548 / $4202/$4203/$4216 | byte/regs | $F364 multiply scale + wrap | 05:F364 |
| $17DB/$17DC/$17DD / $0800 queue | words/bytes | $F167 DMA/phase temps and dual queue entries | 05:F167 |

| $14C7 / $14D4 / $05F3A2,X | word/table | $F386 m16 A adjust | 05:F386 |

| DP+$00 | byte | $95C9 saves incoming A before $B6B2 | 05:95C9 |
| $0022,Y / DP+$04/$05 | bytes | $DA45 face nibble merge via $DA64/$DA6D | 05:DA45 |
| $0031,Y | byte | $DA7C stores DP+$04 operand | 05:DA7C |

| $15F1..$15F9 / $1400↔$1440 | bytes | $D274 face-slot swap snapshot + reseed | 05:D274 |
| DP+$05/$04 / $01EF / $1540 | bytes | $D306 tier from $DBF3 then $F85E/$E073→$DC48 | 05:D306 |
| $1552/$1553/$1554/$1556 | bytes/words | $DB2B ($CAFA,42) face scroll seed before JMP $CA8D | 05:DB2B |
| $1541 / $155E,$1559,$155F (+8) | bytes | $DBF3 face/slot seeds from DP+$05 bit4/bit7 | 05:DBF3 |
| DP+$04 / $1540 / $1546/$1548 | bytes | $DC48 ($CAFA,50) phase after $DDF4/$DDD1; unblocks $D306 | 05:DC48 |
| DP+$B0 / $1558/$1560/$1568 | bytes | $DC7C clear bit1 + scroll zeros | 05:DC7C |
| $1502,X | byte | $E073 stores $1540 using ($1541-1) clamped index | 05:E073 |
| $0000,Y bit1 / $0020,Y | bytes | $D3D6/$D3E9 set/clear via $B594/$B5AA before $1D8583/$859B | 05:D3D6 |
| DP+$00/$04 / $0035,Y | words | $D45F ($CAFA,74) m16 score adjust → JMP $CA8D | 05:D45F |
| DP+$B6 | byte | $DD29 ($CAFA,5A) face-class from $14C2 / DP+$04 | 05:DD29 |

| $0B00,X / $003F,Y / DP+$00/$02/$10..$16 | actors/temps | $D330 bit6 scan + scale fold before $C07B | 05:D330 |

| $05DE63,X / [DP+$00],Y | words/pairs | $DE26 deep $DDF4 pair-table; keys vs $B572 | 05:DE26 |
| $05DEB2,X | bytes | $DEA1 nibble remap (BMI→#$20) | 05:DEA1 |
| $1558/$155A/$155C/$1546 | bytes/words | $EED4/$EF5A/$EFCC/$ECE4 ($E117) phase seeds | 05:EED4 |
| $0D7A/$0D75/$0D01,Y/$0D02,Y/$0D03,Y | words/bytes | $1D8583/$859B face→queue before JML $01C987 | 1D:8583 |


| DP+$10/$11 / $1547..$154F / $1558..$155C | words/bytes | $EB36/$EC61 phase oscillator + seed | 05:EB36 |
| $05ECD4,X word pairs | table | $EC61 ± drift into $155A/$155C | 05:EC61 |
| DP+$02 / $1093ED / $1EC0C6 | bank/ptrs | $EA52/$EA6F stream prologues before $EAA4 | 05:EA52 |
| DP+$83/$85 / $17969C / $1093ED | stream | $EAD0/$EB08 queue expand via $96ED | 05:EB08 |
| $1540 EOR #$03 / $1546 | bytes | $EAEB toggle before JSR $EB08 | 05:EAEB |
| $4204..06 / $4216 / DP+$00 | div/rem | $ED46/$ED4A ÷3 remainder + seed → $1558 | 05:ED4A |
| $05EDFB,X / $05EE56,X | byte pairs | $EDB4/$EE01 class → $155A/$155C | 05:EDB4 |
| $1560..$1566 / $1558..$155C | bytes/words | $E921 dual scroll bank seed + drift | 05:E921 |

| $1558/$155A/$155C/$1546/$155E | bytes/words | $EE5E/$EE99 shared $EEBD drift + seeds | 05:EE5E |
| $1560/$1562/$1564 | bytes/words | $E97B/#$84-phase and $ED88/#$81-phase bank | 05:E97B |
| $1558/$155A/$155C/$155E / DP+$51 | bytes/words | $E9A2 face seed + controller gate | 05:E9A2 |
| DP+$00 / $1558/$155A/$155C | words | $E9D4/$EA0E shared $EA2D m16 drift threshold | 05:E9D4 |
| $1558/$155A/$155C/$155E | bytes/words | $E8DC/$E87F/$E845 phase seeds + drift | 05:E8DC |
