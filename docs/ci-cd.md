# CI/CD

RCBBoard is built and released in three stages. The **same commit** moves through all of them:

```
pull request ──► CI checks
     │ merge into develop
     ▼
develop @ abc123 ──► Development (automatic) ──► QA (you start it) ──► Production (you start it, you approve)
                     Debug APK                    Debug APK              signed release + tag
```

| Stage | Workflow | Starts | Builds | App on the phone |
| --- | --- | --- | --- | --- |
| Checks | `.github/workflows/ci.yml` | every pull request to `develop` | tests + debug build | – |
| Development | `.github/workflows/development.yml` | every push to `develop` (a merged pull request), or by hand | `:app:assembleDebug` | **RCBBoard debug** (`com.ranger.myheliboard.debug`) |
| QA | `.github/workflows/qa.yml` | by hand, with a commit SHA | `:app:assembleDebug` | **RCBBoard debug** (`com.ranger.myheliboard.debug`) |
| Production | `.github/workflows/release.yml` | by hand, with the QA-approved commit SHA | `:app:assembleRelease` | **RCBBoard** (`com.ranger.myheliboard`) |

Development and QA build the same existing `debug` build type, so the QA APK is the same app as the Development APK and replaces it. The debug app and the production app have different application IDs, so both can be installed on the same phone.

`build-debug-apk.yml` (inherited from HeliBoard) still exists for building a debug APK by hand.

