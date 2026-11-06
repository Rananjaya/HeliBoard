# CI/CD

RCBBoard uses two GitHub Actions workflows in `Rananjaya/RCBBoard`:

| Workflow | File | Runs on | Does |
| --- | --- | --- | --- |
| **CI** | `.github/workflows/ci.yml` | pull requests to `develop` | tests and a debug build; read-only, no secrets |
| **Release** | `.github/workflows/release.yml` | pushes to `develop` (for example a merged pull request) | version, signed universal APK, tag, GitHub Release |

`build-debug-apk.yml` (inherited from HeliBoard) still exists for building a debug APK by hand.

Neither workflow merges, commits, force-pushes, touches `main` or pushes anything to the HeliBoard upstream repository. The release workflow only runs in `Rananjaya/RCBBoard` on `develop`.

## CI: pull requests

Runs when a pull request to `develop` is opened, updated with new commits, reopened or marked ready for review. A newer push cancels the older run of the same pull request. Jobs (each shown separately in the pull request):

| Job | What it runs |
| --- | --- |
| Release script tests | `python3 .github/scripts/test_next_version.py` |
| Unit tests | `./gradlew --init-script .github/ci-unit-tests.init.gradle :app:testRunTestsUnitTest -Pandroid.onlyEnableUnitTestForTheTestedBuildType=false` on JDK 21. The `runTests` build type skips tests that already fail upstream; AGP 9 only creates its test task with this flag. The init script leaves out `XLinkTest`, which checks external web links and depends on other websites. |
| Native tests (ASan/UBSan) | `app/src/main/jni/tests/host/run_host_tests.sh` with clang and GoogleTest |
| Debug build | `./gradlew :app:assembleDebug`; the debug APK is attached to the run as an artifact |

