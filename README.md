<div align="center">

<img src="docs/images/rcbboard-icon.png" width="112" alt="RCBBoard app icon">

# RCBBoard

**A private, offline keyboard for Android, for English and Sinhala.**

[![Source version 4.3-beta1](https://img.shields.io/badge/source%20version-4.3--beta1-informational)](#download-and-install)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)
[![Android 5.0+](https://img.shields.io/badge/Android-5.0%2B-3DDC84)](#download-and-install)
[![No internet permission](https://img.shields.io/badge/internet%20permission-none-brightgreen)](#privacy-and-security)

[Download](#download-and-install) · [Features](#features) · [English and Sinhala](#english-and-sinhala) · [Privacy](#privacy-and-security) · [Contributing](#contributing)

</div>

---

RCBBoard is an independent fork of [HeliBoard](https://github.com/HeliBorg/HeliBoard) by Helium314. It is not an official HeliBoard release and is not endorsed by the original maintainers.

RCBBoard is a personal project by [Rananjaya](https://github.com/Rananjaya), made mainly for my own use and to share with friends and family. Most of the code comes from HeliBoard and the projects it is based on (see [Credits](#credits)); RCBBoard adds its own changes, such as a bundled Sinhala dictionary and the Sinhala (Wijesekara) layout.

## Screenshots

<table>
  <tr>
    <td align="center"><img src="docs/images/screenshots/english-suggestions.png" width="250" alt="English typing with word suggestions"><br><sub>English suggestions</sub></td>
    <td align="center"><img src="docs/images/screenshots/sinhala-wijesekara.png" width="250" alt="Sinhala Wijesekara layout with Sinhala suggestions"><br><sub>Sinhala · Wijesekara</sub></td>
    <td align="center"><img src="docs/images/screenshots/emoji-suggestions.png" width="250" alt="Emoji suggestion for the word pizza"><br><sub>Emoji suggestions</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/images/screenshots/next-word-dark.png" width="250" alt="Next-word predictions in the dark theme"><br><sub>Next-word predictions</sub></td>
    <td align="center"><img src="docs/images/screenshots/sinhala-dark.png" width="250" alt="Sinhala sentence typed in the dark theme"><br><sub>Sinhala · dark theme</sub></td>
    <td align="center"><img src="docs/images/screenshots/symbols-toolbar.png" width="250" alt="Symbols layout with the editing toolbar"><br><sub>Symbols and toolbar</sub></td>
  </tr>
</table>

## Features

- **Suggestions and spell check** from dictionaries, with next-word predictions that learn from what you type.
- **Emoji search and suggestions**, inline and in a separate search, using the bundled emoji dictionaries.
- **Themes**: styles, colours and a background image; follows the system day/night setting on Android 10+ (and some versions of Android 9) and dynamic colours on Android 12+.
- **Custom layouts**: edit the main keyboard [layouts](layouts.md) (when *use system languages* is turned off) and special layouts such as symbols, numbers and the functional keys.
- **Clipboard history**, one-handed mode, split keyboard and a number pad.
- **Backup and restore** of settings and learned words / history.
- **More dictionaries** can be added: build your own, or get them from the [dictionaries repository](https://codeberg.org/Helium314/aosp-dictionaries#dictionaries) (quality may vary).
- **Glide typing** needs a closed-source library that is **not included in RCBBoard**, because no compatible open-source library exists. Without it, glide typing is unavailable. If you want it, you have to add the library yourself: it can be extracted from GApps packages ("*swypelibs*"), or downloaded [here](https://github.com/erkserkserks/openboard/tree/46fdf2b550035ca69299ce312fa158e7ade36967/app/src/main/jniLibs) (click on the file and then "raw" or the download button).

The [HeliBoard wiki](https://github.com/HeliBorg/HeliBoard/wiki) explains these features in detail, including the [FAQ](https://github.com/HeliBorg/HeliBoard/wiki/FAQ) and [hidden features](https://github.com/HeliBorg/HeliBoard/wiki/9.-Hidden-features).

## English and Sinhala

RCBBoard is set up for two languages: **English (US)** and **Sinhala**. You can switch between them with the globe key.

| Layout | What you get |
| --- | --- |
| **English (US)** – QWERTY | Suggestions, autocorrection, next-word predictions and emoji suggestions. |
| **Sinhala** | Sinhala word suggestions from the bundled Sinhala dictionary, and Sinhala emoji suggestions. |
| **Sinhala (Wijesekara)** | The standard Wijesekara typewriter arrangement, with the same Sinhala suggestions. |

**Typing in Wijesekara.** Type in Unicode order: the consonant first, then the vowel sign. For example, ක then ා gives කා, and ක then ෙ gives කෙ. Characters that a PC keyboard reaches with AltGr (such as independent vowels and two-part vowel signs) are on long-press. See the [Wijesekara layout notes](docs/sinhala-wijesekara-layout.md) for the full key map and sources.

The Sinhala dictionary and the emoji dictionaries are bundled with their sources and licences documented in [`third_party/`](third_party/) and [`licenses/`](licenses/).

## Privacy and security

- **No internet permission.** RCBBoard does not have Android's `INTERNET` permission, so it cannot directly transmit what you type over a network.
- **Your data stays on your device.** Learned words, clipboard history, dictionaries and settings are stored locally. They leave the device only in a backup you export yourself, or in Android's own device backup if you have that turned on.
- **Few permissions.** Besides vibration, the system user dictionary and being notified when the device starts, RCBBoard only asks for contacts access if you turn on contact-name suggestions.
- **Hardened v2 dictionary reader.** The native code that reads dictionary files in the v2 format (the format of the bundled dictionaries and of dictionary files you import) checks every read against the real file size. It is covered by regression tests run with AddressSanitizer and UndefinedBehaviorSanitizer.
- **Still under review: v4 user-history dictionaries.** The learned-words dictionary that RCBBoard writes on your device uses the separate v4 format. Its native code is still being audited and has not received the same hardening, so RCBBoard does not claim that every native dictionary format is fully protected.
- **Found a security problem?** Please report it privately, as described in [SECURITY.md](SECURITY.md).

## Download and install

1. Download an APK from the [Releases page](https://github.com/Rananjaya/RCBBoard/releases).
2. Install it, then enable **RCBBoard** in Android's keyboard settings (usually *Settings → System → Keyboard → On-screen keyboard*) and select it as your keyboard.

RCBBoard needs Android 5.0 or newer. Release APKs are signed with this certificate; you can check it with `apksigner verify --print-certs` ("certificate SHA-256 digest"):

```
0b01373116d4a7b57646c4c7e2ffc28af91870bdbeabb4b8c05c00f313e2291b
```

> [!IMPORTANT]
> The APK currently on the Releases page (`v4.3-beta1`) was built before the v2 dictionary security fixes described in [Privacy and security](#privacy-and-security). A release that includes them has not been published yet; until then, you can build RCBBoard from source.

> [!NOTE]
> The F-Droid, GitHub release and IzzyOnDroid listings for HeliBoard install the original project, package `helium314.keyboard`. They do not install RCBBoard (`com.ranger.myheliboard`).

**Build from source:** `./gradlew assembleRelease` builds an unsigned release APK (JDK 17 or newer; see [CONTRIBUTING.md](CONTRIBUTING.md#getting-started)). Source code: https://github.com/Rananjaya/RCBBoard/tree/develop

## Customisation resources

Themes, layouts and dictionaries made for HeliBoard usually work in RCBBoard too. The resources below belong to the **upstream HeliBoard community** and other independent projects. They are **not RCBBoard support channels**; please report RCBBoard problems in this repository instead (see [Contributing](#contributing)).

- **Themes** can be saved and loaded from the menu in the *adjust colours* screen. Collections: [Star-Trowa/heliboard-themes](https://github.com/Star-Trowa/heliboard-themes) and [PickleHik3/droid-tings](https://github.com/PickleHik3/droid-tings); custom colours are shared in [HeliBoard's discussions](https://github.com/HeliBorg/HeliBoard/discussions/categories/custom-colors).
- **Layouts** are text files you can edit, copy and share (see [layouts.md](layouts.md)). Custom layouts are shared in [HeliBoard's discussions](https://github.com/HeliBorg/HeliBoard/discussions/categories/custom-layout).
- **Dictionaries** are built from a wordlist in [this format](https://codeberg.org/Helium314/aosp-dictionaries/src/branch/main/wordlists/sample.combined) (see [wordlist information](https://codeberg.org/Helium314/aosp-dictionaries#wordlist-information)), compiled with [external tools](https://github.com/remi0s/aosp-dictionary-tools), and can be shared or added to the [dictionaries repository](https://codeberg.org/Helium314/aosp-dictionaries/). A wordlist can also be loaded directly in the app.
- **HeliBoard community:** [GitHub discussions](https://github.com/HeliBorg/HeliBoard/discussions), [Lemmy](https://lemmy.world/c/Heliboard) and [Reddit](https://www.reddit.com/r/HeliBoard), where these resources are shared.

Other related projects: [k3lp](https://codeberg.org/k3lp/k3lp), a WIP library for keyboard layout parsing (created by [FlorisBoard](https://github.com/florisboard/florisboard/) maintainers), and [swipe-o-scope](https://codeberg.org/eclexic/swipe-o-scope) for visualising gesture data.

## Contributing

RCBBoard is a personal project: contributions are welcome, but there is no guarantee that a report will be answered or a pull request accepted.

- **Problems and ideas:** use the RCBBoard repository and its issue templates. Search existing issues first, keep one topic per issue, and see this [guide to effective bug reporting](https://www.chiark.greenend.org.uk/~sgtatham/bugs.html). Problems with the original HeliBoard app belong in [its own tracker](https://github.com/HeliBorg/HeliBoard/issues).
- **Security problems:** never in public issues; see [SECURITY.md](SECURITY.md).
- **Code:** see [CONTRIBUTING.md](CONTRIBUTING.md), [AI_USAGE.md](AI_USAGE.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Pull requests go to `Rananjaya/RCBBoard`, branch `develop`, never to HeliBoard. GitHub may preselect `HeliBorg/HeliBoard` and `main` because RCBBoard is a fork, so please check the base before creating a pull request (see [Where pull requests go](CONTRIBUTING.md#where-pull-requests-go)).
- **Translations:** inherited translations come from HeliBoard, so please improve them on [HeliBoard's Weblate](https://translate.codeberg.org/projects/heliboard/). Translations of RCBBoard-specific strings are welcome in pull requests.

## License

RCBBoard is licensed under GNU General Public License v3.0. It is a fork of HeliBoard, which is a fork of OpenBoard.

 > Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license. Copyright and license notices must be preserved. Contributors provide an express grant of patent rights.

See repo's [LICENSE](LICENSE) file.

Since the app is based on Apache 2.0 licensed AOSP Keyboard, an [Apache 2.0](LICENSE-Apache-2.0) license file is provided.
The original HeliBoard icon is licensed under [Creative Commons BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). A [license file](LICENSE-CC-BY-SA-4.0) is also included. RCBBoard uses its own launcher icon.

## Credits

- Original HeliBoard icon by [Fabian OvrWrt](https://github.com/FabianOvrWrt) with contributions from The Eclectic Dyslexic
- [OpenBoard](https://github.com/openboard-team/openboard)
- [AOSP Keyboard](https://android.googlesource.com/platform/packages/inputmethods/LatinIME/)
- [LineageOS](https://review.lineageos.org/admin/repos/LineageOS/android_packages_inputmethods_LatinIME)
- [Simple Keyboard](https://github.com/rkkr/simple-keyboard)
- [Indic Keyboard](https://gitlab.com/indicproject/indic-keyboard)
- [FlorisBoard](https://github.com/florisboard/florisboard/)
- HeliBoard and its [contributors](https://github.com/HeliBorg/HeliBoard/graphs/contributors)
