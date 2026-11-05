# Emoji dictionary source

These files are the editable source corresponding to the compiled emoji dictionaries bundled in the app:

| Editable source | Compiled dictionary in the app |
| --- | --- |
| `emoji_en.combined` | `app/src/main/assets/dicts/emoji_en.dict` |
| `emoji_si.combined` | `app/src/main/assets/dicts/emoji_si.dict` |

The upstream repository is [Helium314/aosp-dictionaries](https://codeberg.org/Helium314/aosp-dictionaries).

The exact upstream commit is `ff0cd52be8d9530200fefe8b296c53d4f662091e`.

The files here are copies of these paths at that commit:

- https://codeberg.org/Helium314/aosp-dictionaries/src/commit/ff0cd52be8d9530200fefe8b296c53d4f662091e/emoji_cldr_signal_wordlists/emoji_en.combined
- https://codeberg.org/Helium314/aosp-dictionaries/src/commit/ff0cd52be8d9530200fefe8b296c53d4f662091e/emoji_cldr_signal_wordlists/emoji_si.combined

These `.combined` files are not packaged into the APK. The compiled `.dict` files are.

## Compiling

The upstream README builds a `.dict` file with `dicttool_aosp.jar`:

```text
java -jar dicttool_aosp.jar makedict -s emoji_en.combined -d emoji_en.dict
java -jar dicttool_aosp.jar makedict -s emoji_si.combined -d emoji_si.dict
```

`dicttool_aosp.jar` at the same commit is:

https://codeberg.org/Helium314/aosp-dictionaries/src/commit/ff0cd52be8d9530200fefe8b296c53d4f662091e/dicttool_aosp.jar

The upstream README says that jar is taken from:

https://github.com/remi0s/aosp-dictionary-tools

Licensing and attribution for these dictionaries are in [`licenses/THIRD_PARTY_NOTICES.md`](../../licenses/THIRD_PARTY_NOTICES.md).
