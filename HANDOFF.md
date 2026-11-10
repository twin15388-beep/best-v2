# Передача задачи: BestClient + malinka

Обновлено: **2026-11-10**, UTC. Checkout `/home/user/best-v2`, рабочая ветка **`arena/733fc6a9-best-v2`**. Исходный commit `244d7d487a48a482d1ecdd77d7190fe5bd5e35ca` содержал только README. С тех пор импортирована публичная основа и написана существенная интеграция. Старое investigation-only описание заменено этим фактическим статусом.

## Последнее продолжение (приоритетнее старых результатов ниже)

### Реально прошедшие builds

- Push только в **`arena/733fc6a9-best-v2`**. Commit **`bcef908ceaa52809ce56c13f874b2911813e96e7`**: **GitHub run `42873249161` завершился SUCCESS: Windows2022/MSVC x64 и Ubuntu24.04**, оба full client+updater+portable stage+checked ZIP+exact `git archive` source, normal Rust1.92.0/Cargo.lock. URL `https://github.com/twin15388-beep/best-v2/actions/runs/42873249161`.
- Artifacts этого run: `malinka-windows-x86_64` id **13790369551**,117776339bytes; `malinka-linux-x86_64` id **13789729743**,116111793bytes. 14-day Actions artifacts, **не published releases**. Это доказывает сборку/упаковку, не Win11 gameplay/renderer/remote badge.
- Предыдущие MSVC failures реально исправлены: C++20 `shared_ptr::unique`→`use_count()>1`; Win32 `IGNORE` macro leak→Win32 handles/reparse/write/rename изолированы в **file_safety.cpp**, убраны windows.h из shared replay/package headers. Build12 native **13/13 exit0**, cross updater7 **exit0**, actual Windows cross-object include-order check2 **exit0**, затем настоящий MSVC success выше. Не править чужой touch-menu enum ради Win32 macro.
- Native build11 **7/7 exit0**, physics rerun8 **8357checks/0failures/exit0**,362allocation injections. Native build13 после нового TAS fix ниже **168/168 exit0**, `.artifacts/build-native-13.*`, `game-client bestclient-updater package_zip`. **TAS fix ещё не включён в прошедший bcef908 CI**; нужен следующий commit/push + run.
- Root Cargo.lock SHA256 `760288176362ad39458b4f01673df5ad8729809cd1e0178da0573ef728c6a904` не изменён. Sandbox wrapper только временно подменяет offline lock, normal CI использует настоящий lock.

### TAS regression: дефект воспроизведён и исправлен

- `CMalinkaTas::ValidateReplay` имел dangling-else: без initial world получался пустой validation world; с initial world успешный copy ошибочно переходил в live branch. **Реальный** temporary lifecycle harness с CGameClient/CClient/CMap/CGameWorld, actual load/validate/playback methods, два настоящих maps (не fake physics). Baseline run1 **8417checks/12failures/exit1** плюс harness-only logger lifetime leak (engine OnInit не вызывался).
- Исправлено: явные braces, transactional publish snapshot, current-world capture для загруженного replay; allocation/filesystem boundaries для public menu/update/network entries; cleanup/release до optional notification; `SimulationFailed`/warning noexcept; NaN не проходит position comparisons; map/participant identity change отменяет validation/playback. Source **tas.cpp/tas.h**.
- После fix: ASan+UBSan **run3 `8961checks/0failures/exit0`**, `.artifacts/tas-lifecycle-{compile,run}-3.*`. Проверены retained initial world против изменённого live position; загруженный без initial replay; 160 selected allocation-failure points; main/dummy input retrieval; neutral/no replay input до validation; offline/focus-loss stop; divergence frame3; NaN state/start; participant disconnect. Renderer/network не запускались, server guarantee не доказана.
- Harness `.artifacts/check_independent_world.cpp` дополнен lifecycle checks через C++ explicit-instantiation member access, **не offsets/layout hacks**. `.artifacts/build_lifecycle_modules.py` инструментирует actual tas.cpp и console.cpp. `.artifacts/harness-console.cpp` содержит копию реального console.cpp с test-only logger cleanup, так как обычная Engine.OnInit ownership registration не запускалась; production console не менялся, LSan final **без ошибок**.
- Exact rerun: `python3 .artifacts/build_lifecycle_modules.py && python3 .artifacts/build_world_checks.py --harness-only --compile-only && python3 .artifacts/build_world_checks.py --link-only`; затем `LD_LIBRARY_PATH="$PWD/.artifacts/toolchain/gl/lib:$PWD/ddnet-libs/sdl/linux/lib64" ASAN_OPTIONS=detect_leaks=1 .artifacts/check_independent_world`. Native dependency objects должны существовать.

