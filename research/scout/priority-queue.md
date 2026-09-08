# Priority queue — next reconstruction targets (scout)

Ranked for the reconstruct lane. Prefer: (a) scene setup `8E00+`, (b) HDMA/queue callers `8B28+`,
(c) `8954+`, (d) `9B36+` producers, (e) battle-route hot leaves.
Already-native ranges excluded. Sizes approximate.

| Rank | Target | ~Size | Leaf? | One-line rationale |
|---:|---|---:|:---:|---|
| 1 | `03:99A6` | 45 | Y | hot interpreted cluster 03:99A6–9A26 (40294 hits) (replay range hits≈40132) |
| 2 | `01:8401` | 45 | Y | hot interpreted cluster 01:8401–84CE (84605 hits) (replay range hits≈11309) |
| 3 | `04:8EF7` | 39 | Y | hot interpreted cluster 04:8EF7–8F61 (52409 hits) (replay range hits≈18) |
| 4 | `13:EB47` | 73 | ~ | hot interpreted cluster 13:EB47–EB7F (101024 hits) (replay range hits≈101024) |
| 5 | `04:8000` | 169 | Y | hot interpreted cluster 04:8000–80F1 (76821 hits) (replay range hits≈57912) |
| 6 | `00:933A` | 111 | n | scene-mode table $8B54[6] (JMP from native 8B51) (replay range hits≈39) |
| 7 | `00:9102` | 155 | n | scene-mode table $8B54[3] (JMP from native 8B51) |
| 8 | `00:91B6` | 7 | n | scene-mode table $8B54[4] (JMP from native 8B51) |
| 9 | `03:972D` | 16 | ~ | hot interpreted cluster 03:972D–973C (57155 hits) (replay range hits≈57155) |
| 10 | `00:9A3B` | 60 | ~ | hot interpreted cluster 00:9A3B–9A71 (53795 hits) (replay range hits≈53795) |
| 11 | `00:9735` | 1 | Y | hot interpreted cluster 00:9735–9747 (34988 hits) (replay range hits≈833) |
| 12 | `03:93F7` | 3 | Y | hot interpreted cluster 03:93F7–94D3 (155576 hits) (replay range hits≈6) |
| 13 | `03:9158` | 164 | n | hot interpreted cluster 03:9158–9192 (642808 hits) (replay range hits≈673448) |
| 14 | `01:C885` | 190 | n | hot interpreted cluster 01:C885–C8AA (232089 hits) (replay range hits≈282402) |
| 15 | `03:94DC` | 158 | n | hot interpreted cluster 03:94DC–9504 (51744 hits) (replay range hits≈295346) |
| 16 | `03:957A` | 16 | ~ | hot interpreted cluster 03:957A–9589 (35488 hits) (replay range hits≈35488) |
| 17 | `01:816F` | 100 | ~ | hot interpreted cluster 01:816F–81F4 (40896 hits) (replay range hits≈31363) |
| 18 | `03:91D4` | 40 | ~ | hot interpreted cluster 03:91D4–9254 (31218 hits) (replay range hits≈30640) |
| 19 | `00:AB00` | 44 | ~ | hot interpreted cluster 00:AB00–AD3E (190137 hits) (replay range hits≈6822) |
| 20 | `01:8250` | 52 | ~ | hot interpreted cluster 01:8250–82CF (97088 hits) (replay range hits≈1160) |

## Top 10 quick list

1. `03:99A6` — hot interpreted cluster 03:99A6–9A26 (40294 hits)
2. `01:8401` — hot interpreted cluster 01:8401–84CE (84605 hits)
3. `04:8EF7` — hot interpreted cluster 04:8EF7–8F61 (52409 hits)
4. `13:EB47` — hot interpreted cluster 13:EB47–EB7F (101024 hits)
5. `04:8000` — hot interpreted cluster 04:8000–80F1 (76821 hits)
6. `00:933A` — scene-mode table $8B54[6] (JMP from native 8B51)
7. `00:9102` — scene-mode table $8B54[3] (JMP from native 8B51)
8. `00:91B6` — scene-mode table $8B54[4] (JMP from native 8B51)
9. `03:972D` — hot interpreted cluster 03:972D–973C (57155 hits)
10. `00:9A3B` — hot interpreted cluster 00:9A3B–9A71 (53795 hits)

## Preference mapping

| Preference | Targets in top 20 |
|---|---|
| (a) scene setup 8E00+ | — |
| (b) HDMA/queue 8B28+ | — |
| (c) 8954+ palette/WRAM | — |
| (d) 9B36+ producers | — |
| (e) battle/hot call-chain leaves | `13:EB47`, `03:9158`, `01:C885`, `03:94DC` |

## Notes for reconstruct lane

- Best first picks: **`04:85B6`** (tiny leaf, many callers incl. native `8B3E`), then **`00:8E76`→`00:8D2B`** (scene table[1] abutting native `8E75`).
- Close native `8DFE` chain with **`00:90C1`** (pulls hot **`00:C559`/`00:C68C`**) and later **`06:F14D`.
- HDMA wipe still JSLs **`03:FB8D`**; scene table[0] **`00:8B7A`** is larger—tackle after small leaves.
- Producer frontier: **`00:9BBF`** after native `9BBE`.
- Regen: `python3 research/scout/analyze_scout.py` after new `artifacts/scout-*/executed-addresses.csv`.
