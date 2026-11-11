# MalinkaAvoidPreview — draft, not a release

`MalinkaAvoidPreview` is an unfinished, independent-process preview for one narrowly supported static-map situation. It is **not** the completed external Avoid Freeze requested for the official client. Its optional Windows x64 target has been built and candidate-ABI checked in native CI, and its C++ `LiveReader` has passed an observe-only probe against the exact public release on an owned local freeze map. It has **not** been validated against the user's active process or for actual Avoid outcomes. There is no permanent release or installer; successful native MSVC runs upload a 30-day experimental test artifact containing the executable and observe-only-first instructions.

## Downloading the test build

The latest successful run and its `MalinkaAvoidPreview-experimental-win64` artifact are listed on the [native MSVC workflow page](https://github.com/twin15388-beep/best-v2/actions/workflows/malinka-avoid-preview-native.yml). The artifact includes the standalone `.exe`, an observe-only launcher, test notes, license files, and an executable SHA-256. It expires after 30 days; it is **not** a release or a finished Avoid. Start with `OBSERVE-ONLY.cmd` and do not add `--enable-input` for the first test.

The existing `MalinkaHelper` diagnostic app remains a separate product. The optional preview target is **OFF by default** (`MALINKA_BUILD_AVOID_PREVIEW=OFF`). Neither app changes the installed game files. The preview reads the target process with query/read/synchronize rights only; its only input mechanism is ordinary Windows `SendInput`, and that path is gated behind an explicit `--enable-input` launch option and F8 arming. The controlled probe below does not clear the input gate: first validate the full target layout and the intended user process/session/map in observe-only mode, then compare predictions with actual results in same-map no-input trials. Any later input trial requires separate explicit authorization and its own focus, key-conflict, watchdog, and release checks.

## Deliberate fail-closed limits

The live reader currently requires all of the following:

- The exact public BestClient 3.1.1 executable profile already recognized by the diagnostic helper, a complete scan with exactly one root candidate, and a still-live process with the same creation time.
- An online session with exactly one snapshot player and one predicted local character; predicted-world, client-copy, dummy/local IDs, tick, entity/core backlinks, and effective tuning must agree. Memory is collected twice, but this is a coherence heuristic, **not an atomic snapshot**.
- Only the static game/front tile arrays and whitelisted tile states. Other predicted characters/entities, ego-tile prediction data, teleporter/speedup/switch/tune/door/extra-door layers, tune-zone overrides, unsupported tiles, weapon states, or inputs are refused.
- The configured A and D keys must be single-command `+left` / `+right` binds. Arrow keys are considered only when their exact binds are confirmed. UI/chat/modifier/focus loss, stale ticks, prediction mismatch, or inability to release an owned key disarms and releases input.

The policy uses the pinned public physics sources and a bounded 40-tick prediction horizon. It is not full KRX/gameplay parity, does not support multiplayer/dynamic map mechanics, and does not reproduce or modify the official badge. These constraints are safety gates, not promises that every accepted state is correct.

## Pinned build inputs

The prediction dependency is the public source commit `cf192e3fb8590dbe2e4f66a191aa6616beff6975`. The Windows preview requires the exact `ddnet-libs` submodule commit `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`, including the pinned SDL and other public headers. Use a source checkout outside the game directory and initialize submodules:

```sh
git clone https://github.com/BestProjectTeam/BestClient.git <public-source-dir>
git -C <public-source-dir> checkout --detach cf192e3fb8590dbe2e4f66a191aa6616beff6975
git -C <public-source-dir> submodule update --init --recursive
```

Build on **Windows x64 with the measured MSVC ABI**:

```powershell
cmake -S tools/malinka-helper -B build/avoid-preview -A x64 `
  -DMALINKA_BUILD_AVOID_PREVIEW=ON `
  -DMALINKA_PINNED_SOURCE="C:/path/to/public-source-dir"
cmake --build build/avoid-preview --config Release --target MalinkaAvoidPreview
```

CMake verifies the pinned source manifest, generates required protocol headers into the build tree, and refuses a non-MSVC compiler or a missing/mismatched submodule. A controlled native live probe has checked only the fields consumed by `LiveReader` against the exact public release image; the full compiler-derived class layout remains **not runtime validated**, and the user's active process is untested. Never point `MALINKA_PINNED_SOURCE` at the installed client and never patch or replace client files.

## Current verification status

The latest Linux sanitizer fixture run reports **12,797 policy checks, 361 consecutive trace transitions, 0 mismatches**, with a synthetic safe direction of `-1`, baseline hazard tick `36`, and `no_game_input=true` (logs `avoid-fixture-build-6.log` and `avoid-fixture-run-6.log` in the private workspace). It exercises the policy against the saved runtime-3 trace only. It does **not** compile the Windows reader/preview, read a live game process, exercise `SendInput`, or prove an in-game Avoid.

Native workflow **43032866534/job130564101587** successfully built the optional x64 target with MSVC on `windows-2022` and passed `MalinkaAvoidPreview.exe --self-test-abi`. A second no-input run, **43034353037/job130569175285**, launched the exact public BestClient 3.1.1 release image (SHA-256 `50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f`) on an owned local freeze map and exercised the C++ `LiveReader`/policy in observe-only mode: **250 valid frames, 249 matched transitions, 0 mismatches, 5 unavailable samples, 247 decision evaluations; 0 input events/corrections, no held companion keys, no process writes or DLL injection**. The full probe and Python side checks are reported in the workflow summary. This validates only the exercised fields for that exact public image in this controlled session; collection is still heuristic, not atomic, and it does not validate the user's active process, user-specific maps/binds, or actual Avoid outcomes. No `--enable-input` was supplied in that probe. The native MSVC workflow now uploads a short-lived experimental test artifact; it is not a full Avoid release. **Controlled same-map no-input outcome comparisons and separately authorized input/outcome testing remain unverified.**