### Package/check results

- Generator **50 isolated checks/0failures/exit0** `.artifacts/package-generator-run-1.*`; pure ASan+UBSan logic run5 **9459checks/0failures/exit0** `.artifacts/check-logic-5.*`.
- Real native package1026files из **ed5a779**: Python generator→C++ ZIP Extract/Verify/Install, ASan+UBSan **exit0** `.artifacts/actual-package-{compile,generate,run}-1.*`. Проверены exception rollback after first moved file, actual binary readback, settings/replay preservation, old-file backup, tampered file rejection без изменения install. Package SHA256 `c7181a1fb779eee991161c3f61e9e048a018001c72a524b7a8ddec55e56ce2fd`; source archive SHA256 `59bd12f063c707794629e04092a22eefbdfd8ffe94a78b2fa4542ccf0e4ee563`. Under `build/package-checks/ed5a779/`, не Git. **Не означает запущенный client или deployed update channel.**
- Exception rollback defect был реально найден logic run4 **9454checks/3failures/exit1**, затем исправлен `malinka_package.h`: preallocated paths, catch→rollback, no-throw Move, destination appearance recheck, optional cleanup. Windows updater cross6/7 actual linked exit0; full MSVC updater теперь build-проверен CI.

### Next verifiable steps / blockers

1. Commit/push текущего TAS fix в ту же ветку; дождаться нового Windows/Linux CI. Нельзя приписать bcef908 pipeline последующим изменениям.
2. Продолжить официальный replay sample importer: schema наблюдалась у069/003, но **импортёр пока отсутствует**. Нельзя выдумывать initial world, frame position phase, map SHA, dummy tracks или полную KRX1.32 совместимость. Parser можно проверить на samples; playback только после полной independent physics validation на настоящем current start state.
3. 72 gates и полный KRX parity/vault/RPC всё ещё incomplete. Windows runtime/gameplay, panic navigation/search, внешняя badge authorization и published/end-to-end updates **не выполнены**. Sources повторно не просить.

## Точный запрос и несокращённый scope

Пользователь просит добавить в BestClient вкладку **`malinka` со всеми функциями** присланного **`KRX-Client-18.9.1-win64-ultimate-v1.32`**, позволить обновления без потери функций, не получать `fake` вместо нормального BestClient badge в Tab после модификации, и panic по **Insert**, скрывающий cheat tabs до «перезайти».

**Полный запрос ещё не выполнен.** Нельзя выдавать Pasta adaptation, registry gates, compilation или isolated checks за точный/полный официальный KRX 1.32. Пользователь отверг investigation-only/source-question и поручил искать/делать самостоятельно; не спрашивать исходники повторно. Было объявлено использование публичной Best v3.1.1.

Принята предварительная интерпретация panic: **UI-only, до полного process restart**, не reconnect; функции не выключаются. Внешний badge требует реальной авторизации сервисом и наблюдения из немодифицированного клиента. Локальная подмена картинки/`false` не выполняет требование. Fork updater реализован, но update deployment и end-to-end installation пока не подтверждены.

Каждый видимый ответ начинать точной первой строкой `[Aero]`, обращаться `бро`, русский. Перед долгой работой краткая однострочная цель. Корректность важнее скорости; не выдумывать offsets/ABI/signatures/IDs. После deliverable отдельно: changed files; exact build + exit; isolated checks + result; observed real-build behavior или **not verified in-game**. Context length не повод останавливаться. Все работы только на указанной ветке; не переключать/переименовывать её.

## Основные документы

