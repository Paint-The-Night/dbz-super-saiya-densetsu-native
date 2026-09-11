# Translation patches (no ROMs)

This directory hosts **patch files only**. Commercial ROMs are never stored or
served. Players supply their own clean Japanese Rev 1; the browser validates
SHA-256, then optionally applies the English patch **in memory**.

## Klepto Software — English 1.02

| | |
|---|---|
| File | `klepto-ssd-en-1.02.ips` |
| SHA-256 | `a1607090a6534d6b1711413ae07252c492d6c4c809868a2b6265b32737bfeed8` |
| Size | 62,587 bytes |
| Source | [romhacking.net translation #326](https://www.romhacking.net/translations/326/) |
| Author | **Klepto Software** (SirYoink et al.) |
| Requires | **Headered** Japanese Rev 1 (512-byte copier header) |

Credit: English text © Klepto Software fan translation 1.02 (applied locally in
the player’s browser). See `klepto-readme.txt` for the upstream release notes.

### Apply path (web)

1. Validate **headerless** 1 MiB SHA-256  
   `962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c`
2. Prepend 512 zero bytes → headered image
3. Apply this IPS in RAM
4. Strip the 512-byte header → 1 MiB buffer for `snes_loadRom`

### Integrity

- **Japanese** = byte-identical Rev 1 (hash gate unchanged).
- **English** = same base + public Klepto text/font patch in RAM. Gameplay
  natives still run; some patched code bytes may interpreter-fallback if the
  IPS touches code the port has replaced.
