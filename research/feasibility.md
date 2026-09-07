# Feasibility assessment — 7 September 2026

**Recommendation: proceed with portable C, using an emulator-backed reverse-engineering workflow and a bounded evaluation of SNESRecomp.** A useful compilation probe already succeeds on this Mac. That establishes a practical starting point, not a completed decompilation or a playable port.

Gary's intended outcome is the original game reconstructed in a low-level language, initially on Mac and portable elsewhere. A browser remake or an emulator packaged with the ROM would not satisfy that outcome.

**What the current approaches actually deliver.** A disassembly reconstructs assembly instructions and data layout; an assembler can verify that understood regions reproduce the original bytes. A decompilation/reimplementation reconstructs higher-level logic. Static recompilation mechanically translates machine instructions to host code, often retaining explicit emulated registers and memory operations. The last route can produce native CPU code sooner, but leaves considerable work to recover readable game systems.

For this game, we have not established the original implementation language. We should reconstruct semantics from the binary rather than assume there is recoverable original C or an original compiler we can match. The 65816's switchable register widths, banked addressing, indirect calls, interrupts, and RAM-resident code make static analysis harder than a linear opcode dump. Register-width state can even change instruction lengths. [WDC processor reference](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf), [DiztinGUIsh](https://github.com/DizTools/DiztinGUIsh).

| Approach | Evidence and maturity | Use here |
| --- | --- | --- |
| Annotated disassembly and byte comparison | DiztinGUIsh tracks CPU context, imports execution information, and exports Asar-compatible assembly. Its main interface is Windows-oriented. | Maintain trustworthy addresses, labels, code/data boundaries, and selected byte-exact assembly regions. |
| Verified C reconstruction | snesrev's Zelda 3 is playable from beginning to end, uses existing PPU/DSP implementations, and can compare its RAM against the original game each frame. Its author credits pre-existing disassembly research. | Best demonstrated architectural model for an understandable native port. |
| Automatic C recompilation | mstan/SNESRecomp is alpha; it includes native compilation plus an interpreter fallback and requires per-game integration. The local probe below succeeds. | Evaluate as an accelerator and initial execution substrate. |
| AI-controlled debugger | Mesen2-Diz exposes debugger operations through JSON IPC, including memory, execution, labels, and tracing. Its repository explicitly says there is no current Mac release and a MesenCE migration is underway. | Promising automation design, but Mac integration needs proving. |
| Ghidra SNES extension | joshleaves/ghidra-snes provides SNES loading, a 65816 language definition, and memory/register helpers. | Optional static browsing and cross-references, supported by runtime evidence. |