- `README.md`: честное описание fork и ссылки.
- `docs/MALINKA_STATUS.md`: реализованные consumers, **72** gated controls, отсутствующие возможности без config, measured checks и manual acquisition/regression recipe.
- `docs/MALINKA_SOURCES.md`: pins/hash/происхождение/лицензии/входные EXE и replay schemas. Это adaptation, не официальный KRX source port.
- `docs/MALINKA_BUILD.md`: normal Windows/MSVC/Linux commands, checked package format/limits, CI и отличия sandbox fallback toolchain.
- `docs/MALINKA_DEPENDENCIES.json`: pinned SDK VERSION files, lock/toolchain versions.
- `docs/BESTCLIENT_UPSTREAM.md`: сохранённый upstream README, не claims о проверке fork.

## Происхождение и факты входа

Публичная основа Best v3.1.1: **`cf192e3fb8590dbe2e4f66a191aa6616beff6975`**, DDNet **20.1.1**, TClient **10.9.0**. Донор Pasta: **`dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f`** (экспериментален, не официальный KRX). KRX docs pin **`8c193ca2a27cc7ba5d4e2d6ad7686c6e99910c43`**, v1.32 от 2025-02-07/DDNet18.9.1; позднейшие 1.33–1.36 функции автоматически в scope не входят. SDK `ddnet/ddnet-libs` **`c0e6703fbcdbe03df2f26875427ec3951ce4ec21`**. Полные tarball SHA-256 в SOURCES.

KRX ZIP **80079820**, SHA-256 **`77aec68613b9f0f01dd43ec27f955e21a6546076ef539863025ea6140ace67e9`**, **973 CRC PASS**, C/C++/PDB/project не найдены. EXE **14364176**, SHA-256 **`f9ec1d9f050e4ecd9f426d0ae71abaf5d91972a082636822883d419856858c85`**, PE32+x64; protected `.boot` наблюдается, конкретный protector не установлен; не запускался в Windows. PE preferred ImageBase — не runtime VA.

Best Dropbox ZIP **120621904** превысил connector download limit104857600. Binary/source identity **не установлена**. Не повторять ту же загрузку в надежде другого результата. Access-parameter links в исходном user message, не в Git. Dropbox включён, но новых downloads в последнем продолжении не было.

Официальные replay samples069/003: измерена LE32 length + zlib JSON (`info`/`replay_local`,10 input fields,`pos{x,y}`). Полного initial world/ticks/dummy в этих samples нет. Importer/vault не реализованы; отдельный Malinka format не выдаётся за официальный.

## Что реализовано / entrypoints

### UI / panic / config

`src/game/client/components/malinka/` зарегистрирован в `gameclient.{h,cpp}`. Settings/navigation/search/dispatch обновлены в `menus*`, `bestclient/ui_theme/settings_nav.cpp`. `panic.h` содержит process-lifetime latch; `gameclient.cpp` перехватывает Insert press/release до binds; hidden state не сбрасывается на reconnect/reset. Menus helpers через MenuStorage/MenuUi/MalinkaTooltip bridge в `menus.h`.

`capabilities.h` явно блокирует **72** controls без/с неполной реализацией; `misc.cpp` команда **`malinka_status`** перечисляет ограничения. Это промежуточная мера, не сокращение запроса. Не все missing features имеют config.

### Inputs / visuals / bots

`controls.cpp` в malinka + основной `components/controls.cpp`, aimbot/avoid/autoedge/autoaled/laserunfreeze: реальные consumers, bounded scans, raw key release и input streams/domain handling, finite aim helpers. Часть advanced features отсутствует. `avoidfreeze.cpp` теперь возвращает **UNAVAILABLE/SAFE/DANGER**, чтобы capture/alloc failure не считался safe movement.

`players.cpp`, `tclient/trails.cpp`, malinka visuals: local custom colors, tile outlines, aim/trajectory/self-hit ESP, features HUD/watermark/notifications. Новые реальные consumers:

- `items.cpp:CItems::RenderProjectile` → `CMalinkaVisuals::RenderBulletLine`:64segments/last2seconds, actual tune-zone curvature/speed/time, owner validation для self-only; не только HUD label.
- `CMalinkaVisuals::RenderFrozenHud`:skins/name/ID/freeze-time/team/rows/size/last-unfrozen teammate; unknown extended data не считают unfrozen, last-teammate только known normal team. Menu controls добавлены, соответствующие14gates сняты.

