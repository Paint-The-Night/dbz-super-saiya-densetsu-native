# Native port prototype

This is the first executable milestone, not a completed decompilation.
The main host and reconstructed game routines are C11. SDL2 handles the desktop
window, keyboard, and sound output. A pinned LakeSnes core supplies SNES hardware
and executes game code that has not yet been reconstructed.

Reconstructed code currently covers:

- Reset entry prefix at `$00:8000–800D` (the rest of reset is still interpreted).
- Display forced-blank control at `$00:828F–82A2`.
- Two random-number streams at `$00:82A3–82C6`:
  `seed = (5 * seed + 17) modulo 65536`, using separate direct-page variables
  at `$2F` and `$31`.
- Both controller ports at `$00:82D4–82FA`: wait for automatic polling,
  cache held buttons, and calculate newly pressed buttons as `held & ~previous`.
- Background scroll upload at `$00:82FB–8323`: send four cached scroll words
  to the BG1/BG2 horizontal and vertical hardware latches.
- Frame graphics upload at `$00:8324–83CB`: OAM, dirty palettes, and queued VRAM
  transfers, followed by HDMA and OAM-address restoration.
- Unused sprite hiding at `$00:86A0–86B5` and palette-shadow clearing at
  `$00:86B6–86C7`. See `ram-map.md` for the recovered buffers and queue layout.

These replacements execute compiled C at fixed game addresses. They retain
individual instruction bus cycles, flags, and interrupt boundaries. They do
not call the CPU interpreter to perform their own operations. The remaining
game code does. Only the pinned Japanese Rev 1 ROM is accepted; a copier header
is normalized before the SHA-256 check.

**Run and controls**

Open the Mac app and drop your original ROM onto its window, or launch the
executable with `--rom /absolute/path/to/game.sfc`. Successful windowed launches
remember absolute ROM paths for the next launch. The app bundles SDL and contains
no game ROM or extracted assets.

Arrow keys move. Z = B, X = A, A = Y, S = X, Enter = Start, right Shift = Select,
D = L, C = R. P pauses, Tab accelerates, Escape exits. Controller hardware is not
implemented yet. The original game is Japanese; no translation patch is applied.

Battery saves use the app's SDL preferences directory, normally
`~/Library/Application Support/GaryPerrigo/DBZNativePort/dbz-rev1.srm` on macOS.
An explicit `--save-dir` overrides it. Headless tests have no save-directory
default, so tests start clean and do not alter personal saves.

**Build and verification**

Install CMake, Ninja, a C11 compiler, and SDL2 development files. The hardware core
is vendored with its MIT license and a pinned provenance record. Build with:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

From the project directory, verify a deterministic opening/new-game sequence:

```sh
mkdir -p artifacts/start-game
build/dbz-port --rom 'Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc' \
  --headless --verify --frames 3000 --inputs tests/start-game.inputs \
  --dump-dir artifacts/start-game
```

This runs two separate machine states: one with the C replacements enabled,
one with the original interpreter. It compares serialized machine state,
framebuffer bytes, and stereo PCM after every frame, exits nonzero on divergence,
and saves both states for diagnosis. `--no-native` disables all replacements.
Frame reports record the input mask, hashes, CPU address, and native instruction
count; `executed-addresses.csv` supplies an observed-code worklist.
With `--inputs`, the replay exclusively controls game buttons; live keyboard
buttons cannot change its results. Pause, speed, and exit controls remain active.

Successful runs with `--dump-dir` also write `final.dbzstate`. Resume with
`--load-checkpoint path/to/final.dbzstate`; frame numbers and replay inputs begin
at 1 relative to that checkpoint. Both verification instances load the same
state. The versioned checkpoint includes the ROM identity, a SHA-256 checksum,
machine state, and exact audio sample phase. The core state also preserves the
DSP output history, which is needed for identical resumed PCM. Raw `final.state`
is a diagnostic artifact and is not accepted by this option. Older raw states
predating the DSP state fix are incompatible; regenerate checkpoints by replaying.

The `checkpoint_equivalence` CTest compares a 360-frame run with a 211+149-frame
split, checks every resumed state/video/audio hash, and rejects seven malformed
checkpoint variants. Python 3 is required for this integration test.

For the longer takeoff/flight/event route, use `--frames 5700 --inputs
tests/flight-event.inputs` with a fresh dump directory and no checkpoint.
This returns to the flying overworld menu after an event dialogue; it does not
yet provide a battle test. To branch from the opening's frame-3000 checkpoint,
use `--load-checkpoint .../final.dbzstate --frames 600 --inputs
tests/kame-flight.inputs`. These replay files target the exact starting states
described in their comments.

Isolated tests execute the original ROM routines against all 65,536 seeds in
each stream, comparing registers/flags and the ordered read/write/idle bus
sequence. Display tests cover every cached control byte at aligned and unaligned
direct pages. No game byte array is embedded in the tests.
Controller tests cover all 65,536 held masks with four previous-state patterns,
both ports, aligned/unaligned direct pages, and busy-wait execution. Another
512 scroll cases check all uploaded bytes and ordered bus transactions.

The baseline and hybrid share the same PPU/APU hardware model. These comparisons
validate the translated CPU behavior relative to that model. They do not prove
the emulator's hardware fidelity; a separate bsnes/MesenCE comparison remains
future work. Executed native instruction share measures this replay's workload,
not the percentage of the game's source recovered.

For sanitizers, configure another directory with `-DDBZ_SANITIZE=ON` and run the
same tests/replay with `UBSAN_OPTIONS=halt_on_error=1`. LeakSanitizer is unavailable
on this Apple Silicon host. Two upstream undefined shifts encountered during ROM
header probing and BRR audio decoding have local fixes documented in
`third_party/lakesnes/PROVENANCE.md`.

On macOS, `python3 tools/package_mac.py` produces `dist/DBZ Native Port.app`
with an ad-hoc signature and a bundled SDL dylib. This local developer build is
not notarized. Other platforms build the same executable through CMake; they
have not been run or packaged yet.
The package records the minimum macOS required by its executable and SDL library;
the current local Homebrew build requires macOS 26.0 and is Apple Silicon only.

**Next work**

Recover the remaining interrupt/update and initialization routines using the
executed-address worklist, add battle and save/reload replay coverage, and move
larger verified subsystems into named C functions. The previous SNESRecomp
experiment stays isolated under `.local/`; its automatically generated routines
are not part of this executable. The present runtime uses the permissively
licensed reference core and explicitly tested replacements.
