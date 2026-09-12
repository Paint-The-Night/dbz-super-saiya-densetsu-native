#!/usr/bin/env python3
"""Declarative multi-scene regression runner for dbz-port.

See docs/scene-regression.md and tests/scenes/README.md.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "tests" / "scenes" / "manifest.json"
DEFAULT_ROM_CANDIDATES = [
    os.environ.get("DBZ_TEST_ROM", ""),
    os.environ.get("DBZ_ROM", ""),
    str(ROOT / "Backup" / "Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc"),
]


def die(message: str, code: int = 1) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(code)


def resolve_rom(explicit: str | None) -> Path:
    candidates = [explicit] if explicit else []
    candidates.extend(DEFAULT_ROM_CANDIDATES)
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate).expanduser()
        if not path.is_absolute():
            path = (Path.cwd() / path).resolve()
        else:
            path = path.resolve()
        if path.is_file():
            return path
    die(
        "ROM not found. Pass --rom or set DBZ_TEST_ROM / DBZ_ROM "
        "(Japanese Rev 1 .sfc)."
    )
    raise AssertionError("unreachable")


def load_manifest(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text())
    if int(data.get("version", 0)) != 1:
        die(f"Unsupported manifest version in {path}")
    if not isinstance(data.get("scenes"), list) or not data["scenes"]:
        die(f"Manifest {path} has no scenes")
    return data


def scene_by_id(manifest: dict[str, Any], scene_id: str) -> dict[str, Any]:
    for scene in manifest["scenes"]:
        if scene["id"] == scene_id:
            return scene
    die(f"Unknown scene id: {scene_id}")
    raise AssertionError("unreachable")


def select_scenes(
    manifest: dict[str, Any], only: list[str] | None, include_extended: bool
) -> list[dict[str, Any]]:
    by_id = {s["id"]: s for s in manifest["scenes"]}
    if only is not None:
        missing = [sid for sid in only if sid not in by_id]
        if missing:
            die("Unknown --only id(s): " + ", ".join(missing))
        # Explicit --only includes extended stubs without requiring --extended.
        return [by_id[sid] for sid in only]

    selected: list[dict[str, Any]] = []
    for scene in manifest["scenes"]:
        if scene.get("extended") and not include_extended:
            continue
        selected.append(scene)
    return selected


def dependency_order(
    manifest: dict[str, Any], selected: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    """Ensure chain dependencies run first when present in the selection or required."""
    by_id = {s["id"]: s for s in manifest["scenes"]}
    needed: dict[str, dict[str, Any]] = {}

    def require(scene_id: str) -> None:
        if scene_id in needed:
            return
        scene = by_id.get(scene_id)
        if scene is None:
            die(f"Missing dependency scene: {scene_id}")
        parent = scene.get("load_from")
        if parent:
            require(parent)
        needed[scene_id] = scene

    for scene in selected:
        require(scene["id"])
    # Preserve selected order for the leaves; deps already inserted via require.
    ordered: list[dict[str, Any]] = []
    seen: set[str] = set()

    def add(scene: dict[str, Any]) -> None:
        parent = scene.get("load_from")
        if parent and parent not in seen:
            add(by_id[parent])
        if scene["id"] not in seen:
            ordered.append(scene)
            seen.add(scene["id"])

    for scene in selected:
        add(scene)
    # Also keep any auto-pulled deps that were not in selected (for checkpoint only).
    for scene_id, scene in needed.items():
        if scene_id not in seen:
            add(scene)
    return ordered


def repo_path(rel: str | None) -> Path | None:
    if not rel:
        return None
    path = Path(rel)
    if not path.is_absolute():
        path = ROOT / path
    return path.resolve()


def build_cmd(
    binary: Path,
    rom: Path,
    scene: dict[str, Any],
    dump_dir: Path,
    checkpoint: Path | None,
    capture_every: int | None,
) -> list[str]:
    cmd = [
        str(binary),
        "--rom",
        str(rom),
        "--headless",
        "--frames",
        str(int(scene["frames"])),
        "--dump-dir",
        str(dump_dir),
        "--lang",
        str(scene.get("lang") or "ja"),
    ]
    if scene.get("verify"):
        cmd.append("--verify")
    inputs = repo_path(scene.get("inputs"))
    if inputs is not None:
        if not inputs.is_file():
            die(f"Inputs missing for {scene['id']}: {inputs}")
        cmd += ["--inputs", str(inputs)]
    if checkpoint is not None:
        cmd += ["--load-checkpoint", str(checkpoint)]
    every = capture_every
    if every is None:
        every = int(scene["frames"])
    cmd += ["--capture-every", str(every)]
    return cmd


def read_report(dump_dir: Path) -> dict[str, Any]:
    path = dump_dir / "report.json"
    if not path.is_file():
        die(f"Missing report.json in {dump_dir}")
    return json.loads(path.read_text())


def read_trace(dump_dir: Path) -> list[dict[str, Any]]:
    path = dump_dir / "frames.jsonl"
    if not path.is_file():
        die(f"Missing frames.jsonl in {dump_dir}")
    return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]


def write_en_golden(scene: dict[str, Any], dump_dir: Path, golden_path: Path) -> None:
    trace = read_trace(dump_dir)
    if not trace:
        die(f"Empty trace for golden update: {scene['id']}")
    frames = int(scene["frames"])
    if len(trace) != frames:
        die(f"Trace length {len(trace)} != frames {frames} for {scene['id']}")
    sig_frames = sorted(
        {
            max(1, frames // 6),
            max(1, frames // 3),
            max(1, (2 * frames) // 3),
            max(1, (5 * frames) // 6),
            frames,
        }
    )
    last = trace[-1]
    golden = {
        "scene_id": scene["id"],
        "lang": scene.get("lang") or "en",
        "frames": frames,
        "final_video_hash": last["video_hash"],
        "final_pc": last["pc"],
        "signatures": [
            {
                "frame": n,
                "video_hash": trace[n - 1]["video_hash"],
                "pc": trace[n - 1]["pc"],
            }
            for n in sig_frames
        ],
    }
    golden_path.parent.mkdir(parents=True, exist_ok=True)
    golden_path.write_text(json.dumps(golden, indent=2) + "\n")
    print(f"UPDATED golden {golden_path}")


def check_smoke_en(scene: dict[str, Any], dump_dir: Path) -> None:
    report = read_report(dump_dir)
    if report.get("verification_enabled") is not False:
        die(f"{scene['id']}: expected verification_enabled false, got {report}")
    if report.get("equivalence_passed") is not None:
        die(f"{scene['id']}: expected equivalence_passed null, got {report}")
    if int(report.get("frames", -1)) != int(scene["frames"]):
        die(f"{scene['id']}: frame count mismatch in report: {report}")
    golden_rel = scene.get("golden")
    if not golden_rel:
        die(f"{scene['id']}: smoke_en scene missing golden path")
    golden_path = repo_path(golden_rel)
    assert golden_path is not None
    if not golden_path.is_file():
        die(
            f"{scene['id']}: golden missing at {golden_path}. "
            f"Run with --update-golden {scene['id']} when ROM is available."
        )
    golden = json.loads(golden_path.read_text())
    trace = read_trace(dump_dir)
    last = trace[-1]
    if last["video_hash"] != golden["final_video_hash"]:
        die(
            f"{scene['id']}: final video_hash {last['video_hash']} "
            f"!= golden {golden['final_video_hash']}"
        )
    if golden.get("final_pc") and last["pc"] != golden["final_pc"]:
        die(f"{scene['id']}: final pc {last['pc']} != golden {golden['final_pc']}")
    for sig in golden.get("signatures", []):
        frame = int(sig["frame"])
        if frame < 1 or frame > len(trace):
            die(f"{scene['id']}: signature frame {frame} out of range")
        row = trace[frame - 1]
        if row["video_hash"] != sig["video_hash"]:
            die(
                f"{scene['id']}: frame {frame} video_hash {row['video_hash']} "
                f"!= golden {sig['video_hash']}"
            )
        if sig.get("pc") and row["pc"] != sig["pc"]:
            die(
                f"{scene['id']}: frame {frame} pc {row['pc']} != golden {sig['pc']}"
            )


def check_verify(scene: dict[str, Any], dump_dir: Path) -> None:
    report = read_report(dump_dir)
    if report.get("verification_enabled") is not True:
        die(f"{scene['id']}: expected verification_enabled true, got {report}")
    if report.get("equivalence_passed") is not True:
        die(f"{scene['id']}: equivalence_passed is not true: {report}")
    if int(report.get("frames", -1)) != int(scene["frames"]):
        die(f"{scene['id']}: frame count mismatch in report: {report}")
    ckpt = dump_dir / "final.dbzstate"
    if not ckpt.is_file():
        die(f"{scene['id']}: missing final.dbzstate after verified run")


def run_scene(
    binary: Path,
    rom: Path,
    scene: dict[str, Any],
    work: Path,
    checkpoints: dict[str, Path],
    update_golden: str | None,
    keep_dumps: bool,
) -> str:
    scene_id = scene["id"]
    dump_dir = work / scene_id
    if dump_dir.exists():
        shutil.rmtree(dump_dir)
    dump_dir.mkdir(parents=True)

    checkpoint = None
    parent = scene.get("load_from")
    if parent:
        checkpoint = checkpoints.get(parent)
        if checkpoint is None or not checkpoint.is_file():
            die(
                f"{scene_id}: missing checkpoint from dependency {parent}. "
                "Ensure the parent scene runs first in this invocation."
            )

    cmd = build_cmd(binary, rom, scene, dump_dir, checkpoint, None)
    print(f"RUN {scene_id}: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        die(
            f"{scene_id}: dbz-port exited {result.returncode}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )

    mode = scene.get("mode") or ("smoke_en" if scene.get("lang") == "en" else "verify")
    if update_golden == scene_id or (
        update_golden == "*" and mode == "smoke_en"
    ):
        golden_rel = scene.get("golden")
        if not golden_rel:
            die(f"{scene_id}: cannot update golden (no golden path in manifest)")
        write_en_golden(scene, dump_dir, repo_path(golden_rel))  # type: ignore[arg-type]

    if mode in ("verify", "chain"):
        check_verify(scene, dump_dir)
    elif mode == "smoke_en":
        check_smoke_en(scene, dump_dir)
    else:
        die(f"{scene_id}: unknown mode {mode!r}")

    ckpt = dump_dir / "final.dbzstate"
    if ckpt.is_file():
        checkpoints[scene_id] = ckpt
    status = "PASS"
    if not keep_dumps and mode == "smoke_en":
        # Keep verify dumps only while dependents may need checkpoints; cleaned with work dir.
        pass
    print(f"{status}: {scene_id} ({mode}, {scene['frames']}f)")
    return status


def list_scenes(manifest: dict[str, Any]) -> None:
    print(f"{'ID':22} {'MODE':10} {'LANG':4} {'FRAMES':>6} EXT  NOTES")
    for scene in manifest["scenes"]:
        print(
            f"{scene['id']:22} {scene.get('mode',''):10} "
            f"{scene.get('lang',''):4} {int(scene['frames']):6d} "
            f"{'yes' if scene.get('extended') else 'no ':3}  "
            f"{scene.get('notes', '')}"
        )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary",
        required=True,
        help="Path to dbz-port (e.g. build/dbz-port)",
    )
    parser.add_argument(
        "--rom",
        default=None,
        help="Japanese Rev 1 ROM (else DBZ_TEST_ROM / DBZ_ROM / Backup/…)",
    )
    parser.add_argument(
        "--manifest",
        default=str(DEFAULT_MANIFEST),
        help="Scene manifest JSON",
    )
    parser.add_argument(
        "--only",
        default=None,
        help="Comma-separated scene ids (includes extended stubs if named)",
    )
    parser.add_argument(
        "--extended",
        action="store_true",
        help="Include extended=true stub scenes",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List scenes and exit",
    )
    parser.add_argument(
        "--update-golden",
        default=None,
        metavar="ID",
        help="Rewrite EN smoke golden for id (or * for all smoke_en runs)",
    )
    parser.add_argument(
        "--work-dir",
        default=None,
        help="Keep dump dirs here instead of a temp directory",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    manifest = load_manifest(Path(args.manifest).resolve())
    if args.list:
        list_scenes(manifest)
        return 0

    binary = Path(args.binary).resolve()
    if not binary.is_file():
        die(f"Binary not found: {binary}")
    rom = resolve_rom(args.rom)

    only = None
    if args.only:
        only = [part.strip() for part in args.only.split(",") if part.strip()]
        if not only:
            die("--only was empty")

    selected = select_scenes(manifest, only, args.extended)
    if not selected:
        die("No scenes selected (try --extended or --only)")
    ordered = dependency_order(manifest, selected)
    selected_ids = {s["id"] for s in selected}

    work_ctx: Any
    keep = bool(args.work_dir)
    if args.work_dir:
        work = Path(args.work_dir).resolve()
        work.mkdir(parents=True, exist_ok=True)
        work_ctx = nullcontext(work)
    else:
        work_ctx = tempfile.TemporaryDirectory(prefix="dbz-scenes-")

    checkpoints: dict[str, Path] = {}
    passed: list[str] = []
    with work_ctx as work_path:
        work = Path(work_path)
        for scene in ordered:
            # Dependencies pulled only for checkpoints still run (and verify)
            # so chain resumes are trustworthy.
            run_scene(
                binary,
                rom,
                scene,
                work,
                checkpoints,
                args.update_golden,
                keep_dumps=keep,
            )
            if scene["id"] in selected_ids:
                passed.append(scene["id"])

        dep_note = ""
        extras = [s["id"] for s in ordered if s["id"] not in selected_ids]
        if extras:
            dep_note = f" (also built deps: {', '.join(extras)})"
        print(f"PASS: {len(passed)} scene(s): {', '.join(passed)}{dep_note}")
    return 0


class nullcontext:
    def __init__(self, value: Path) -> None:
        self.value = value

    def __enter__(self) -> Path:
        return self.value

    def __exit__(self, *args: object) -> None:
        return None


if __name__ == "__main__":
    raise SystemExit(main())
