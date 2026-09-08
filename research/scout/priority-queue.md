# Priority queue — next reconstruction targets (scout)

Ranked for the reconstruct lane. Prefer: (a) scene setup `8E00+`, (b) HDMA/queue callers `8B28+`,
(c) `8954+`, (d) `9B36+` producers, (e) battle-route hot leaves.
Already-native ranges excluded. Sizes approximate.

| Rank | Target | ~Size | Leaf? | One-line rationale |
|---:|---|---:|:---:|---|
| 1 | `00:8AE5` | 23 | ~ | actor-type table / fallthrough after 8AD6–8AE4 (replay range hits≈1421) |
| 2 | `03:9752` | 42 | Y | hot interpreted cluster 03:9752–977B (85200 hits) (replay range hits≈85200) |
| 3 | `04:83B2` | 95 | Y | hot interpreted cluster 04:83B2–8410 (51327 hits) (replay range hits≈51327) |
| 4 | `00:9251` | 75 | n | scene-mode table $8B54[5] (JMP from native 8B51) |
| 5 | `04:8550` | 29 | Y | hot interpreted cluster 04:8550–869D (4336327 hits) (replay range hits≈26663) |
| 6 | `01:8401` | 45 | Y | hot interpreted cluster 01:8401–84CE (85438 hits) (replay range hits≈11309) |
| 7 | `04:8EF7` | 39 | Y | hot interpreted cluster 04:8EF7–8F61 (52409 hits) (replay range hits≈18) |
| 8 | `03:952E` | 76 | n | hot interpreted cluster 03:952E–9561 (243602 hits) (replay range hits≈243602) |
| 9 | `13:EB47` | 73 | ~ | hot interpreted cluster 13:EB47–EB7F (101024 hits) (replay range hits≈101024) |
| 10 | `04:8000` | 169 | Y | hot interpreted cluster 04:8000–80F1 (76821 hits) (replay range hits≈57912) |
| 11 | `04:889E` | 97 | ~ | hot interpreted cluster 04:889E–88C5 (81930 hits) (replay range hits≈81930) |
| 12 | `00:933A` | 111 | n | scene-mode table $8B54[6] (JMP from native 8B51) (replay range hits≈39) |
| 13 | `00:9102` | 155 | n | scene-mode table $8B54[3] (JMP from native 8B51) |
| 14 | `00:91B6` | 7 | n | scene-mode table $8B54[4] (JMP from native 8B51) |
| 15 | `03:972D` | 16 | ~ | hot interpreted cluster 03:972D–973C (57155 hits) (replay range hits≈57155) |
| 16 | `00:9A3B` | 60 | ~ | hot interpreted cluster 00:9A3B–9A71 (53795 hits) (replay range hits≈53795) |
| 17 | `03:93F7` | 3 | Y | hot interpreted cluster 03:93F7–94D3 (155576 hits) (replay range hits≈6) |
| 18 | `03:9158` | 164 | n | hot interpreted cluster 03:9158–9192 (642808 hits) (replay range hits≈673448) |
| 19 | `01:C885` | 190 | n | hot interpreted cluster 01:C885–C8AA (232089 hits) (replay range hits≈282402) |
| 20 | `03:94DC` | 158 | n | hot interpreted cluster 03:94DC–9504 (51744 hits) (replay range hits≈295346) |

## Top 10 quick list

1. `00:8AE5` — actor-type table / fallthrough after 8AD6–8AE4
2. `03:9752` — hot interpreted cluster 03:9752–977B (85200 hits)
3. `04:83B2` — hot interpreted cluster 04:83B2–8410 (51327 hits)
4. `00:9251` — scene-mode table $8B54[5] (JMP from native 8B51)
5. `04:8550` — hot interpreted cluster 04:8550–869D (4336327 hits)
6. `01:8401` — hot interpreted cluster 01:8401–84CE (85438 hits)
7. `04:8EF7` — hot interpreted cluster 04:8EF7–8F61 (52409 hits)
8. `03:952E` — hot interpreted cluster 03:952E–9561 (243602 hits)
9. `13:EB47` — hot interpreted cluster 13:EB47–EB7F (101024 hits)
10. `04:8000` — hot interpreted cluster 04:8000–80F1 (76821 hits)

## Preference mapping

| Preference | Targets in top 20 |
|---|---|
| (a) scene setup 8E00+ | — |
| (b) HDMA/queue 8B28+ | — |
| (c) 8954+ palette/WRAM | `00:8AE5` |
| (d) 9B36+ producers | — |
| (e) battle/hot call-chain leaves | `03:952E`, `13:EB47`, `03:9158`, `01:C885`, `03:94DC` |

## Notes for reconstruct lane

- Best first picks: **`04:85B6`** (tiny leaf, many callers incl. native `8B3E`), then **`00:8E76`→`00:8D2B`** (scene table[1] abutting native `8E75`).
- Close native `8DFE` chain with **`00:90C1`** (pulls hot **`00:C559`/`00:C68C`**) and later **`06:F14D`.
- HDMA wipe still JSLs **`03:FB8D`**; scene table[0] **`00:8B7A`** is larger—tackle after small leaves.
- Producer frontier: **`00:9BBF`** after native `9BBE`.
- Regen: `python3 research/scout/analyze_scout.py` after new `artifacts/scout-*/executed-addresses.csv`.
