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

### `03:99A6` — LEAF

- Range ≈ `03:99A6–03:99D2` (~45 bytes)
- Nested: none; exits: [('RTL', 39378)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 03:99A6–9A26 (40294 hits)
- Hits entry/range: 4704/40132

### `01:8401` — LEAF

- Range ≈ `01:8401–01:842D` (~45 bytes)
- Nested: none; exits: [('RTL', 33837)]
- Callers (static sample): 00:976E/JSL, 00:9A19/JSL
- Rationale: hot interpreted cluster 01:8401–84CE (84605 hits)
- Hits entry/range: 833/11309

### `04:8EF7` — LEAF

- Range ≈ `04:8EF7–04:8F1D` (~39 bytes)
- Nested: none; exits: [('RTS', 36637)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 04:8EF7–8F61 (52409 hits)
- Hits entry/range: 1/18

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

### `03:972D` — NEAR-LEAF

- Range ≈ `03:972D–03:973C` (~16 bytes)
- Nested: [('JSL', '00:8B0E')]; exits: [('RTL', 38716)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 03:972D–973C (57155 hits)
- Hits entry/range: 11005/57155

### `00:9A3B` — NEAR-LEAF

- Range ≈ `00:9A3B–00:9A76` (~60 bytes)
- Nested: [('JSL', '04:A074')]; exits: [('RTL', 39542)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 00:9A3B–9A71 (53795 hits)
- Hits entry/range: 1015/53795

### `00:9735` — LEAF

- Range ≈ `00:9735–00:9735` (~1 bytes)
- Nested: none; exits: [('RTL', 38709)]
- Callers (static sample): 00:976A/JSL, 00:9AE3/JSL, 00:9AEC/JSL, 00:9AFF/JSL
- Rationale: hot interpreted cluster 00:9735–9747 (34988 hits)
- Hits entry/range: 833/833

### `03:93F7` — LEAF

- Range ≈ `03:93F7–03:93F9` (~3 bytes)
- Nested: none; exits: [('RTL', 37881)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 03:93F7–94D3 (155576 hits)
- Hits entry/range: 2/6

### `03:957A` — NEAR-LEAF

- Range ≈ `03:957A–03:9589` (~16 bytes)
- Nested: [('JSL', '00:8B0E')]; exits: [('RTL', 38281)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 03:957A–9589 (35488 hits)
- Hits entry/range: 5696/35488

### `01:816F` — NEAR-LEAF

- Range ≈ `01:816F–01:81D2` (~100 bytes)
- Nested: [('JSL', '01:8A0E'), ('JSL', '01:8292')]; exits: [('RTL', 33234)]
- Callers (static sample): 01:8124/JSL
- Rationale: hot interpreted cluster 01:816F–81F4 (40896 hits)
- Hits entry/range: 973/31363

### `03:91D4` — NEAR-LEAF

- Range ≈ `03:91D4–03:91FB` (~40 bytes)
- Nested: [('JSR', '03:0364'), ('JSL', '00:85F5')]; exits: [('RTL', 37371)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 03:91D4–9254 (31218 hits)
- Hits entry/range: 1532/30640

### `00:AB00` — NEAR-LEAF

- Range ≈ `00:AB00–00:AB2B` (~44 bytes)
- Nested: [('JSR', '00:82FB')]; exits: [('RTS', 43819)]
- Callers (static sample): 00:A99B/JSR, 00:B420/JSR, 00:B6D8/JSR
- Rationale: hot interpreted cluster 00:AB00–AD3E (190137 hits)
- Hits entry/range: 379/6822

### `01:8250` — NEAR-LEAF

- Range ≈ `01:8250–01:8283` (~52 bytes)
- Nested: [('JSL', '01:8292'), ('JSL', '01:8284')]; exits: [('RTL', 33411)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 01:8250–82CF (97088 hits)
- Hits entry/range: 58/1160

### `01:802A` — NEAR-LEAF

- Range ≈ `01:802A–01:8091` (~104 bytes)
- Nested: [('JSL', '01:8292'), ('JSL', '01:8292')]; exits: [('RTL', 32913)]
- Callers (static sample): n/a
- Rationale: hot interpreted cluster 01:802A–8067 (31704 hits)
- Hits entry/range: 1321/33025

## Good next-native heuristics

1. **`00:8E76` / `00:8B7A`** — immediate scene-mode handlers after already-native `8B28`/`8E26`.
2. **`00:90C1`** — direct JSL from native scene setup `8E14`; finishes the `8DFE` chain.
3. **`00:9BBF+`** — producer-side continuation after enqueue `9B36`/`9B79`.
4. **`04:85B6` / `03:FB8D`** — mandatory callees of native HDMA wipe; unblock full `8B28` ownership.
5. Prefer leaves ≤ ~80 bytes with known JSR/JSL from natives for differential tests.
