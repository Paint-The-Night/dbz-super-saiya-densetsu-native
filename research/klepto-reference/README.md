# Klepto Software English patch — reference only

This directory holds the public **Klepto Software** Super Saiya Densetsu English
IPS 1.02 and its release notes for **wording / research reference**.

## Hard rule

- **Do not** load, apply, or ship these files at runtime.
- The product boots only the clean Japanese Rev 1 ROM.
- English localization is a **native C** project (`src/i18n.*`, dialogue/font
  hooks). Klepto text may inform EN string tables; it must never be applied as
  IPS against the live ROM (that path caused mojibake when natives still ran JP
  print routines over patched bytes).

See `docs/i18n.md`.

## Upstream

| | |
|---|---|
| File | `klepto-ssd-en-1.02.ips` |
| SHA-256 | `a1607090a6534d6b1711413ae07252c492d6c4c809868a2b6265b32737bfeed8` |
| Source | [romhacking.net translation #326](https://www.romhacking.net/translations/326/) |
| Author | **Klepto Software** (SirYoink et al.) |
| Requires (if applied offline) | Headered Japanese Rev 1 |

Credit: English text © Klepto Software fan translation 1.02 — reference wording
only in this tree.
