# Priority queue — next reconstruction targets (scout)

Ranked for the reconstruct lane. Prefer: (a) scene setup `8E00+`, (b) HDMA/queue callers `8B28+`,
(c) `8954+`, (d) `9B36+` producers, (e) battle-route hot leaves.
Already-native ranges excluded. Sizes approximate.

| Rank | Target | ~Size | Leaf? | One-line rationale |
|---:|---|---:|:---:|---|
| 1 | `04:85B6` | 25 | Y | JSL from 00:8B3E (HDMA/queue wipe) (replay range hits≈1633) |
| 2 | `00:8D2B` | 35 | Y | JSL from scene-mode 8E76 (abuts native scene build) |
| 3 | `00:8E76` | 32 | n | scene-mode table $8B54[1] (JMP from native 8B51) |
| 4 | `00:90C1` | 65 | n | JSL from 00:8E14 (scene setup 8DFE chain) (replay range hits≈338) |
| 5 | `03:FB8D` | 408 | Y | JSL from 00:8B42 (HDMA/queue wipe / scene entry) (replay range hits≈44) |
| 6 | `00:9BBF` | 140 | ~ | falls after native VRAM enqueue producers 9B36–9BBE |
| 7 | `00:8B7A` | 433 | n | scene-mode table $8B54[0] (JMP from native 8B51) (replay range hits≈830) |
| 8 | `00:C68C` | 123 | ~ | JSL from 00:90C1 (pair with C559) (replay range hits≈558352) |
| 9 | `00:C559` | 148 | Y | JSL from 00:90C1 (replay range hits≈1941624) |
| 10 | `00:8E96` | 282 | n | scene-mode table $8B54[2] (JMP from native 8B51) (replay range hits≈138) |
| 11 | `06:F14D` | 159 | ~ | JSL from 00:8E21 (scene setup writes $0700=$20 then F14D) (replay range hits≈1477) |
| 12 | `00:8FE5` | 285 | n | hot interpreted cluster 00:8FE5–9043 (456243 hits) (replay range hits≈456595) |
| 13 | `00:89B8` | 79 | ~ | after WRAM cursor helper 89AC–89B7 (replay range hits≈80) |
| 14 | `00:849C` | 58 | Y | after mosaic/window 8471–849B (replay range hits≈297305) |
| 15 | `00:8AE5` | 23 | ~ | actor-type table / fallthrough after 8AD6–8AE4 (replay range hits≈1421) |
| 16 | `00:85EF` | 40 | Y | hot interpreted cluster 00:85EF–865B (3550038 hits) (replay range hits≈668535) |
| 17 | `00:877E` | 91 | ~ | after VRAM CPU copy 8732–877D (replay range hits≈44) |
| 18 | `00:9444` | 21 | ~ | scene-mode table $8B54[9] (JMP from native 8B51) |
| 19 | `00:9459` | 22 | ~ | scene-mode table $8B54[10] (JMP from native 8B51) |
| 20 | `00:95A9` | 14 | ~ | scene-mode table $8B54[15] (JMP from native 8B51) |

## Top 10 quick list

1. `04:85B6` — JSL from 00:8B3E (HDMA/queue wipe)
2. `00:8D2B` — JSL from scene-mode 8E76 (abuts native scene build)
3. `00:8E76` — scene-mode table $8B54[1] (JMP from native 8B51)
4. `00:90C1` — JSL from 00:8E14 (scene setup 8DFE chain)
5. `03:FB8D` — JSL from 00:8B42 (HDMA/queue wipe / scene entry)
6. `00:9BBF` — falls after native VRAM enqueue producers 9B36–9BBE
7. `00:8B7A` — scene-mode table $8B54[0] (JMP from native 8B51)
8. `00:C68C` — JSL from 00:90C1 (pair with C559)
9. `00:C559` — JSL from 00:90C1
10. `00:8E96` — scene-mode table $8B54[2] (JMP from native 8B51)

## Preference mapping

| Preference | Targets in top 20 |
|---|---|
| (a) scene setup 8E00+ | `00:8E76`, `00:90C1`, `00:8E96`, `00:8FE5` |
| (b) HDMA/queue 8B28+ | `04:85B6`, `00:8D2B`, `03:FB8D`, `00:8B7A` |
| (c) 8954+ palette/WRAM | `00:89B8`, `00:8AE5` |
| (d) 9B36+ producers | `00:9BBF` |
| (e) battle/hot call-chain leaves | `00:C68C`, `00:C559`, `00:8FE5`, `00:849C`, `00:85EF` |

## Notes for reconstruct lane

- Best first picks: **`04:85B6`** (tiny leaf, many callers incl. native `8B3E`), then **`00:8E76`→`00:8D2B`** (scene table[1] abutting native `8E75`).
- Close native `8DFE` chain with **`00:90C1`** (pulls hot **`00:C559`/`00:C68C`**) and later **`06:F14D`.
- HDMA wipe still JSLs **`03:FB8D`**; scene table[0] **`00:8B7A`** is larger—tackle after small leaves.
- Producer frontier: **`00:9BBF`** after native `9BBE`.
- Regen: `python3 research/scout/analyze_scout.py` after new `artifacts/scout-*/executed-addresses.csv`.