Sources for the table: [DiztinGUIsh](https://github.com/DizTools/DiztinGUIsh), [Asar](https://github.com/RPGHacker/asar), [Zelda 3](https://github.com/snesrev/zelda3), [SNESRecomp](https://github.com/mstan/snesrecomp), [Mesen2-Diz](https://github.com/danielburgess/Mesen2-Diz), [Ghidra SNES](https://github.com/joshleaves/ghidra-snes).

Mesen's current community continuation is [nesdev-org/MesenCE](https://github.com/nesdev-org/MesenCE), which lists Apple Silicon development builds. The old [SourMesen/Mesen2](https://github.com/SourMesen/Mesen2) repository is archived and directs users there. Start with MesenCE for interactive inspection; for repeatable unattended comparisons, evaluate a headless harness around an independent established core.

The most defensible AI workflow is **observe → propose an interpretation → implement → compare → retain or reject**. Give AI small routines, register state, memory traces, caller context, and specific hypotheses. Require evidence for names, data structures, and formulas. AI is useful for accelerating those steps, but neither a plausible explanation nor successfully compiling C establishes fidelity. No general SNES-specific AI success rate was established by this research.

A useful caution is [decompbound](https://github.com/monofuel/decompbound): its author reports completing EarthBound on a newly written emulator, while separately reporting roughly 5.70% byte-exact decompiled ROM coverage and leaving native reimplementation as a future goal. A video of a game running can demonstrate a very different achievement from reconstructing its game logic.

**Findings specific to Super Saiya Densetsu.** Searches covered English titles, Chou Saiya Densetsu, and Japanese 超サイヤ伝説, alongside disassembly, decompilation, GitHub, hacking, and analysis terms. No complete public C decompilation or matching full disassembly was found. This is a search result, not proof none exists privately or under another name.

There is worthwhile existing research. [GameCenter GX](https://gcgx.games/dbz1/) explicitly studies ROM version 1.1, with a [battle calculation page](https://gcgx.games/dbz1/analyze.html), character data, encounters, and bugs. Treat its formulas as external hypotheses to confirm against this ROM's arithmetic, rounding, and random-number sequence. [TuxedoCyan's save-state hacking guide](https://gamefaqs.gamespot.com/snes/588289-dragon-ball-z-super-saiya-densetsu/faqs/10071) supplies historical leads; emulator save-state offsets must not be mistaken for SNES CPU addresses.

The similarly named [super-saiya-densetsu-2](https://github.com/gald89/super-saiya-densetsu-2) is a browser fan remake/continuation based on Dragon Ball Z III, not a decompilation of this SNES game. Existing translation work may help identify text systems, but should remain separate from the baseline original ROM.

**What was verified locally.** The supplied file is 1,048,576 bytes without a copier header. Its header declares LoROM, ROM/RAM/battery, 8 KiB SRAM, Japan, revision 1, and no enhancement chip. The calculated checksum equals the stored `33fd`, with a valid complement. Reset is at `00:8000`, native NMI at `00:80EB`, and native IRQ at `00:8154`. The internal title's `SFX` text does not mean it uses a Super FX chip. See [the full identity report](rom-identification.json) for hashes.

Standard cartridge hardware and the small image reduce the hardware scope. They do not establish how difficult the event interpreter, compressed assets, battle system, or complete story coverage will be. Those are still unknown.

The experiment built SNESRecomp revision `555322683a5dadba2d118e147af887f3d5c8188d` locally, supplied only the starter `auto_vectors` configuration, generated C, and used the generated CMake/Ninja build. Results:

| Measured item | Result |
| --- | --- |
| Analyzer roots | 9 |
| Discovered entry addresses | 375 |
| Register-width-specific function variants | 388 |
| Variants classified eligible for ahead-of-time C | 311 |
| Variants left for low-level interpretation | 77 |
| Banks with emitted C | 9 |
| Compilation | Successful ARM64 Mach-O objects and static library |
| Linked/running game | Not attempted; no host executable yet |
| Behavioral comparison | Not yet performed |

The reset variant itself remains interpreted because of `truncated_call_continuation`. Default compiler settings suppress warnings and allow implicit function declarations. Building an archive does not check that every external symbol can link. These results therefore establish **generation and compilation feasibility only**. They are not a percentage of the game decompiled, nor proof that any discovered routine is semantically correct. [Measured summary](recompiler-probe.json), [generation log](snesrecomp-generation.log), [build log](snesrecomp-build.log).

A second run through the saved reproduction script compiled successfully, produced the same summary, and generated all 11 C files byte-for-byte identically. The original ROM's SHA-256 remained unchanged. The logs linked above reflect this latest reproduction. [Reproduction summary](recompiler-probe-reproduction.json).

**Proposed architecture.** Keep game logic in portable C with fixed-width integer types and explicit arithmetic behavior. Keep platform-specific file access, controllers, audio output, timing, and window creation behind a small host layer, using SDL where practical and CMake for builds. Avoid making game logic depend on Objective-C or Apple APIs.

Initially reuse SNES graphics, audio, DMA, and memory-map implementations. That is compatible with native game logic: the PPU is hardware, and the SPC700 audio program is a separate processor workload. Track any remaining interpretation of the main game CPU explicitly. Once a system is understood, replace register-heavy generated routines with named C functions and compare against the unchanged baseline. Retain original bugs until intentional fixes have separate tests.

SNESRecomp's current [license](https://github.com/mstan/snesrecomp/blob/main/LICENSE) is PolyForm Noncommercial 1.0.0, with separate dependency terms. This is a concrete framework-selection constraint; it is not a permissively licensed drop-in dependency. Keep the experiment isolated while deciding how much runtime code to adopt. ROMs and extracted game data stay local; a future source release should use a user-supplied-ROM extraction flow.

**Milestones and acceptance criteria.**

1. Pin identity and tools; capture deterministic reset/title traces, inputs, frame images, audio, and relevant CPU/WRAM state. Preserve CPU width and bank context with discoveries.
2. Resolve boot and interrupt control flow and link a Mac host. Pass a repeatable boot/title comparison. Report main-CPU native execution versus fallback execution separately.
3. Build one representative gameplay slice: new game, overworld movement, menu interaction, a card battle, and save/reload. Recover and validate at least one named game subsystem, preferably battle calculations or card selection.
4. Expand across story events, maps, enemies, transformations, endings, and audio changes. Use deterministic input replays, subsystem comparisons, and a full playthrough corpus; reaching the title screen is only the first milestone.
5. Package the Mac app and compile the same core on another architecture/platform. Reduce main-CPU fallback toward zero over the documented corpus, while acknowledging unvisited paths. A readable reconstruction and a mechanically translated build remain separate progress measures.

For testing, use an independent emulator as the baseline for hardware output. Internal interpreted-versus-native comparisons help locate CPU translation errors but can share hardware bugs. Compare CPU state at agreed boundaries and WRAM/SRAM, video, and audio at controlled synchronization points. Record emulator version, initial save state, reset behavior, and inputs so mismatches can be reproduced. [SNESRecomp reference-debugging design](https://github.com/mstan/snesrecomp/blob/main/README.md#reference-debugging).

The decision is to begin with the boot/title and one-battle slice. Those results will determine whether SNESRecomp saves time or whether a more direct C reconstruction on an established hardware core is preferable. A full-game schedule would be speculation before that slice exposes the game's dispatch, asset, and script systems.
