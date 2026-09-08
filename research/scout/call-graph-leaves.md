# Call-graph leaves / near-leaves — scout lane

Leaf ≈ no nested JSR/JSL before RTS/RTL on a linear scan. Near-leaf ≤ 2 callees.
Sizes are approximate (M/X=8 length table; REP/SEP can skew a few bytes).

## Callees of already-native regions (not yet native)

## Scene dispatch table `$00:8B54` (from native `8B51`)

Native HDMA/queue wipe ends in `JMP ($8B54,X)` with X = (DP+$24 & $7F) * 2.

| Index | Target | Approx size | Leaf? | Nested calls | Hot hits (range) |
|---:|---|---:|:---:|---|---:|
| 0 | `00:8B7A` | 433 | n | 03:A6CB, 00:0284, 00:8887, 06:E837 | 830 |
| 1 | `00:8E76` | 32 | n | 00:8D2B, 02:D812 | 0 |
| 2 | `00:8E96` | 282 | n | 03:EF29, 00:94B2, 00:0284, 00:8887 | 138 |
| 3 | `00:9102` | 155 | n | 00:8908, 00:8DAC, 00:919D, 01:C796 | 0 |
| 4 | `00:91B6` | 7 | n | — | 0 |
| 5 | `00:9251` | 75 | n | 00:929C | 0 |
| 6 | `00:933A` | 111 | n | 00:2622, 00:92FB, 03:EA1E | 39 |
| 7 | `00:93AC` | 66 | n | 03:B68F, 03:9255, 04:9FDC | 54 |
| 8 | `00:943F` | 5 | ~ | 04:D85B | 0 |
| 9 | `00:9444` | 21 | ~ | 04:8EBA | 0 |
| 10 | `00:9459` | 22 | ~ | 03:EF1A, 02:EFE1 | 0 |
| 11 | `00:952B` | 5 | ~ | 02:F462 | 0 |
| 12 | `00:9530` | 5 | ~ | 04:8674 | 0 |
| 13 | `00:9535` | 36 | n | 02:D9D9, 00:9569 | 0 |
| 14 | `00:9579` | 48 | n | 00:9485, 04:85A1, 00:8DAC, 00:8908 | 0 |
| 15 | `00:95A9` | 14 | ~ | 00:8E96 | 0 |

## Notable bank `$00` leaves / near-leaves near hot PCs & native frontiers

### `00:8E76` — CANDIDATE

- Range ≈ `00:8E76–00:8E95` (~32 bytes)
- Nested: [('JSL', '00:8D2B'), ('JSL', '02:D812')]; exits: [('JMP', 35706)]
- Callers (static sample): n/a
- Rationale: scene-mode table $8B54[1] (JMP from native 8B51); falls after native scene pointer build 8E26–8E75; also scene table[1]
- Hits entry/range: 0/0

### `00:9BBF` — NEAR-LEAF

- Range ≈ `00:9BBF–00:9C4A` (~140 bytes)
- Nested: [('JSL', '00:8711'), ('JSL', '00:8724')]; exits: [('RTL', 40010)]
- Callers (static sample): n/a
- Rationale: falls after native VRAM enqueue producers 9B36–9BBE
- Hits entry/range: 0/0

### `00:8B7A` — CANDIDATE

- Range ≈ `00:8B7A–00:8D2A` (~433 bytes)
- Nested: [('JSL', '03:A6CB'), ('JSR', '00:0284'), ('JSL', '00:8887'), ('JSL', '06:E837'), ('JSL', '00:8B15'), ('JSL', '03:9255'), ('JSL', '00:86B6'), ('JSR', '00:94E7'), ('JSR', '00:9485'), ('JSL', '01:B61E'), ('JSL', '01:B67A'), ('JSL', '01:B6AC'), ('JSL', '01:B6BF'), ('JSL', '00:8324'), ('JSL', '02:D37D'), ('JSL', '03:99B3'), ('JSL', '03:93FA'), ('JSL', '06:EC12'), ('JSL', '00:C559'), ('JSL', '00:C68C'), ('JSL', '00:90E6'), ('JSL', '00:8DAC'), ('JSL', '00:877E'), ('JSL', '03:91FC'), ('JSL', '03:F7A4'), ('JSL', '00:8DFA'), ('JSR', '00:00A9'), ('JSL', '01:C535'), ('JSL', '00:8D4E'), ('JSL', '03:D2F2'), ('JSL', '02:D844'), ('JSL', '06:EBC1'), ('JSL', '04:85B6'), ('JSL', '04:85B6')]; exits: [('RTL', 36138)]
- Callers (static sample): 02:F0B2/JSL
- Rationale: scene-mode table $8B54[0] (JMP from native 8B51); scene table[0]; first mode handler after HDMA wipe
- Hits entry/range: 2/830

### `00:8E96` — CANDIDATE

