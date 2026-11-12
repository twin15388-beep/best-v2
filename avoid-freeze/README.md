# AvoidFreeze — standalone, observe-only implementation (work in progress)

This directory is a **new standalone implementation**. It does not include or build the imported `tools/malinka-helper` preview. The latter remains a reference snapshot only.

## Current milestone

- A separate Windows x64 console program is built in native MSVC CI against one pinned public BestClient source/release profile.
- Its first live mode is **observe-only advice**: it opens the target with query/read/synchronize rights, rejects unknown builds and unsupported states, runs the pinned public single-character movement core against an owned copy of a narrow static map, and reports a suggested direction.
- This milestone does **not** send keyboard input. There is no `SendInput`, keyboard hook, DLL injection, process-memory write, client-file patch, or replacement path in the target.
- The physics policy is deliberately narrow: one active predicted character; no other predicted characters/entities; no dynamic collision layers, ego-tile data, unsupported map tiles, active hook/jump/weapon input, ninja/special movement modes, or unsupported prediction settings. Unsupported data must produce `BLOCKED`.
- A direction suggestion is not a guarantee that the server will agree. The program does not reproduce full client/server parity, and no user map, user process, gameplay outcome, or anti-cheat compatibility has been tested.

Do not use it on a public server. Updated native MSVC x64 build, Windows policy tests, `--self-test-layout` and the read-only/no-input audit passed in [Actions run 43294756391](https://github.com/twin15388-beep/best-v2/actions/runs/43294756391). The self-test checks every measured member offset consumed by the reader plus the nested world-config and input layouts in that compiled target. The root scan now retries `ERROR_PARTIAL_COPY` as page-bounded reads; it still requires every byte and stops if any retry is incomplete. A first manual check of the prior build stopped at the root scan with Win32 299 and produced no `RESULT`; the updated retry has not yet been tested on a live process. This artifact includes `SHA256SUMS.txt`; verify the EXE against it before any optional local/private check. CI does not prove runtime capture or gameplay outcomes.

## Pinned source and profile scope

Physics headers and source are verified against public BestClient commit `cf192e3fb8590dbe2e4f66a191aa6616beff6975` by [`pinned-source-sha256.json`](pinned-source-sha256.json): 1126 tracked files / 16,150,950 bytes. The Windows target additionally requires the recursive `ddnet-libs` submodule at `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`.

The live reader accepts only the exact public EXE SHA-256 `50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f`. It checks the loaded AMD64 PE headers, an audited RTTI/vtable fingerprint, a unique bounded client-root candidate, and compiler-derived MSVC x64 member offsets. Those layout numbers are object-relative byte offsets (not RVAs); they are copied into [`src/platform/measured_layout.h`](src/platform/measured_layout.h) with provenance. Prior controlled observe-only work exercised only a subset of fields on the same public image; it did not validate this new reader, a user's process, or gameplay. Every new read path is still subject to local runtime checks and fail-closed gates.

Use a separate public source checkout, never the installed game directory:

```powershell
git clone https://github.com/BestProjectTeam/BestClient.git C:/src/BestClient-public
git -C C:/src/BestClient-public checkout --detach cf192e3fb8590dbe2e4f66a191aa6616beff6975
git -C C:/src/BestClient-public submodule update --init --recursive
```

## Build

Required: Visual Studio 2022 x64 tools, Windows 10/11 SDK, CMake 3.24+, Python 3.10+, and Git.

```powershell
cmake -S avoid-freeze -B build/avoid-freeze -A x64 `
  -DAF_PINNED_SOURCE="C:/src/BestClient-public" `
  -DBUILD_TESTING=ON
cmake --build build/avoid-freeze --config Release --target AvoidFreeze avoid-freeze-tests
ctest --test-dir build/avoid-freeze -C Release --output-on-failure
build/avoid-freeze/Release/AvoidFreeze.exe --self-test-layout
```

The native process reader is only enabled for MSVC x64. Linux builds the policy/tests against the same pinned public physics source; they do not compile the Windows reader.

## Future manual check (not requested yet)

After the native build and layout self-test pass, the only intended first user check is observe-only on a local/private test map. It will require the user to identify the unchanged official `DDNet.exe` PID in Task Manager and run `OBSERVE-ONLY.cmd` from the test bundle. No PC agent is required. Do not pass any input-enabling option; none exists in this milestone. If Windows blocks the unsigned test binary, stop—do not disable security controls.

The final report is advisory telemetry only. It must not be described as an Avoid success unless an independent in-game outcome was actually observed. See [`TEST-READ-ME.txt`](TEST-READ-ME.txt) for the exact future procedure.
