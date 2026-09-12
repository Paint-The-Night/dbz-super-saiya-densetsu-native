# Shop / inn EN smoke — scout notes (2026-09-12)

Goal: short deterministic `--lang en` route from `ja_opening` / `kame-flight`
patterns to main-script face `$74` (idxs 72–80, 122–129) or inn/rest (83–86).

## Result

**Not cheap enough for an `en_shop_smoke` scene.** Mainland on-foot is
reachable, but Baba’s gray-brick shop / Bulma inn were not hit within ~8.5k
frames of scripted exploration. No `en_shop_smoke` inputs/golden landed.

## What worked (deterministic)

From `artifacts/…/ja_opening/final.dbzstate` (menu open at Kame House):

1. Reuse `research/scout/battle-route.inputs` through the Tenshinhan / “Enemies
   approach!” beat (~frames 761–1200; `bsel=1` idxs 88→89→236).
2. **B** (~1850) cancels the stuck flight **Land** menu so movement resumes.
3. Hold **Left** (~1900–2120, mask `040`) — flight axes: Up=`010` → −X,
   Left=`040` → −Y. Mainland grass appears around cam ≈ `(1530, 1846)`.
4. **A, A** with Land selected (EN chrome: Land / Item / Menu) lands on foot
   (~2350). On-foot menu is Talk / Look / Fly / Item / Menu (EN≠JA already
   covered by `en_menu_smoke` / chrome filter).

Proven as probe-g (~4600f): on-foot mainland + “Nothing there!” Look text.
Flight→land path alone is ~2.2–2.5k frames after opening (opening itself 3k).

## Why shop/inn still missing

- Baba / inn are **not** at the first coastal landfall; walkthroughs place them
  near the mountain cave further inland (gray brick = Baba, dome = Bulma inn).
- On-foot grid walks (probe-h/i, to ~8.5k total) keep hitting random encounters
  (`bsel=1` / `7`) that zero the camera and eat input. Clearing them and
  navigating to a specific building is a **long** map route, not a short smoke.
- No frame observed `bsel==2` with shop/inn idxs; `$0723/$0733` never entered
  the face-`$74` / inn set.

## Not done

- No `tests/*.inputs` / `en_shop_smoke` scene (would be extended-only and still
  incomplete without a building hit).
- No densetsu deploy (runtime untouched; temporary `frames.jsonl` scout fields
  reverted).

## Next time (if prioritized)

Script a longer inland fly (Left past first coast toward mountain cam), land
near gray/white building pixels, walk into door / Talk; expect multi-encounter
handling and likely `extended` ctest. Until then, tables remain EN-covered but
unsmoked for shop/inn.
