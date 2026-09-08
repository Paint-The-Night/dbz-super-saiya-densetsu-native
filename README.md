# DBZ Super Saiya Densetsu — Native Port Research

An evidence-driven reconstruction of **Dragon Ball Z: Super Saiya Densetsu**
(Japanese Rev 1) for portable C, starting with a native macOS build.

> This repository does **not** contain the commercial game ROM, extracted game
> assets, save files, or generated ROM data. You must supply a legally obtained,
> unmodified copy locally. The project accepts only the verified Japanese Rev 1
> ROM and checks its SHA-256 before use.

## What exists today

The project is a working hybrid prototype. A pinned SNES hardware core interprets
the unrecovered game code while verified C replacements run at fixed original
addresses. The replacements preserve CPU flags, bus accesses, timing, and
interrupt sampling, so they can run safely beside the interpreter.

The current reconstruction covers:

- reset setup and forced-blank control;
- both random-number streams;
- automatic controller polling and newly pressed-button detection;
- background scroll writes;
- sprite, palette, and queued VRAM uploads;
- unused-sprite hiding and palette-shadow clearing.

The native host provides an SDL2 window, keyboard input, audio, battery saves,
deterministic input replays, frame captures, checkpoints, and per-frame
differential verification against a second interpreter instance.

Verified routes reach the Kame House overworld, takeoff, flight, encounter
dialogue, and battle commands and animations. Reconstruction now also includes
scene and actor updates and substantial battle routines. Full-game progression
and elimination of the remaining interpreter fallback are still unfinished.

### Progress tracker

**95.51% — 12,643 / 13,237 discovered instruction sites reconstructed and verified**

This percentage uses the pinned static-analysis worklist as its denominator.
That worklist is incomplete, so it is a useful progress gauge for this project,
not a claim about whole-game completion. Recalculate it
with `python3 tools/progress.py`; use `--json` for automation.
Run `python3 tools/progress.py --check-readme` to verify this block, or
`python3 tools/progress.py --update-readme` to synchronize it. These progress
checks are ROM-free and run as part of the test suite.

See [the progress record](docs/progress.md) for measured coverage and evidence,
[the prototype guide](docs/prototype.md) for commands and controls, and [the
RAM map](docs/ram-map.md) for recovered buffers and graphics queues.

## Build

Requirements:

- CMake 3.20 or newer;
- Ninja or another CMake generator;
- a C11 compiler;
- SDL2 development files;
- Python 3 for the checkpoint integration test.

On macOS with Homebrew:

```sh
brew install cmake ninja sdl2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

To enable the ROM-dependent differential tests, configure with your local ROM:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDBZ_TEST_ROM="$DBZ_ROM"
```

Without a local test ROM, the portable build still succeeds and CTest runs the
ROM-free progress checks. The differential tests run when a test ROM is available.

The test suite compares the translated routines with the actual original ROM
instructions across exhaustive and randomized cases. It also verifies that a
run split across a checkpoint produces the same machine state, video, and PCM
audio as an uninterrupted run.

## Run it

Set `DBZ_ROM` to the path of your local ROM, then run a deterministic replay:

```sh
export DBZ_ROM="/path/to/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc"
mkdir -p artifacts/replay
build/dbz-port --rom "$DBZ_ROM" \
  --headless --verify --frames 5700 \
  --inputs tests/flight-event.inputs \
  --dump-dir artifacts/replay
```

To capture caller context for an unresolved routine, set `DBZ_ENTRY_TRACE`
to its address (for example `03:99A6`). The native runner writes matching
pre-instruction CPU register snapshots to `entry-snapshots.csv`; this local
file is ignored by Git and never contains the ROM.

For an interactive window, omit `--headless --frames`:

```sh
build/dbz-port --rom "$DBZ_ROM"
```

Keyboard controls are arrows, `Z`/`X` for B/A, `A`/`S` for Y/X, Enter for
Start, Right Shift for Select, and `D`/`C` for L/R. `P` pauses, Tab enables
turbo, and Escape exits. The game text is Japanese; no translation patch is
applied.

The host remembers the last absolute ROM path for windowed launches. Battery
saves go to the SDL preferences directory on macOS unless `--save-dir` is
specified. Headless runs do not touch personal saves unless explicitly given a
save directory.

## Checkpoints

Every successful dump writes `final.dbzstate`. Resume it with:

```sh
build/dbz-port --rom "$DBZ_ROM" \
  --headless --verify --frames 600 \
  --load-checkpoint artifacts/replay/final.dbzstate \
  --dump-dir artifacts/continued
```

The checkpoint is bound to the ROM hash and includes a checksum, machine state,
audio phase, and DSP output history. Raw diagnostic `final.state` files are not
accepted as checkpoints.

## Native macOS bundle

On macOS, the packaging script creates `dist/DBZ Native Port.app`, bundles SDL2,
includes the relevant license notices, and verifies the ad-hoc signature:

```sh
python3 tools/package_mac.py
open "dist/DBZ Native Port.app"
```

The bundle is a local developer build, not notarized. Its minimum macOS version
is derived from the executable and the SDL library used for that build. The
current local Apple Silicon package reports macOS 26.0; build SDL yourself for
an older deployment target.

## Project shape

`src/native.c` contains the game-specific replacements and `src/native_video.inc`
contains the graphics-transfer routines. `src/main.c` is the portable SDL host.
`third_party/lakesnes/` contains the pinned MIT-licensed hardware core and local
sanitizer fixes, with provenance recorded alongside it. `research/` documents
the ROM, recompiler experiments, and reverse-engineering decisions. The earlier
SNESRecomp output remains a feasibility experiment and is not linked into the
active executable.

The repository intentionally keeps original-address semantics visible rather
than hiding the work behind a large generated code dump. Each new replacement
must pass differential tests before it is included in a replay.

## Roadmap

1. Capture and verify the first real battle route.
2. Recover the remaining NMI, initialization, menu, map, and battle routines.
3. Extend replay coverage through progression, saving, endings, and every major
   gameplay subsystem.
4. Compare the host against an independent emulator for hardware fidelity.
5. Validate builds on additional platforms and toolchains.

This is an ongoing reverse-engineering project, not a completed decompilation.
The current executable demonstrates the workflow and the correctness gates;
it does not yet replace the full original game logic.

## Legal and attribution

The game ROM and its artwork, music, text, and other copyrighted material remain
the property of their respective rights holders. This repository contains only
original research, reconstruction code, test tooling, and third-party code whose
licenses are included with that code. Do not submit ROMs, extracted assets, or
save files in issues or pull requests.

The hardware core is [LakeSnes](https://github.com/elzo-d/LakeSnes), pinned and
credited in [`third_party/lakesnes/PROVENANCE.md`](third_party/lakesnes/PROVENANCE.md).
Its MIT license is included in the repository. The recompiler experiment is
kept separate from the executable and retains its upstream licensing terms.
