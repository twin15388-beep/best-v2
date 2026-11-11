# MalinkaAvoidPreview — draft, not a release

`MalinkaAvoidPreview` is an unfinished, independent-process preview for one narrowly supported static-map situation. It is **not** the completed external Avoid Freeze requested for the official client, has not been built with native Windows/MSVC in this workspace, and has **not** been verified in-game. There is no preview `.exe` or delivery package.

The existing `MalinkaHelper` diagnostic app remains a separate product. The optional preview target is **OFF by default** (`MALINKA_BUILD_AVOID_PREVIEW=OFF`). Neither app changes the installed game files. The preview reads the target process with query/read/synchronize rights only; its only input mechanism is ordinary Windows `SendInput`, and that path is gated behind an explicit `--enable-input` launch option and F8 arming. Do not enable input until the native build and the live/gameplay gates below have been completed.

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

CMake verifies the pinned source manifest, generates required protocol headers into the build tree, and refuses a non-MSVC compiler or a missing/mismatched submodule. The documented compiler-derived class layout is **not runtime validated**; a matching executable hash alone does not prove the official binary used that build configuration. Never point `MALINKA_PINNED_SOURCE` at the installed client and never patch or replace client files.

## Current verification status

The latest Linux sanitizer fixture run reports **12,797 policy checks, 361 consecutive trace transitions, 0 mismatches**, with a synthetic safe direction of `-1`, baseline hazard tick `36`, and `no_game_input=true` (logs `avoid-fixture-build-6.log` and `avoid-fixture-run-6.log` in the private workspace). It exercises the policy against the saved runtime-3 trace only. It does **not** compile the Windows reader/preview, read a live game process, exercise `SendInput`, or prove an in-game Avoid.

After a native build, `MalinkaAvoidPreview.exe --self-test-abi` runs the pinned MSVC x64 candidate-layout and member-offset assertions without opening a process or sending input. Passing it would validate that compiler-built candidate layout only; it would **not** validate the layout of the official executable. Local Zig/Windows-GNU object checks with temporary thread shims and MSVC-only ABI assertions removed were syntax checks only. They are not a Windows build. **Native MSVC compilation, the candidate self-check, live on-map capture, controlled in-game outcomes, and the external badge remain unverified.**
