# Sinhala (Wijesekara) layout

RCBBoard has two Sinhala keyboard layouts:

| Layout | File | Notes |
|---|---|---|
| Sinhala | `app/src/main/assets/layouts/main/sinhala.json` | The layout from upstream HeliBoard. Unchanged. |
| **Sinhala (Wijesekara)** | `app/src/main/assets/layouts/main/sinhala_wijesekara.json` | Added in RCBBoard. Follows the Wijesekara standard and adds its AltGr characters as long-press keys. |

Both use the same Sinhala dictionary, suggestions, autocorrection and learned words.

## Enable and select the layout

1. Open RCBBoard settings → **Languages & Layouts**.
2. Tap **Sinhala** (add it first if it isn't in the list).
3. Under **Layout**, choose **Sinhala (Wijesekara)**. To go back, choose **Sinhala**.
4. Switch languages with the globe/language key or a long-press on the space bar, as usual.

## How it works

- **Normal and Shift layers** follow the Wijesekara arrangement of SLS 1134 (Rev. 2:2004). Each key's Normal and Shift characters match both the Microsoft "Sinhala - Wij 9" layout and the GNU/LKLUG Wijesekera keymap. The two sources differ on one key (X), noted below.
- **Shift** applies only when you press Shift (or Caps Lock). Automatic capitalisation at the start of a sentence does not switch layers, because Sinhala has no upper case.
- **Long-press** adds the characters that a PC keyboard reaches with AltGr: independent vowels (ආ ඇ ඈ ඌ ඒ ඓ ඕ ඖ ඎ ඏ ඐ) and two-part vowel signs (ේ ෛ ො ෝ ෞ ෲ ෳ). Which key each one is on is an RCBBoard choice (grouped with a related character); the characters themselves are those of the standard.
- **Independent vowels** are typed directly as single characters (for example ආ is U+0D86). They are never built from a vowel sign.
- **Typing order** is the Unicode (logical) order used by the Microsoft and SLS layouts: consonant first, then the vowel sign. ක then ා gives කා; ක then ෙ gives කෙ. Typewriter-style "kombuva first" input is not emulated.
- **Nothing is merged or reordered.** The keyboard sends exactly the code points on the keys. ෙ followed by ා stays U+0DD9 U+0DCF (shown as ො), which the Sinhala dictionary lookup already treats the same as ො U+0DDC.

## Key mapping

Key = the key position on a US keyboard, as used by the standard. Code points are listed after each character. ¹ = long-press from `locale_key_texts/si.txt`, which applies to both Sinhala layouts. The app also adds digits and symbols as long-press options on the top row of every layout; those are not listed.

### Row 1

| Key | Normal | Shift | Long-press (normal) | Long-press (Shift) |
|---|---|---|---|---|
| Q | ු `U+0DD4` | ූ `U+0DD6` | — | — |
| W | අ `U+0D85` | උ `U+0D8B` | ආ `U+0D86`, ඇ `U+0D87`, ඈ `U+0D88` | ඌ `U+0D8C` |
| E | ැ `U+0DD0` | ෑ `U+0DD1` | — | — |
| R | ර `U+0DBB` | ඍ `U+0D8D` | — | ඎ `U+0D8E` |
| T | එ `U+0D91` | ඔ `U+0D94` | ඒ `U+0D92`, ඓ `U+0D93` | ඕ `U+0D95`, ඖ `U+0D96` |
| Y | හ `U+0DC4` | ශ `U+0DC1` | — | — |
| U | ම `U+0DB8` | ඹ `U+0DB9` | — | — |
| I | ස `U+0DC3` | ෂ `U+0DC2` | — | — |
| O | ද `U+0DAF` | ධ `U+0DB0` | ඳ `U+0DB3` ¹ | — |
| P | ච `U+0DA0` | ඡ `U+0DA1` | — | — |
| [ | ඤ `U+0DA4` | ඥ `U+0DA5` | ෴ `U+0DF4` ¹ | ෴ `U+0DF4` ¹ |

### Row 2

| Key | Normal | Shift | Long-press (normal) | Long-press (Shift) |
|---|---|---|---|---|
| A | ් `U+0DCA` | ෟ `U+0DDF` | ෳ `U+0DF3` | — |
| S | ි `U+0DD2` | ී `U+0DD3` | — | — |
| D | ා `U+0DCF` | ෘ `U+0DD8` | ෲ `U+0DF2` | — |
| F | ෙ `U+0DD9` | ෆ `U+0DC6` | ේ `U+0DDA`, ෛ `U+0DDB`, ො `U+0DDC`, ෝ `U+0DDD`, ෞ `U+0DDE` | — |
| G | ට `U+0DA7` | ඨ `U+0DA8` | — | — |
| H | ය `U+0DBA` | ්ය `U+0DCA U+200D U+0DBA` | — | — |
| J | ව `U+0DC0` | ළු `U+0DC5 U+0DD4` | — | — |
| K | න `U+0DB1` | ණ `U+0DAB` | — | — |
| L | ක `U+0D9A` | ඛ `U+0D9B` | — | — |
| ; | ත `U+0DAD` | ථ `U+0DAE` | — | — |
| ` | ්ර `U+0DCA U+200D U+0DBB` | ර් `U+0DBB U+0DCA U+200D` | — | — |

### Row 3

| Key | Normal | Shift | Long-press (normal) | Long-press (Shift) |
|---|---|---|---|---|
| X | ං `U+0D82` | ඞ `U+0D9E` | ඃ `U+0D83` | — |
| C | ජ `U+0DA2` | ඣ `U+0DA3` | ඦ `U+0DA6` ¹ | — |
| V | ඩ `U+0DA9` | ඪ `U+0DAA` | ඬ `U+0DAC` ¹ | — |
| B | ඉ `U+0D89` | ඊ `U+0D8A` | — | — |
| N | බ `U+0DB6` | භ `U+0DB7` | — | — |
| M | ප `U+0DB4` | ඵ `U+0DB5` | — | — |
| , | ල `U+0DBD` | ළ `U+0DC5` | ඏ `U+0D8F`, ඐ `U+0D90` | — |
| . | ග `U+0D9C` | ඝ `U+0D9D` | ඟ `U+0D9F` ¹ | — |
| \ | ් `U+0DCA U+200D` | ZWJ `U+200D` | ZWNJ `U+200C` | — |

The ASCII punctuation that the standard puts on Z (' "), ] (; :) and ' (. ,) is left out, because it is on the symbols page and bottom row.

### Differences from the existing Sinhala layout

The 31 letter keys (Normal and Shift) are the same in both layouts. The differences:

| Position | Sinhala (existing) | Sinhala (Wijesekara) |
|---|---|---|
| Row 2, last key | ඏ / ්‍ර (rakaransaya) | ්‍ර (rakaransaya) / ර්‍ (repaya), the standard ` key |
| Row 3, last key | ෳ / ර්‍ (repaya) | ්‍ (join: al-lakuna + ZWJ) / ZWJ; long-press ZWNJ |
| Long-press | from `si.txt` only | adds about 20 characters (listed above) |

## Multi-character keys

| Key | Output | Use |
|---|---|---|
| ` | ්‍ර `U+0DCA U+200D U+0DBB` | rakaransaya, e.g. ශ + ්‍ර + ී = ශ්‍රී |
| Shift + ` | ර්‍ `U+0DBB U+0DCA U+200D` | repaya, e.g. ක + ර්‍ + ම = කර්‍ම |
| Shift + H | ්‍ය `U+0DCA U+200D U+0DBA` | yansaya, e.g. ක + ්‍ය = ක්‍ය |
| \ | ්‍ `U+0DCA U+200D` | join (touching letters), e.g. ක + ්‍ + ෂ = ක්‍ෂ |
| Shift + \ | ZWJ `U+200D` | zero width joiner |
| long-press \ | ZWNJ `U+200C` | zero width non-joiner |

## Decisions

- **X key:** Shift gives ඞ (GNU/SLS order, same as the existing layout); ඃ is on long-press. Microsoft's Wij 9 has these the other way round.
- **"Touch" (ZWJ before al-lakuna):** not included. The sources disagree and the join key covers modern conjuncts.
- **Microsoft's private-use characters** on Shift+J and \ are not used: Shift+J gives ළු (U+0DC5 U+0DD4).

## Known limitations

- Multi-character keys (්‍ර, ර්‍, ්‍ය, ්‍) commit their text straight away instead of joining the word being typed. Suggestions restart after them. The existing Sinhala layout behaves the same.
- Backspace removes one character at a time. A consonant joined with al-lakuna + ZWJ (for example ශ්‍) is removed in one press, so no stray ZWJ is left. This is the app's normal delete behaviour.

## Sources and licences

No code or data was copied from these sources. The key positions of a keyboard standard are facts; the layout file was written for RCBBoard.

| Source | Used for | Licence |
|---|---|---|
| [Microsoft "Sinhala - Wij 9" (KBDSW09), via kbdlayout.info](https://kbdlayout.info/kbdsw09/) | Normal, Shift and AltGr characters with code points | © Microsoft; used as a reference only |
| [GNU/LKLUG Sinhala Wijesekera Compatible Keymap](https://ftp.gwdg.de/pub/gnu/www/savannah-checkouts/non-gnu/sinhala/doc/keymaps/sinhala-keyboard_1.html) | Cross-check of Normal and Shift; follows SLS 1134 Rev. 2:2004 | None stated; reference only |
| [para-d/si-wijesekara-keyboard](https://github.com/para-d/si-wijesekara-keyboard) | Cross-check (XKB file) | Unlicense |
| [Sithakuru](https://codeberg.org/sinhala/sithakuru) | Checked; its layout file could not be read | GPL-3.0-only; nothing used |
| HeliBoard `sinhala.json` | JSON structure | GPL-3.0 (same as RCBBoard) |

## Tests

- `app/src/test/java/helium314/keyboard/SinhalaWijesekaraLayoutTest.kt`: JSON shape, every key's Normal and Shift output, long-press options, Unicode ranges, registration next to the existing layout, and a checksum that the existing `sinhala.json` is unchanged.
- `app/src/test/java/helium314/keyboard/latin/InputLogicTest.kt` (tests starting with `wijesekara`, and the layout switching test): typing through the input pipeline, ZWJ sequences, backspace, the composing word, and switching between English, Sinhala (Wijesekara) and Sinhala.
