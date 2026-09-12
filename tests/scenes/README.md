# Scene regression packs

Declarative replay scenes for systematic host-level regression. The runner is
`tools/run_scenes.py`; design notes live in [`docs/scene-regression.md`](../../docs/scene-regression.md).

## Layout

| Path | Role |
|------|------|
| `manifest.json` | Scene pack definitions (id, mode, frames, inputs, chain deps) |
| `goldens/en/` | OCR-free EN smoke goldens — **hashes / PC signatures only** (no BMP/ROM) |
| `../start-game.inputs` etc. | Shared replay scripts (not duplicated here) |

## Modes

| Mode | Host flags | Pass criteria |
|------|------------|---------------|
| `verify` | `--lang ja --verify` | `report.json` → `equivalence_passed == true` |
| `smoke_en` | `--lang en` (no `--verify`) | Final `video_hash` + signatures match golden; `contrast_ja` asserts EN≠JA after crawl |
| `chain` | `--load-checkpoint <prior>/final.dbzstate` + relative inputs | Same as `verify` (JA) after loading dependency |

## Quick start

```sh
export ROM="${DBZ_TEST_ROM:-${DBZ_ROM:-Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc}}"
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM"
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --list
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --only ja_opening,en_intro_smoke
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --update-golden en_intro_smoke
python3 tools/run_scenes.py --binary build/dbz-port --rom "$ROM" --extended   # include stubs
```

Default CTest (`scene_regression`) runs non-`extended` scenes when a test ROM is configured.