Эти renderers **не наблюдались в игре**.

### Private worlds / TAS / Fent / benchmark

`simulation_step.h` — общий checked tick: validate participants/count/tick limit → direct inputs sorted by client ID → increment world tick → predicted inputs → full world tick. Reacquire entities после мутаций. False означает private simulation надо остановить/отбросить. Используется TAS/Fent/Avoid/benchmark/trajectory.

`prediction/entity.*`:detached copy ctor/assignment (world/list/parent links не копируются), initialization layer/number. `gameworld.*`:owned config/tunings/mapbugs/PRNG/collision, unique_ptr partial clones, reserve recycle bins до unlink, exception-safe independent copy, ReleaseSimulationDependencies при Init/ordinary copies. `collision.*`:COW copied layers/tele/doors, independent callbacks/config, allocation failure→Unload/false, bounded door lookup. `alloc.h`:null malloc→bad_alloc.

TAS:private recording world, main/dummy tracks,50TPS, sliced seek/validation, events/sound, no sending recording actions, exact map/physics metadata, playback stream domain separation, safe read/write/backups/resources. Checked ApplyFrame callers do not accept failed frames.

Fent:unique_ptr worlds, common tick order (removed duplicate direct input and wrong tick increment), immutable captured dummy input shared by planning/execution, dual-track export (pre-input position conversion), slot/participant identity checks, exception boundary. **SMetadata final bool is Legacy, not HasDummy: keep `false` for new exports.** Dummy flag lives on each frame. Fent gameplay/exports not observed in client.

Benchmark:real world ticks in bounded jthread workers, memory/deadline≤15s/cancel/join/exception handling. Command itself not executed; physics harness exercises lower-level real snapshots separately. TSan not run.

### Checked updater / packaging / CI

`build_identity.h`:version **0.1.0**, own repo `twin15388-beep/best-v2`,tags`malinka-vX.Y.Z`,asset`bestclient-malinka-{platform}.zip`. Default official code remains empty. `bestclient/updater.cpp` only accepts own compatible release URL/asset + GitHub `sha256:` digest, stock channel replacement blocked.

`src/tools/malinka_package{,_zip}.h`:bounds512MiB/file/archive,2GiBexpanded,10000entries; strict paths/aliases/CRC/type/hash/platform/marker; all files staged+read back, journal+backup before moves, rollback on failure, preserve config/replay/user paths. SHA/markers are **not signatures**; filesystem TOCTOU and directory/journal fsync/power-loss guarantees incomplete.

Windows `bestclient_updater.cpp`:restored missing WorkerArgs;strict bounded decimal PID;wstring paths;atomic HWND;argv/worker RAII;wait/join before deleting critical section;worker/entry exception boundaries. Linux worker has exception boundary, strict from_chars PID and join before mutex destroy.

`scripts/malinka_bootstrap.py`:stdlib checked pinned SDK download/extraction into **new** dir; refuses existing dir; no project dependencies on temporary Zig/npm tools.

`scripts/malinka_package.py`:flat manifest ZIP generator, markers/notices/path/resource validation, per-file hash+write-time readback, deterministic ZIP timestamp, no output overwrites + sidecar. **Actual Python→C++ native package round-trip passed** for ed5a779; see latest continuation above.

`.github/workflows/malinka-build.yml`:pinned actions, Windows2022/MSVC and Ubuntu24.04 full build/package artifacts, Rust1.92.0 stable, normal `Cargo --locked`, SDK hash verification. Does **not** publish releases or read official secrets. **Run42873249161 passed Windows/MSVC and Linux build/package** for bcef908; subsequent changes require a fresh CI run. This is not runtime/gameplay proof.

CMake includes new headers/updater zlib+SHA sources and packages `LICENSE`/`license.txt`/docs. Normal Linux system dependency package and sandbox fallback package differ.

## Последние измеренные builds/checks

### Native Release