- Range ≈ `00:8E96–00:8FAF` (~282 bytes)
- Nested: [('JSL', '03:EF29'), ('JSR', '00:94B2'), ('JSR', '00:0284'), ('JSL', '00:8887'), ('JSL', '00:C559'), ('JSL', '00:C68C'), ('JSL', '00:90E6'), ('JSL', '00:90C1'), ('JSL', '06:EAD4'), ('JSL', '06:EB52'), ('JSL', '01:B61E'), ('JSL', '03:F06F'), ('JSL', '00:8FB2'), ('JSL', '01:C535'), ('JSL', '00:8AFC')]; exits: [('RTI', 36783)]
- Callers (static sample): 00:95AE/JSL
- Rationale: scene-mode table $8B54[2] (JMP from native 8B51); scene table[2]
- Hits entry/range: 1/138

### `00:89B8` — NEAR-LEAF

- Range ≈ `00:89B8–00:8A06` (~79 bytes)
- Nested: [('JSL', '00:8A28'), ('JSL', '00:8A69')]; exits: [('RTL', 35334)]
- Callers (static sample): 02:F405/JSL, 03:B22A/JSL
- Rationale: after WRAM cursor helper 89AC–89B7
- Hits entry/range: 2/80

### `00:849C` — LEAF

- Range ≈ `00:849C–00:84D5` (~58 bytes)
- Nested: none; exits: [('RTL', 34005)]
- Callers (static sample): 00:A879/JSL, 00:A8E6/JSL, 00:AC37/JSL, 00:AD28/JSL, 00:B167/JSL, 00:B196/JSL, 00:B230/JSL, 00:B5F1/JSL, 00:C1BB/JSL, 00:C254/JSL
- Rationale: after mosaic/window 8471–849B; hot interpreted cluster 00:849C–84DA (365252 hits)
- Hits entry/range: 6124/297305

### `00:8AE5` — NEAR-LEAF

- Range ≈ `00:8AE5–00:8AFB` (~23 bytes)
- Nested: [('JSL', '61:2423')]; exits: [('RTL', 35579)]
- Callers (static sample): n/a
- Rationale: actor-type table / fallthrough after 8AD6–8AE4
- Hits entry/range: 0/1421

### `00:85EF` — LEAF

- Range ≈ `00:85EF–00:8616` (~40 bytes)
- Nested: none; exits: [('RTL', 34326)]
- Callers (static sample): 13:EB90/JSL
- Rationale: hot interpreted cluster 00:85EF–865B (3550038 hits)
- Hits entry/range: 3608/668535

### `00:877E` — NEAR-LEAF

- Range ≈ `00:877E–00:87D8` (~91 bytes)
- Nested: [('JSL', '00:87E9'), ('JSL', '00:87F1')]; exits: [('RTL', 34776)]
- Callers (static sample): 00:8C88/JSL, 04:A6A1/JSL, 04:AA56/JSL
- Rationale: after VRAM CPU copy 8732–877D
- Hits entry/range: 2/44

### `00:9444` — NEAR-LEAF

- Range ≈ `00:9444–00:9458` (~21 bytes)
- Nested: [('JSL', '04:8EBA')]; exits: [('RTL', 37976)]
- Callers (static sample): n/a
- Rationale: scene-mode table $8B54[9] (JMP from native 8B51)
- Hits entry/range: 0/0

### `00:9459` — NEAR-LEAF

- Range ≈ `00:9459–00:946E` (~22 bytes)
- Nested: [('JSL', '03:EF1A'), ('JSL', '02:EFE1')]; exits: [('RTL', 37998)]
- Callers (static sample): n/a
- Rationale: scene-mode table $8B54[10] (JMP from native 8B51)
- Hits entry/range: 0/0

### `00:95A9` — NEAR-LEAF

- Range ≈ `00:95A9–00:95B6` (~14 bytes)
- Nested: [('JSL', '00:8E96')]; exits: [('RTL', 38326)]
- Callers (static sample): n/a
- Rationale: scene-mode table $8B54[15] (JMP from native 8B51)
- Hits entry/range: 0/0

### `00:C559` — LEAF

- Range ≈ `00:C559–00:C5EC` (~148 bytes)
- Nested: none; exits: [('RTL', 50668)]
- Callers (static sample): 00:8844/JSL, 00:8C72/JSL, 00:8F34/JSL, 00:90CF/JSL, 00:9FE2/JSL, 00:A1B5/JSL, 00:AE2D/JSL, 00:B2D9/JSL, 00:BBE1/JSL, 01:CAA2/JSL
- Rationale: hot interpreted cluster 00:C559–C7D9 (2658191 hits)
- Hits entry/range: 81/1941624

### `03:9752` — LEAF

- Range ≈ `03:9752–03:977B` (~42 bytes)
- Nested: none; exits: [('RTL', 38779)]
- Callers (static sample): 03:9606/JSL, 03:9695/JSL
- Rationale: hot interpreted cluster 03:9752–977B (85200 hits)
- Hits entry/range: 3550/85200

