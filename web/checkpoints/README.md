# Opening Skip checkpoints

Host **SKIP** loads one of these `DBZCHK1` files (same format as
`--load-checkpoint`) to jump past title / crawl / Raditz into playable
Kame House overworld.

| File | Lang | Source |
|------|------|--------|
| `opening-ja.dbzstate` | JA | `tests/start-game.inputs` → frame 3000 (`ja_opening`) |
| `opening-en.dbzstate` | EN | `tests/en-to-overworld.inputs` → frame 4500 (`en_menu_smoke`) |

Both bind ROM SHA-256 `962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c`
(Japanese Rev 1). Regenerate:

```bash
ROM="Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc"
./build/dbz-port --rom "$ROM" --headless --frames 3000 \
  --inputs tests/start-game.inputs --lang ja --dump-dir /tmp/ck-ja
./build/dbz-port --rom "$ROM" --headless --frames 4500 \
  --inputs tests/en-to-overworld.inputs --lang en --dump-dir /tmp/ck-en
cp /tmp/ck-ja/final.dbzstate web/checkpoints/opening-ja.dbzstate
cp /tmp/ck-en/final.dbzstate web/checkpoints/opening-en.dbzstate
```

Verify load:

```bash
./build/dbz-port --rom "$ROM" --headless --frames 30 --lang en \
  --load-checkpoint web/checkpoints/opening-en.dbzstate
```
