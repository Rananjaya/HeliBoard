# Sinhala word-list source

This directory holds the editable Sinhala word list. `main_si.combined` is compiled to `app/src/main/assets/dicts/main_si.dict`. The word list and the `.combined` file stay in this directory and are not packaged into the APK.

## Source

Repository: https://github.com/laknath/Sinhala-Dictionary

Exact commit: `cfcfb500d9e24c4101d9146e2795ec2f0074f385`

Selected file: `sinhala_full_word_list_2010-09-10.txt`

SHA-256: `5e6d95b2e1196cd77639015f3e1155b15adc727b252d0c890cb7d80c198f00bb`

The copy in this directory is byte-for-byte the file from that commit. It is not corrected, normalized, or filtered.

The 2016 word list in the same repository is not used. It contains phrases, duplicates, and entries that are not Sinhala words.

## Licence and attribution

The OpenOffice extension in that commit, `si_LK.oxt`, contains `LICENSES-en.txt`. A verbatim copy is in this directory. It says:

> Copyright (C) <2010> Laknath Semage <blaknath [at] gmail [dot] com>
>
> GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

The repository README names the author as Buddhika Laknath (`blaknath@gmail.com`) and the 2016 updater as ipman_98 (`ipnpd@yahoo.com`). The 2010 word list and the licence notice were added by the copyright holder named above. That notice does not say the authors endorse RCBBoard.

`GPL-3.0.txt` is the full GPL-3.0 licence text. The upstream notice points at `http://www.gnu.org/licenses/` and does not include that full text itself. The APK copies `GPL-3.0.txt` and `LICENSES-en.txt` to `assets/third_party_licenses/`.

## Conversion to `main_si.combined`

`main_si.combined` is a separate file. The original word list is left unchanged.

A line is kept when every character is in the Sinhala block `U+0D80..U+0DFF` or is U+200C or U+200D. This keeps 83,029 of the 83,061 lines. The other 32 lines contain a digit, `%`, `=`, a quotation mark, an en dash, or a Latin `x`. There are no duplicate kept lines and no empty lines.

The 39 lines that are not NFC are copied with their original code points. Spellings are not corrected.

Header:

```text
dictionary=main:si,locale=si,description=Sinhala word list from laknath/Sinhala-Dictionary commit cfcfb50,date=1284076800,version=1
```

`date=1284076800` is 2010-09-10 UTC, the date of the source file. `version=1` is the revision of this conversion. It is not the binary dictionary format version.

Each kept line is written as ` word=<word>,f=0`.

## Frequency

The source lines have no frequency. `dicttool_aosp.jar` and RCBBoard's combined-format reader both start a word at frequency 0 and replace it only when the line contains `f=`. A missing `f=` is therefore 0. Frequency 0 is a real terminal frequency. The "not a probability" value used by the dictionary engine is -1, so these words are still dictionary words.

`f=0` is written on every line so the file shows that default. It is not a popularity rank. Every kept word is equally likely. Prefix suggestions can still be returned, and they are not ordered by how common the word is. Autocorrect stays conservative because a low frequency produces a low score.

The dictionary-tool examples sometimes rewrite every frequency to a high value such as 254. That is a separate script, not the tool default. It would treat every Sinhala word as maximally common, so it is not used here.

## Unicode normalization

Ordinary Sinhala keystrokes are not rewritten. The only NFC step in input is `DeadKeyCombiner`, and it runs for the Latin combining range `U+0300..U+035B`. Sinhala al-lakuna (`U+0DCA`) is outside that range. Korean dictionary lookup uses its own NFD/NFC path.

The Sinhala layout has separate keys for `ෙ` (`U+0DD9`), `්` (`U+0DCA`), and `ා` (`U+0DCF`). It has no single key for `ේ` (`U+0DDA`) or `ෝ` (`U+0DDD`). Typing `ෙ` then `්` produces `U+0DD9 U+0DCA`. NFC makes that pair into `U+0DDA`. The same happens for `ො` + `්` and `U+0DDD`.

The 2010 list already stores `ේ` in 11,721 words and `ෝ` in 4,130 words. Only 39 kept words use the two-code-point spelling. For 34 of those 39, the NFC spelling is also present as its own entry. These five exist only in the two-code-point spelling:

- නේවිපිල්ලේ
- වින්ඳේ
- සැසඳේ
- හැටහතේ
- හේදෙන්න

English is unaffected by Sinhala NFC. English letters do not change under NFC.

The stored file keeps the original code points, including the 39 non-NFC spellings. Normalizing the stored list would drop the 34 duplicate spellings and change the five spellings above, and it still would not make a typed `ෙ` + `්` match a stored `ේ`.

Dictionary lookup does that match. `SinhalaLookup` runs a second exact query for the NFC form only when the typed word contains a Sinhala character and is not already NFC. Equal-score duplicates keep the NFC spelling. English and Korean strings are not normalized by this path. The Latin dead-key combiner is unchanged.

යංශය (`U+0DCA U+200D U+0DBA`), රකාරංශය (`U+0DCA U+200D U+0DBB`), and රේඵය are separate multi-code-point keys. Those keys commit immediately and are not added to the composing word. Their sequences are in the dictionary and match when the user types the code points one by one.