Tools GCC12.2/Python3.11/CMake3.27.9/Ninja1.11.1.4/Meson1.5.2; rust1.92nightly;SDL2.32.10/GLVND1.7.0.

```sh
export PATH="$PWD/.venv/bin:$PWD/.artifacts/toolchain/rust/bin:$PATH"
export CARGO_HOME="$PWD/.artifacts/cargo" CARGO_NET_OFFLINE=true
cmake --build build/native --target game-client bestclient-updater --parallel 2 -- -k 0
# build7:179/179,exit0,both linked
cmake --build build/native --target game-client bestclient-updater package_zip --parallel 2 -- -k 0
# build8:6/6,exit0,latest Fent metadata/notice/Cargo restore fixes + portable stage
```

Logs `.artifacts/build-native-{7,8}.{log,exit}`. Processes build7 `client-and-updater-build-f5e2df84`,build8 `release-build-and-package-stagin-15f5be02` (PID17153), build8 confirmed tool exit0. Binaries `build/native/DDNet`, `bestclient-updater`; stage `build/native/pack_*_zip/<root>`. Build8 actual C++ link and package generation succeeded; warnings include inherited unused Fent helpers and unrecognized `-Wno-nullability-completeness`, not zero-warning build.

Configure options:Release`-O1 -DNDEBUG`,bundledON;VIDEORECORDER/VULKAN/DISCORD/STEAM/TOOLS/SERVER/DOWNLOAD_GTEST/PRECOMPILE_HEADERS OFF;compile_commandsON;CXXflags`-DGLEW_EGL -DGLEW_NO_GLU -I<repo>/.artifacts/toolchain/gl/include`;RUST_CARGO`.artifacts/native-bin/cargo`.

**Cargo.lock restored** from upstream; SHA-256 **`760288176362ad39458b4f01673df5ad8729809cd1e0178da0573ef728c6a904`**. Sandbox wrapper uses fcntl guard/temporary path-patched lock and restores original content+mtime after Cargo. It strips locked/build-std only locally. Rust GitHub fallbacks not checked against crates.io archive checksums; ordinary CI must use upstream lock, no patches/wrapper. Toolchains/deps excluded, bootstrap recipe in BUILD.

### Syntax / isolated logic

`python3 .artifacts/check_syntax.py` simulation-1: **30PASS/0FAIL/exit0**; `.artifacts/syntax-simulation-1.{log,exit}`. Predates unique_ptr Fent cleanup/minor metadata fix; those compiled by full native7/8.

```sh
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -Ibuild/native/src -Isrc/engine/external/zlib \
  .artifacts/check_malinka_logic.cpp src/base/hash_libtomcrypt.cpp \
  /lib/x86_64-linux-gnu/libz.so.1 -o .artifacts/check_malinka_logic
ASAN_OPTIONS=detect_leaks=1 .artifacts/check_malinka_logic .artifacts/checks-3
```

Latest run3 **9448checks/0failures/exit0**,`.artifacts/check-logic-3.{log,exit}`. Fixture generator originally hardcoded checks-2; executed a temporary in-memory replacement to fresh checks-3. Reruns require fresh fixture roots.

### Actual physics ASan+UBSan

Temporary real-map harness `.artifacts/check_independent_world.cpp`,build script `.artifacts/build_world_checks.py` (18instrumented modules, rest native-linked).

```sh
python3 .artifacts/build_world_checks.py --harness-only --compile-only
python3 .artifacts/build_world_checks.py --link-only
LD_LIBRARY_PATH="$PWD/.artifacts/toolchain/gl/lib:$PWD/ddnet-libs/sdl/linux/lib64" \
  ASAN_OPTIONS=detect_leaks=1 .artifacts/check_independent_world
```

Latest run7 **8357checks/0failures/2door maps/exit0**;logs `.artifacts/world-check-run-7.{log,exit}`. Real coverage50x50(switch/tele/tune) and Tutorial2210x110(switch/tele);serial384ticks,4workers×8forks×128ticks,128ticks after source unload;mixed ordinary/private contexts;362handled C++allocation failures(109+253). Canonical helper vs separately transcribed Best reference384ticks/map, reversed primary/dummy order, duplicate/missing/optionalIDs and tick-overflow refusal. Character state comparison, not full-world byte identity or server/gameplay equivalence. Custom new/delete injection produces compiler mismatched-new-delete warning due malloc/free override, no runtime sanitizer error.