No workflow merges, commits, force-pushes, touches `main` or pushes anything to the HeliBoard upstream repository. Development, QA and Production only run in `Rananjaya/RCBBoard` on `develop`. Development and QA sign with a separate, persistent [debug signing key](#debug-signing-key); only Production uses the production signing key.

## CI: pull requests

Runs when a pull request to `develop` is opened, updated with new commits, reopened or marked ready for review. A newer push cancels the older run of the same pull request. Development reuses the same jobs (as a reusable workflow, on the exact commit it builds):

| Job | What it runs |
| --- | --- |
| Release script tests | `python3 -m unittest discover -s .github/scripts -p 'test_*.py'` |
| Unit tests | `./gradlew --init-script .github/ci-unit-tests.init.gradle :app:testRunTestsUnitTest -Pandroid.onlyEnableUnitTestForTheTestedBuildType=false` on JDK 21. The `runTests` build type skips tests that already fail upstream; AGP 9 only creates its test task with this flag. The init script leaves out `XLinkTest`, which checks external web links and depends on other websites. |
| Native tests (ASan/UBSan) | `app/src/main/jni/tests/host/run_host_tests.sh` with clang and GoogleTest |
| Debug build | `./gradlew :app:assembleDebug`; the debug APK is attached to the run (skipped when called by Development, which builds the same APK itself) |

Pull request runs have read-only access and no secrets, so code from a pull request (including one from someone else's fork) cannot sign or publish anything.

## Stage 1: Development

Runs automatically after every push to `develop`, which is what merging a pull request does. You can also start it by hand (**Actions → Development → Run workflow** on `develop`); it then builds the current tip of `develop`.

1. Runs the CI checks (release scripts, unit tests, native tests) on exactly the pushed commit.
2. Builds the Debug APK from that commit (`:app:assembleDebug`) with version `X.Y.Z-dev.<run>+<commit>`.
3. Signs it with the [RCBBoard debug key](#debug-signing-key) and verifies it: v1/v2 signatures, the debug key's certificate, app ID `com.ranger.myheliboard.debug`, the expected version, minSdk 21 and all four ABIs.
4. Uploads `RCBBoard-dev-X.Y.Z-dev.<run>-<commit>.apk` (kept 14 days).

**Getting the Development APK:** open the run (**Actions → Development**) and download the APK from the run summary or from the *Artifacts* section at the bottom. It downloads as an `.apk` file (not a zip); you must be signed in to GitHub. The summary also shows the full commit SHA for the next stage.

## Stage 2: QA

Started by hand when you want to test a commit on your phone:

1. Copy the full commit SHA from the summary of a successful Development run.
2. **Actions → QA → Run workflow**, branch `develop`, paste the SHA (40 characters, lower case, no spaces).

The QA workflow then:

1. **Checks the commit:** it must be a full 40-character SHA, it must be on `develop`, and it must have a **successful Development run** (which already ran all CI checks on exactly this commit). Otherwise the run stops and nothing is built.
2. Checks out exactly that commit and confirms it.
3. Builds the Debug APK (`:app:assembleDebug`) with version `X.Y.Z-qa.<run>+<commit>`, where `X.Y.Z` is the version a production release of this commit would get.
4. Signs it with the same [RCBBoard debug key](#debug-signing-key), verifies it and uploads `RCBBoard-qa-X.Y.Z-qa.<run>-<full commit SHA>.apk` (kept 14 days).

The run is named **"QA &lt;commit&gt;"**. Production uses this name to find it, so it cannot be renamed.

**Installing and testing the QA APK:** download it from the run summary on your phone and open it. It is the **RCBBoard debug** app, the same app as the Development APK, and it installs next to the production RCBBoard app. Enable it in *Settings → System → Keyboard → On-screen keyboard*, switch to it and test.

Every Development and QA APK is signed with the same RCBBoard debug key and has a higher versionCode than earlier builds, so a new QA or Development APK **installs as an update** over the installed debug app and keeps its settings and learned words. Only a debug app signed with a different key (for example one built locally in Android Studio, or a build from before the debug key was set up) has to be uninstalled once first.

## Stage 3: Production

Started by hand after QA, never by a merge:

1. Copy the full commit SHA from the summary of the QA run you tested.
2. **Actions → Release → Run workflow**, branch `develop`, paste the SHA. Leave `bump` on `auto`.
3. The run waits for your approval (see [Production approval](#production-approval)). Approve it to build and publish.

The release workflow:

1. **Checks the commit:** full SHA, on `develop`, **a successful QA run for exactly this SHA**, and **not older than the latest release** (releasing older code is refused).
2. **Determines the version** of exactly that commit (see [Versioning](#versioning)). If only documentation, CI or tests changed since the last release, nothing is published. If the newest tag has no published release, it stops with recovery steps (see [Recovering a failed release](#recovering-a-failed-release)).
3. **Waits for approval** in the `release` environment.
4. **Builds** the universal release APK from exactly that commit (`:app:assembleRelease`, all four ABIs, no Gradle cache).
5. **Signs** it with the existing production key from the `release` environment secrets and **verifies** it: v1 and v2 signatures, exactly one signer whose certificate SHA-256 is `0b01373116d4a7b57646c4c7e2ffc28af91870bdbeabb4b8c05c00f313e2291b`, package `com.ranger.myheliboard`, version, minSdk 21, all four ABIs.
6. **Tags** exactly that commit as `rcbboard-vX.Y.Z`.
7. **Publishes** a GitHub Release with `RCBBoard-X.Y.Z.apk` and `SHA256SUMS.txt`. The notes list the version, commit, QA run, build date, installation steps and the certificate fingerprint (no commit messages). It is marked *latest* only if no newer release is published.

Safety:

- Only one release run at a time; a queued run waits for the running one.
- Existing tags are never moved or overwritten, and an existing release is never replaced.
- The GitHub CLI uploads to a draft and only publishes it when all uploads succeeded.
- Tags and releases created with the workflow token do not start other workflows.
- If a production secret is missing, the run stops before building. An unsigned build is never published.

## The same commit in every stage

Every stage builds a commit SHA, never "whatever `develop` is now":

| Stage | Which commit | Check |
| --- | --- | --- |
| Development | the pushed commit (`github.sha`) | – |
| QA | the SHA you enter | on `develop` + successful Development run for that SHA |
| Production | the SHA you enter | on `develop` + successful QA run for that SHA + not older than the latest release |

QA and Production check out exactly that SHA and confirm it (`git rev-parse HEAD`) before building; the tag is created on that SHA. If `develop` moves on after QA, Production still builds and tags the QA-approved SHA. If you enter a newer, untested SHA, Production stops with *"there is no successful QA run for …"*. Short SHAs, branch names and other spellings are rejected.

## Debug signing key

Development and QA sign their Debug APKs with one persistent **RCBBoard debug key**, stored as GitHub Actions secrets. It is never the production key, and it is never created by a workflow. Because every run uses the same certificate, each new Development or QA build can update the installed debug app. If the secrets or the variable are missing, Development and QA stop with *"Debug signing not configured"* before building.

**1. Create the key once** on your computer, outside the repository (keep it next to, but separate from, the production keystore). keytool asks for a password; with the default PKCS12 format the key password is the same as the keystore password.

```sh
keytool -genkeypair -v -keystore ~/Documents/keys/rcbboard-debug.keystore \
  -alias rcbboard-debug -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=RCBBoard Debug"
```

**2. Get its certificate SHA-256** (plain lower-case hex, as `apksigner` prints it):

```sh
keytool -exportcert -keystore ~/Documents/keys/rcbboard-debug.keystore -alias rcbboard-debug | shasum -a 256
```

**3. Add it to GitHub** (Settings → Secrets and variables → Actions):

| Tab | Name | Value |
| --- | --- | --- |
| Secrets | `DEBUG_KEYSTORE_BASE64` | the keystore, base64-encoded: `base64 -i ~/Documents/keys/rcbboard-debug.keystore \| pbcopy`, then paste |
| Secrets | `DEBUG_KEYSTORE_PASSWORD` | the keystore password |
| Secrets | `DEBUG_KEY_ALIAS` | `rcbboard-debug` |
| Secrets | `DEBUG_KEY_PASSWORD` | the key password (with PKCS12: the same as the keystore password) |
| Variables | `DEBUG_CERT_SHA256` | the certificate SHA-256 from step 2 (not secret) |

These are repository secrets: pull request runs from forks never receive them, and only `development.yml` and `qa.yml` use them. The workflows decode the keystore into the runner's temporary folder, sign with `apksigner` (passwords passed through environment variables, never printed), check that the signer is exactly `DEBUG_CERT_SHA256`, and delete the keystore at the end. Never commit the keystore or its passwords.

## Production approval

The `release` environment holds the production signing secrets. With **yourself as required reviewer**, every release run stops before the build job and shows **Review deployments**; nothing is built, signed, tagged or published until you approve. Approve only after testing the QA APK of the same commit. GitHub records who approved and when.

Setup: **Settings → Environments → release → Required reviewers**: add yourself. Leave *Prevent self-review* unchecked (you are the only reviewer). *Deployment branches and tags*: `develop` only.

## Recovering a failed release

If a release run fails after it has created the tag `rcbboard-vX.Y.Z` (for example while uploading), that tag has no published release. The next release run then stops with *"The newest release tag rcbboard-vX.Y.Z … has no published GitHub Release"*, so nothing is built on top of a version that was never published. Fix it in one of two ways:

- **Publish it:** open **Actions → Release**, find the failed run and choose **Re-run all jobs**. It builds the same version from the same commit and creates the missing release.
- **Drop it:** if that build must not be published, delete any draft release for the tag (Releases page), delete the tag with `git push origin --delete rcbboard-vX.Y.Z`, and start the release again. The version number is then used for the next release.

A re-run of an older release is never marked as the *latest* release if a newer `rcbboard-v*` release has been published in the meantime. If a run was cancelled during the upload, a draft release can remain; delete it on the Releases page.

## Versioning

Versions are `MAJOR.MINOR.PATCH`, tagged `rcbboard-vMAJOR.MINOR.PATCH`. The prefix keeps RCBBoard's tags apart from HeliBoard's own `vX.Y` tags.

The bump is taken from [Conventional Commits](https://www.conventionalcommits.org/) in all commits between the previous release and the released commit (merge commits themselves are skipped, the commits they bring in are counted). The largest bump wins:

| Commit | Bump | Example |
| --- | --- | --- |
| `type!:` or a `BREAKING CHANGE:` footer | MAJOR | `refactor!: change the backup format` |
| `feat:` | MINOR | `feat(layout): add a Tamil layout` |
| anything else that changes the app (`fix`, `perf`, `refactor`, `build`, `chore`, upstream merges, messages without a type) | PATCH | `fix: crash when opening settings` |
| commits that only change documentation, CI or tests | no release | `docs: update README` |

"Only documentation, CI or tests" means every changed file is under `.github/`, `docs/`, `art/`, `fastlane/`, `app/src/test/`, `app/src/androidTest/` or `app/src/main/jni/tests/`, or is a Markdown file or `.gitignore`/`.gitattributes`/`.editorconfig` in the repository root. Everything else counts as an app change.

With squash merges, the pull request title becomes the commit message, so give pull requests Conventional Commit titles. To force a release or a larger bump, choose `patch`, `minor` or `major` for `bump` when starting the release.

**versionCode** = MAJOR × 10000 + MINOR × 100 + PATCH, for example 4.4.0 → 40400. MINOR and PATCH must stay below 100. Development and QA builds use the minutes since 1970 instead, so a later debug build always has a higher versionCode; the debug app is a separate app, so this never affects the production app.

`app/build.gradle.kts` keeps `4300` / `4.3-beta1` as defaults for local builds; the workflows pass `-PrcbboardVersionCode` and `-PrcbboardVersionName`.

## One-time setup on GitHub

These settings are made by the repository owner on github.com; the workflows do not change them.

1. **Actions** enabled for the repository (the **Actions** tab).
2. **Actions permissions** (Settings → Actions → General): allow GitHub-made actions plus `gradle/actions` and `android-actions/setup-android`; require approval for workflows from external contributors' forks; *Workflow permissions*: **Read repository contents and packages permissions**, and leave *Allow GitHub Actions to create and approve pull requests* unchecked.
3. **Environment `release`** with the production secrets `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD` (the existing release keystore; never replace it), deployment branch `develop`, and **yourself as required reviewer** (see [Production approval](#production-approval)).
4. **Debug signing key** as repository secrets and variable (see [Debug signing key](#debug-signing-key)). Configure it before merging the pipeline, otherwise the first Development run stops.
5. **Branch protection** for `develop` (Settings → Branches), requiring the checks `Release script tests`, `Unit tests`, `Native tests (ASan/UBSan)` and `Debug build`.

## Download and install a production release

1. Open [Releases](https://github.com/Rananjaya/RCBBoard/releases) and download `RCBBoard-X.Y.Z.apk` on the Android device.
2. Open the file and allow installing from that source if asked. It updates an existing RCBBoard installation and keeps settings and learned words.
3. Enable RCBBoard in *Settings → System → Keyboard → On-screen keyboard*.

To check a download: `apksigner verify --print-certs RCBBoard-X.Y.Z.apk` must show the production certificate SHA-256 above, and `shasum -a 256 -c SHA256SUMS.txt` checks the file.

## Troubleshooting

| Problem | What to do |
| --- | --- |
| No workflow runs at all | Actions are not enabled (setup step 1). |
| QA: "… is not a full 40-character lower-case commit SHA" | Paste the full SHA from the Development run summary, without spaces. |
| QA: "… is not on develop" | Only commits on `develop` can be tested; merge the pull request first. |
| QA: "there is no successful Development run for …" | Wait for (or re-run) the Development run of that commit. |
| Release: "there is no successful QA run for …" | Run QA with exactly this SHA, test the APK, then start the release again. |
| Release: "… is older than the latest release" / "does not contain the latest release" | Only commits after the latest release can be released. |
| Release waits with "Review deployments" | That is the approval step: approve it after testing the QA APK. |
| Development/QA: "Debug signing not configured" | Add the missing debug signing secrets or the `DEBUG_CERT_SHA256` variable (see [Debug signing key](#debug-signing-key)), then re-run. |
| Development/QA: "DEBUG_KEYSTORE_BASE64 is not valid base64" or an `apksigner sign` error | Encode the keystore again without line breaks; check alias and passwords. |
| Development/QA: "APK signing certificate is …, expected …" | `DEBUG_CERT_SHA256` does not match the debug keystore in the secrets; recompute it (step 2). |
| The phone says *"App not installed"* for a Development/QA APK | The installed debug app was signed with another key (for example built in Android Studio): uninstall it once. |
| Release: "Signing secrets missing" | Add the named secrets to the `release` environment, then re-run the failed run. |
| Release: "APK signing certificate is …, expected …" | The `release` secrets contain a different key. Use the existing release keystore. Nothing was published. |
| "… has no published GitHub Release" | An earlier release failed after creating its tag; see [Recovering a failed release](#recovering-a-failed-release). |
| "tag … already exists on another commit" / "Tag exists" | A tag with that version points to a different commit. Do not move it; find out where it came from. |
| "the clone is shallow" / "fetch-depth" / "fetch-tags" | The jobs need the full history and tags; do not change `fetch-depth: 0` / `fetch-tags: true`. |
| Unit tests fail | Download `unit-test-reports` from the run. Test locally with JDK 21 or newer, with the same command as CI (see [Testing locally](#testing-locally)). |
| Native tests fail | Run `app/src/main/jni/tests/host/run_host_tests.sh` locally (needs clang++, GoogleTest and a JDK). |
| Build fails with lint errors | Download the build reports from the run; `./gradlew :app:lintVitalRelease` shows the same errors locally. |

## Testing locally

```sh
python3 -m unittest discover -s .github/scripts -p 'test_*.py'   # release and stage scripts
python3 .github/scripts/next_version.py --head origin/develop       # what a release of develop would be
./gradlew --init-script .github/ci-unit-tests.init.gradle :app:testRunTestsUnitTest \
    -Pandroid.onlyEnableUnitTestForTheTestedBuildType=false          # unit tests as in CI (JDK 21+)
app/src/main/jni/tests/host/run_host_tests.sh                       # native tests
./gradlew :app:assembleDebug                                        # Debug APK (your local debug key; CI re-signs with the RCBBoard debug key)
./gradlew :app:assembleRelease -PrcbboardVersionCode=40400 -PrcbboardVersionName=4.4.0   # unsigned release APK
```
