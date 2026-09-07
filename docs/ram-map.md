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
