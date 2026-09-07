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
| DP+62 | byte | HDMA channel mask restored after uploads | 8324 |
| DP+A8 | word | Deferred OAM address, uploaded then cleared | 8324 |
| DP+BA bit 7 | bit | Skip initial HDMA disable when set | 8324 |
| 0200–03FF | 512 bytes | Palette shadow, 256 two-byte entries | 8324, 86B6 |
| 0400–061F | 544 bytes | OAM shadow: sprite records and high table | 8324, 86A0 |
| 070F, 0711, 0713 | words | Transfer bookkeeping reset after queue drain; producers not yet mapped | 8324 |
| 0715 | byte | Palette dirty: nonzero triggers upload, then cleared | 8324 |
| 0800 onward | eight-byte entries | VRAM DMA queue, stops at first inactive entry | 8324 |

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
Queue capacity and producer ownership remain to be recovered. Do not infer a
capacity from the test queue lengths or apply new bounds that change game logic.
