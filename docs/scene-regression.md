# Scene regression suite

Declarative multi-scene packs so host changes can systematically exercise many
game parts without one-off shell recipes.

## Why

Unit coverage in `tests/test_native.c` proves native leaves against the reference
interpreter. Integration still needs longer, scripted journeys (opening, flight,
EN boot smoke, battle). Scene packs store those journeys as data:

- which inputs / frame counts to run;
- whether to differentially `--verify`;
- whether to chain from a prior `final.dbzstate`;
- OCR-free EN goldens (video hashes / PC signatures only).

## Layout

```text
tests/scenes/manifest.json     # scene pack definitions
tests/scenes/README.md         # quick reference
tests/scenes/goldens/en/       # EN smoke goldens (hashes only; no BMP/ROM)
tools/run_scenes.py            # runner
docs/scene-regression.md       # this file
```

Replay scripts stay in `tests/*.inputs` and `research/scout/*.inputs`.

## Modes

| Mode | Host CLI | Assertion |
|------|----------|-----------|
| **verify** (JA) | `--lang ja --verify --frames N --inputs …` | `report.json` → `equivalence_passed == true` |
| **smoke_en** | `--lang en --frames N` (**no** `--verify`) | Final `video_hash` + signature rows vs golden JSON; optional `contrast_ja` also runs JA and asserts EN≠JA after `diverge_after` |
| **chain** | `--load-checkpoint <dep>/final.dbzstate` + relative inputs | Same as verify after the dependency checkpoint |

Chain dependencies are declared with `load_from` in the manifest. The runner
builds missing parents in the same work directory before the child scene.

## MVP scenes

| Id | Mode | Frames | Notes |
|----|------|--------|-------|
| `ja_opening` | verify | 3000 | `tests/start-game.inputs` |
| `ja_kame_flight` | chain ← opening | 600 | `tests/kame-flight.inputs` |
| `en_intro_smoke` | smoke_en | 2400 | No inputs; EN≠JA contrast after ~2093; golden under `goldens/en/` |
| `ja_flight_event` | verify (extended) | 5700 | Stub; `--extended` or `--only` |
| `ja_battle_route` | chain ← opening (extended) | 1800 | Stub; `research/scout/battle-route.inputs` |

Extended stubs are **skipped by default** (same idea as `-DDBZ_EXTENDED_TESTS`
for `battle_equivalence`).

## Runner

```sh
export ROM="${DBZ_TEST_ROM:-${DBZ_ROM:-Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc}}"

python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM"
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --list
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --only ja_opening,en_intro_smoke
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --update-golden en_intro_smoke
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --extended
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --work-dir artifacts/scenes
```

Host binary flags (survey):

```text
dbz-port --rom … [--headless --frames N] [--verify] [--lang en|ja]
         [--inputs …] [--dump-dir DIR] [--load-checkpoint …]
```

## CTest

When `DBZ_TEST_ROM` / `DBZ_ROM` / the local `Backup/…Rev 1.sfc` is available,
CMake registers `scene_regression` (non-extended MVP scenes), parallel to
`checkpoint_equivalence`. Rebuild/reconfigure after pulling so CTest picks it up.

## Goldens

- Commit **hashes and PC signatures only** — never ROM bytes or BMP captures.
- Regenerate after intentional EN visual changes:

```sh
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" \
  --only en_intro_smoke --update-golden en_intro_smoke
```

## Related docs

- [`prototype.md`](prototype.md) — host CLI, checkpoints, input replays
- [`battle-route.md`](battle-route.md) — extended battle route
- [`i18n.md`](i18n.md) / [`text-api.md`](text-api.md) — EN path (smoke scenes)