The workflow has read-only repository access and no secrets, so code from a pull request (including one from someone else's fork) cannot sign or publish anything. When a job fails, open it in the **Checks** tab; unit test and build reports are attached to the run as artifacts.

## Releases

When commits reach `develop`, the release workflow:

1. **Determine version** (`.github/scripts/next_version.py`): finds the previous `rcbboard-v*` tag, checks every commit since then and works out the next version (see [Versioning](#versioning)). If only documentation, CI or tests changed, it stops here and nothing is published. If the previous tag has no published GitHub Release, it stops with recovery steps (see [Recovering a failed release](#recovering-a-failed-release)).
2. **Build** the universal release APK (all four ABIs: arm64-v8a, armeabi-v7a, x86, x86_64) from that exact commit with `-PrcbboardVersionCode` and `-PrcbboardVersionName` (`./gradlew :app:assembleRelease`, no Gradle cache).
3. **Sign** (`.github/scripts/sign_release.py`) with `zipalign` + `apksigner` and the existing release key from the `release` environment secrets. The keystore is decoded into the runner's temporary folder and deleted afterwards.
4. **Verify** the APK before anything is published: valid ZIP, aligned, `apksigner verify` with v1 and v2 signatures, exactly one signer whose certificate SHA-256 is `0b01373116d4a7b57646c4c7e2ffc28af91870bdbeabb4b8c05c00f313e2291b`, package `com.ranger.myheliboard`, the new versionCode and versionName, minSdk 21 and all four ABIs. Same package, same certificate and a higher versionCode mean it installs as an update over existing RCBBoard installations.
5. **Tag** the commit as `rcbboard-vX.Y.Z` in `Rananjaya/RCBBoard`.
6. **Publish** a GitHub Release with `RCBBoard-X.Y.Z.apk` and `SHA256SUMS.txt`. The release notes list the version, commit, build date, file name, installation steps and the certificate fingerprint. They do not list commit messages. The release is marked as *latest* only if no newer `rcbboard-v*` release is published.

Safety:

- Only one release run at a time (`concurrency: release-develop`); a queued run waits for the running one. If several pushes arrive while one is running, GitHub keeps only the newest queued run, which still includes all earlier commits.
- Existing tags are never moved or overwritten, and a release that already exists is never replaced. If a run fails after creating the tag, re-running it builds the same version again and creates the missing release; until then, later runs stop instead of releasing on top of it.
- The GitHub CLI uploads files to a draft and only publishes it when all uploads have succeeded; a failed upload deletes the draft again.
- Tags and releases created with the workflow token do not start other workflows, so there are no loops.
- If a secret is missing, the run stops before building, with an error naming the missing secret. An unsigned build is never published.

You can also start a release by hand: **Actions → Release → Run workflow** on `develop`, with `bump` = `auto`, `patch`, `minor` or `major`. A forced bump publishes even if only documentation changed.

## Recovering a failed release

If a release run fails after it has created the tag `rcbboard-vX.Y.Z` (for example while uploading), that tag has no published release. The next release run then stops with *"The newest release tag rcbboard-vX.Y.Z … has no published GitHub Release"*, so nothing is built on top of a version that was never published. Fix it in one of two ways:

- **Publish it:** open **Actions → Release**, find the failed run for that commit and choose **Re-run all jobs**. It builds the same version from the same commit and creates the missing release. Afterwards, re-run the run that stopped (or wait for the next push).
- **Drop it:** if that build must not be published, delete any draft release for the tag (Releases page), delete the tag with `git push origin --delete rcbboard-vX.Y.Z`, and re-run the run that stopped. The version number is then used for the next release.

A re-run of an older release is never marked as the *latest* release if a newer `rcbboard-v*` release has been published in the meantime. If a run was cancelled during the upload, a draft release can remain; delete it on the Releases page.

## Versioning

Versions are `MAJOR.MINOR.PATCH`, tagged `rcbboard-vMAJOR.MINOR.PATCH`. The prefix keeps RCBBoard's tags apart from HeliBoard's own `vX.Y` tags.

The bump is taken from [Conventional Commits](https://www.conventionalcommits.org/) in all commits since the previous release (merge commits themselves are skipped, the commits they bring in are counted). The largest bump wins:

| Commit | Bump | Example |
| --- | --- | --- |
| `type!:` or a `BREAKING CHANGE:` footer | MAJOR | `refactor!: change the backup format` |
| `feat:` | MINOR | `feat(layout): add a Tamil layout` |
| anything else that changes the app (`fix`, `perf`, `refactor`, `build`, `chore`, upstream merges, messages without a type) | PATCH | `fix: crash when opening settings` |
| commits that only change documentation, CI or tests | no release | `docs: update README` |

"Only documentation, CI or tests" means every changed file is under `.github/`, `docs/`, `art/`, `fastlane/`, `app/src/test/`, `app/src/androidTest/` or `app/src/main/jni/tests/`, or is a Markdown file or `.gitignore`/`.gitattributes`/`.editorconfig` in the repository root. Everything else counts as an app change.

With squash merges, the pull request title becomes the commit message, so give pull requests Conventional Commit titles.

**versionCode** = MAJOR × 10000 + MINOR × 100 + PATCH, for example 4.3.1 → 40301 and 4.4.0 → 40400. MINOR and PATCH must stay below 100. The first automatic release counts from 4.3.0 (after the manually built `v4.3-beta1`, versionCode 4300), so every automatic release has a higher versionCode than any earlier RCBBoard build.

`app/build.gradle.kts` keeps `4300` / `4.3-beta1` as defaults for local builds. A locally built APK therefore has a lower versionCode than the published releases and cannot be installed over them; build locally with `-PrcbboardVersionCode=… -PrcbboardVersionName=…` if you need that.

## One-time setup on GitHub

These settings are made by the repository owner on github.com; the workflows do not change them.

1. **Enable Actions.** GitHub disables workflows in forks by default: open the **Actions** tab and enable them.
2. **Actions permissions** (Settings → Actions → General):
   - *Actions permissions*: allow GitHub-made actions plus `gradle/actions` and `android-actions/setup-android` (or allow all actions). The workflows pin each action to a commit.
   - *Approval for running fork pull request workflows*: require approval for all external contributors.
   - *Workflow permissions*: **Read repository contents and packages permissions** (the release workflow asks for write access itself), and leave *Allow GitHub Actions to create and approve pull requests* unchecked.
3. **Environment** (Settings → Environments → New environment): name it `release`.
   - *Deployment branches and tags*: **Selected branches and tags**, add `develop`.
   - Optional: *Required reviewers* = yourself, to approve every release before it is built and published.
4. **Secrets** in the `release` environment (Settings → Environments → release → Add environment secret):

   | Secret | Value |
   | --- | --- |
   | `ANDROID_KEYSTORE_BASE64` | the existing release keystore, base64-encoded |
   | `ANDROID_KEYSTORE_PASSWORD` | keystore password |
   | `ANDROID_KEY_ALIAS` | key alias |
   | `ANDROID_KEY_PASSWORD` | key password |

   To encode the keystore on macOS without printing it: `base64 -i /path/to/release.keystore | pbcopy`, then paste it as the secret value. Use the existing key: a new key would produce APKs that cannot update existing installations. Never commit the keystore or its passwords.
5. Optional: protect `develop` (Settings → Branches) and require the CI checks to pass before merging.

Configure the secrets before merging the pull request that adds these workflows: that merge is itself a push to `develop` and starts the first release.

## Download and install a release

1. Open [Releases](https://github.com/Rananjaya/RCBBoard/releases) and download `RCBBoard-X.Y.Z.apk` on the Android device.
2. Open the file and allow installing from that source if asked. It updates an existing RCBBoard installation and keeps settings and learned words.
3. Enable RCBBoard in *Settings → System → Keyboard → On-screen keyboard*.

To check a download: `apksigner verify --print-certs RCBBoard-X.Y.Z.apk` must show the certificate SHA-256 above, and `shasum -a 256 -c SHA256SUMS.txt` checks the file.

## Troubleshooting

| Problem | What to do |
| --- | --- |
| No workflow runs at all | Actions are not enabled for the fork (setup step 1). |
| "Signing secrets missing" | Add the named secrets to the `release` environment (setup step 4), then re-run the failed run. |
| "ANDROID_KEYSTORE_BASE64 is not valid base64" or a signing error | Encode the keystore again without line breaks; check alias and passwords. |
| "APK signing certificate is …, expected …" | The secrets contain a different key. Use the existing release keystore. Nothing was published. |
| Release job waits for approval or is rejected by the environment | Approve it under the run, or check the `release` environment's deployment branches (`develop`). |
| "… has no published GitHub Release" | An earlier run failed after creating its tag; see [Recovering a failed release](#recovering-a-failed-release). |
| "tag … already exists on another commit" / "Tag exists" | A tag with that version points to a different commit. Do not move it; find out where it came from. A manual run with a larger bump creates the next free version. |
| "the clone is shallow" | The version job needs `fetch-depth: 0`; do not change it. |
| Unit tests fail | Download `unit-test-reports` from the run. Test locally with JDK 21 or newer, with the same command as CI (see [Testing locally](#testing-locally)). |
| Native tests fail | Run `app/src/main/jni/tests/host/run_host_tests.sh` locally (needs clang++, GoogleTest and a JDK). |
| Build fails with lint errors | Download the build reports from the run; `./gradlew :app:lintVitalRelease` shows the same errors locally. |
| Android SDK component not found | Check `ANDROID_PACKAGES` in the workflow against `compileSdk` and `ndkVersion` in `app/build.gradle.kts`. |

## Testing locally

```sh
python3 .github/scripts/test_next_version.py               # version rules
python3 .github/scripts/next_version.py --head origin/develop   # what the next release would be
./gradlew --init-script .github/ci-unit-tests.init.gradle :app:testRunTestsUnitTest \
    -Pandroid.onlyEnableUnitTestForTheTestedBuildType=false  # unit tests as in CI (JDK 21+)
app/src/main/jni/tests/host/run_host_tests.sh              # native tests
./gradlew :app:assembleDebug                               # debug build
./gradlew :app:assembleRelease -PrcbboardVersionCode=40301 -PrcbboardVersionName=4.3.1   # unsigned universal APK
```
