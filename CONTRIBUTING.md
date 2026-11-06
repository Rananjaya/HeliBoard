# Contributing to RCBBoard

RCBBoard is my personal keyboard project, based on [HeliBoard](https://github.com/HeliBorg/HeliBoard) (which is based on OpenBoard and the AOSP keyboard). I develop it mainly for myself, friends and family. It is licensed under GPL-3.0, like HeliBoard.

Contributions are welcome, but there is no guarantee that a contribution will be accepted or reviewed quickly. If you are planning a larger change, it is a good idea to ask first.

Problems with the original HeliBoard app belong in [HeliBoard's own tracker](https://github.com/HeliBorg/HeliBoard/issues), not here.

This document has two parts: **RCBBoard rules** (Part 1) and **technical notes inherited from HeliBoard** (Part 2).

# Part 1 – RCBBoard rules

## Getting started

- Repository: https://github.com/Rananjaya/RCBBoard (pull requests go to the `develop` branch)
- Java: building needs **JDK 17 or newer**; the JVM unit tests need **JDK 21 or newer** (Robolectric runs Android SDK 36, which needs Java 21). The JDK bundled with Android Studio works for both.
- Never commit `local.properties`, keystores, passwords or other machine-specific or secret files.

## Principles

1. **Security first.** Treat external input as untrusted, especially imported files (dictionaries, layouts, backups) and the native dictionary code. Check sizes, offsets and lengths before using them.
2. **Privacy first.** Typed text, clipboard contents, learned words and dictionaries stay on the device. Do not log typed text or personal data.
3. **Offline only.** RCBBoard has no `INTERNET` permission and no network features. Do not add the permission, network libraries, analytics or crash reporting.
4. **Keep user data safe.** Changes must not lose learned words, typing history, settings or user dictionaries.
5. **Keep it simple.** Small, focused changes that use the existing code are easiest to review.

## Code

- Kotlin for new code where practical; otherwise follow the style of the file you are editing. Do not reformat unrelated code.
- Native code (`app/src/main/jni`): check every buffer access against its real size, avoid integer overflow, and add tests for parser changes.
- Do not change the package ID, app name, version, signing configuration or permissions without asking first.
- Keep the existing copyright and SPDX licence headers. New files get `// SPDX-License-Identifier: GPL-3.0-only`.
- No proprietary code or binary blobs. Code from elsewhere must be GPL-3.0 compatible, and its source must be named.

## Testing

Please test your change and say in the pull request how you tested it:

- **JVM unit tests** (JDK 21+): `./gradlew :app:testDebugUnitTest`
  Some tests already fail on the unchanged upstream code, and are not caused by RCBBoard: `CharacterIteratorTest` (`lastGraphemeFails`, `firstGraphemeFails`), `XLinkTest` (`layoutsLinks`, `readmeLinks`, `knownDictionaries`; these need network access), `InputLogicTest.insertLetterIntoWordHangulFails` and `StringUtilsTest.detectEmojiAtStartFails`. Your change should not add new failures.
- **Native tests**, if you changed `app/src/main/jni`: `app/src/main/jni/tests/host/run_host_tests.sh`
  Needs clang++, GoogleTest (for example `brew install googletest`) and a JDK for `jni.h` (set `JAVA_HOME`). The tests run with AddressSanitizer and UndefinedBehaviorSanitizer and must all pass. Known upstream failures are listed in `app/src/main/jni/tests/host/known_upstream_failures.txt`.
- **Build**: `./gradlew :app:assembleDebug` (the debug app installs as `com.ranger.myheliboard.debug`, next to the normal app).
- **Try it** on a device or emulator if your change affects typing, suggestions, layouts or the UI, including English and Sinhala (standard and Wijesekara layouts).
- **Security fixes** should come with a test that fails before the fix and passes after it.

## Git workflow

- `main` tracks the original HeliBoard upstream. Do not commit RCBBoard work to it.
- `develop` is the RCBBoard branch. Pull requests target `develop`.
- Use one branch per topic, for example `feature/…`, `security/…`, `chore/…` or `docs/…`.
- Changes reach `develop` through pull requests, which I merge.
- Never push to the original HeliBoard repository.
- Keep security fixes in their own commits, separate from unrelated changes.
- Do not rewrite history that has already been pushed.

## Pull requests

Please use the pull request template and describe:

- what changed and why,
- how you tested it,
- any security or privacy effects (or "none"),
- anything that changes existing behaviour or data,
- screenshots for UI changes.

One topic per pull request, please.

## AI tools

Claude Code, GitHub Copilot and other AI development tools are allowed. You remain responsible for understanding, reviewing and testing what they produce. See [AI_USAGE.md](AI_USAGE.md).

## Translations

RCBBoard has no translation platform of its own. Translations inherited from HeliBoard come in with upstream merges; to improve them, please contribute to [HeliBoard on Weblate](https://translate.codeberg.org/projects/heliboard/). Translations of RCBBoard-specific strings are welcome in pull requests.

## Dictionaries

- Bundled dictionaries need a compatible licence and a documented source, like the Sinhala dictionary in `third_party/sinhala-dictionary/` and the emoji dictionaries in `third_party/emoji-dictionaries/` (see also `licenses/THIRD_PARTY_NOTICES.md`).
- Other dictionaries can be imported in the app. Community dictionaries are collected in the [aosp-dictionaries repository](https://codeberg.org/Helium314/aosp-dictionaries).

## Licence and attribution

- RCBBoard is licensed under GPL-3.0 ([LICENSE](LICENSE)), and contributions are accepted under the same licence.
- Keep all existing copyright notices, licence files and attribution, including those of HeliBoard, OpenBoard, AOSP and third-party components (`LICENSE-Apache-2.0`, `LICENSE-CC-BY-SA-4.0`, `licenses/`, `third_party/`).
- Much of the code was written by the HeliBoard, OpenBoard and AOSP developers. Do not present inherited code as RCBBoard's own work.

## Security problems and conduct

- Do not report security problems publicly. See [SECURITY.md](SECURITY.md).
- Please be friendly. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

# Part 2 – Technical notes inherited from HeliBoard

These notes come from HeliBoard's contribution guide and still describe the inherited code.

## About the code

RCBBoard, like HeliBoard, is based on the AOSP keyboard, and in many places still contains mostly the original code. There are some extensions, and some parts have been replaced completely.
When working on this app, you will likely notice its rather large size, and quite different code styles and often ancient comments and _TODO_s, where the latter are typically untouched since AOSP times.
Unfortunately a lot of the old code is hard to read or to fully understand with all of its intended (and unintended) consequences.

Some hints for finding what you're looking for:
* Layouts: stored in `layouts` folder in assets, interpreted by `KeyboardParser` and `TextKeyData`
  * Popups: either on layouts, or in `locale_key_texts` (mostly letter variations for specific languages that are not dependent on layout)
* Touch and swipe input handling: `PointerTracker`
* Handling of keycode / text inputs: `InputLogic`
  * chain: `PointerTracker` -> `KeyboardActionListenerImpl` -> `LatinIME` -> `InputLogic`
* Suggestions: `DictionaryFacilitatorImpl`, `Suggest`, `InputLogic`, and `SuggestionStripView` (in order from creation to display, omitting the native library)
* Communication with the app / text field (inputs, reading current text): `RichInputConnection`
* Receiving events and information from the app / text field: `LatinIME`
* Settings are in `SettingsValues`, with some functionality in `Settings` and the default values in `Default`

Code that is especially prone to unintended effects, and hard to test (behaviour may depend on the app and OS version): `InputLogic`, `Suggest`, `RichInputConnection`.

## Adding / adjusting layouts

See [layouts.md](layouts.md#adding-new-layouts--languages) for how to add new layouts to the app. Please stay in line with other layouts regarding the popup keys.

When editing existing layouts, please consider that people should still get what they're used to. In case of doubt it might be better to add a new layout instead of overhauling existing layouts.
`locale_key_texts` files should only contain letters that are actually part of the language, with exception of the optional `more_popups_<...>.txt` files.

## Updating emojis

See make-emoji-keys tool [README](tools/make-emoji-keys/README.md).