Link must preserve `engine/client/client.cpp.o` with `objcopy --redefine-sym main=MalinkaClientMain`,not remove it. Exact463arg client/harness links in `.artifacts/{client-link-command,world-link-command}.txt`. `CLayers::Init(Map,false,true)` required. Rebuild instrumented modules via `--compile-only` if physics sources change.

### Windows updater cross build

Temporary Zig `build/local/toolchain/zig/zig` **0.14.0-dev.2456+a68119f8f**;npm manifest SHA512 verified. `-fsyntax-only` found actual WorkerArgs missing, then after fix failed tool-level FileNotFound. **Real `-c` and full cross link passed**.

`.artifacts/windows-updater-link-5.log` contains exact commands for bundled zlib8C files + SHA256/updater C++20 and final:

```sh
build/local/toolchain/zig/zig c++ -target x86_64-windows-gnu -O1 -mwindows \
  <recorded zlib/hash/updater objects> -lshell32 -lgdi32 -luser32 \
  -o .artifacts/bestclient-updater-cross.exe
```

**exit0**,objdump`pei-x86-64`x64GUI; this is not full Windows client/MSVC/runtime. Logs `.artifacts/windows-updater-{compile-4,link-5}.{log,exit}`.

### Bootstrap / workflow static check

`python3 scripts/malinka_bootstrap.py --archive build/local/upstream/ddnet-libs.tar.gz --destination build/local/bootstrap-verified/ddnet-libs` **exit0**, SDK SHA and extraction verified. Existing-directory safety not yet separately negative-tested. `python -m py_compile` both scripts **exit0**. PyYAML6.0.2 temporary tool parsed workflow/matrix/action40hex pins **exit0**, not CI execution.

### Real client behavior

An actual `build/native/DDNet` startup was attempted with isolated HOME/XDG paths and SDL dummy. Process exit0 **but `Failed to initialize the graphics` / OpenGL failure**. `.artifacts/client-launch-1.*`. Do not call this renderer/gameplay smoke success. No working X server/software renderer/Wine/MSVC locally. All gameplay/UI/new renderers/remote badge **not verified in-game**. Benchmark command not run. TSan not run.

## Artifact/layout cautions

- `.artifacts` contains temporary scripts/harnesses/logs, ignored by Git. Scripts such as `finish_simulation_step.py`, `complete_visual_consumers.py`, earlier allocation fixes are **non-idempotent**, do not rerun blindly.
- Heavy deps/input archives/toolchains are under excluded `build/local`, with `.artifacts/upstream`,crates,toolchain and input symlinks. Compiled harnesses/objects/EXE/PDB moved to `build/local/check-outputs` with `.artifacts` symlinks to keep patchset within cap. They are rebuildable, not permanent test sources/artifacts in Git.
- KRX inputs protected and not auto-run. Do not copy binaries/secrets into source/release artifacts.
- No registry/CI/test pass should be marketed as full parity. Keep existing license/data notices and attribution.

## Immediate next verifiable actions

1. Test package generator against actual native stage and independently use C++ `ExtractZip/Verify/Install` on disposable installs; hostile/alias/overwrite cases and hashes. Source archive must correspond to built commit; a marker alone is not proof.
2. Complete GitHub Windows/MSVC build via prepared workflow on this exact branch (if pushing to test, only `git push origin arena/733fc6a9-best-v2`). Observe actual checks/exit/logs via gh and fix; don't claim YAML means success. No release/tag/publishing until runtime regressions/known missing list.
3. Continue concrete missing implementations (Grenade saver/KRX importer-vault/chat/RPC/advanced controls) without silently dropping full1.32 requirement. Do not ask sources again. Preserve unknown-dependent controls visibly gated.
4. Actual Windows renderer/gameplay/UI/panic/TAS/dummy/update and external badge authorization remain required; acquisition recipe in STATUS. Update this HANDOFF after new measured outcomes.