### `00:943F` — NEAR-LEAF

- Range ≈ `00:943F–00:9443` (~5 bytes)
- Nested: [('JSL', '04:D85B')]; exits: [('RTL', 37955)]
- Callers (static sample): n/a
- Rationale: scene-mode table $8B54[8] (JMP from native 8B51)
- Hits entry/range: 0/0

### `00:952B` — NEAR-LEAF

- Range ≈ `00:952B–00:952F` (~5 bytes)
- Nested: [('JSL', '02:F462')]; exits: [('RTL', 38191)]
- Callers (static sample): n/a
- Rationale: scene-mode table $8B54[11] (JMP from native 8B51)
- Hits entry/range: 0/0

### `00:9530` — NEAR-LEAF

- Range ≈ `00:9530–00:9534` (~5 bytes)
- Nested: [('JSL', '04:8674')]; exits: [('RTL', 38196)]
- Callers (static sample): n/a
- Rationale: scene-mode table $8B54[12] (JMP from native 8B51)
- Hits entry/range: 0/0

### `00:98F2` — NEAR-LEAF

- Range ≈ `00:98F2–00:990E` (~29 bytes)
- Nested: [('JSL', '01:C95A'), ('JSL', '01:C91F')]; exits: [('RTL', 39182)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 00:98F2–995E (154628 hits)
- Hits entry/range: 1623/145817

### `04:8550` — LEAF

- Range ≈ `04:8550–04:856C` (~29 bytes)
- Nested: none; exits: [('RTL', 34156)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 04:8550–869D (4336327 hits)
- Hits entry/range: 3858/26663

### `00:82C7` — NEAR-LEAF

- Range ≈ `00:82C7–00:82D3` (~13 bytes)
- Nested: [('JSL', '00:82A3')]; exits: [('RTL', 33491)]
- Callers (static sample): 01:F8FE/JSL, 03:8105/JSL
- Rationale: gap after RNG 82A3–82C6 before controller
- Hits entry/range: 1/6

### `01:8401` — LEAF

- Range ≈ `01:8401–01:842D` (~45 bytes)
- Nested: none; exits: [('RTL', 33837)]
- Callers (static sample): 00:976E/JSL, 00:9A19/JSL
- Rationale: hot interpreted cluster 01:8401–84CE (85438 hits)
- Hits entry/range: 833/11309

### `13:EB47` — NEAR-LEAF

- Range ≈ `13:EB47–13:EB8F` (~73 bytes)
- Nested: [('JSL', '00:85E9')]; exits: [('RTL', 60303)]
- Callers (static sample): 04:8F57/JSL, 04:A9E8/JSL, 04:AA03/JSL, 04:B4B6/JSL, 04:BA9E/JSL, 04:BAB9/JSL, 04:C4A3/JSL, 04:C4A9/JSL, 04:C4AF/JSL, 04:C8B3/JSL
- Rationale: hot interpreted cluster 13:EB47–EB7F (101024 hits)
- Hits entry/range: 3608/101024

### `04:8000` — LEAF

- Range ≈ `04:8000–04:80A8` (~169 bytes)
- Nested: none; exits: [('RTL', 32936)]
- Callers (static sample): 04:869E/JSL, 04:8A44/JSL, 04:A074/JSL, 04:A32D/JSL, 04:A70A/JSL, 04:A854/JSL, 04:A967/JSL, 04:AB7F/JSL, 04:B102/JSL, 04:B3E4/JSL
- Rationale: hot interpreted cluster 04:8000–80F1 (76821 hits)
- Hits entry/range: 1016/57912

### `04:889E` — NEAR-LEAF

- Range ≈ `04:889E–04:88FE` (~97 bytes)
- Nested: [('JSR', '04:7FA9'), ('JSR', '04:FFA9')]; exits: [('RTL', 35070)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 04:889E–88C5 (81930 hits)
- Hits entry/range: 1/81930

### `03:93F7` — LEAF

- Range ≈ `03:93F7–03:93F9` (~3 bytes)
- Nested: none; exits: [('RTL', 37881)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 03:93F7–94D3 (155576 hits)
- Hits entry/range: 2/6

## Good next-native heuristics

1. **`00:8E76` / `00:8B7A`** — immediate scene-mode handlers after already-native `8B28`/`8E26`.
2. **`00:90C1`** — direct JSL from native scene setup `8E14`; finishes the `8DFE` chain.
3. **`00:9BBF+`** — producer-side continuation after enqueue `9B36`/`9B79`.
4. **`04:85B6` / `03:FB8D`** — mandatory callees of native HDMA wipe; unblock full `8B28` ownership.
5. Prefer leaves ≤ ~80 bytes with known JSR/JSL from natives for differential tests.
