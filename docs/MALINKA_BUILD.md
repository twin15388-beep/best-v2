# Сборка и checked update packages

Это исходники экспериментального fork, не готовый полный KRX Ultimate 1.32. Актуальные ограничения: [MALINKA_STATUS.md](MALINKA_STATUS.md). Происхождение и pin-ы: [MALINKA_SOURCES.md](MALINKA_SOURCES.md).

## Обычная воспроизводимая схема зависимостей

- Существующий CMake project, **C++20**, Python **3.11+**, CMake **3.24+**; SDK MSVC x64 / Windows 11 для основного пользовательского target. Linux — GCC/Clang с C++20.
- SDK: `ddnet/ddnet-libs` commit `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`. `scripts/malinka_bootstrap.py` скачивает его с codeload.github.com и сверяет SHA-256 **до распаковки**. Уже существующий `ddnet-libs` никогда не заменяется.
- Rust MSRV исходной основы **1.85**; pipeline фиксирует **1.92.0 stable**, не произвольный nightly. `Cargo.lock` — оригинальный upstream lock с registry checksums; CMake вызывает Cargo с `--locked`.
- cxx **1.0.194**; остальные Rust versions/checksums в `Cargo.lock`. Полный список native SDK `VERSION` файлов записан в `MALINKA_DEPENDENCIES.json`. Bundled zlib/JSON/GLEW/Opus/SQLite/WavPack sources также находятся в `src/engine/external/`.
- Новые Python scripts используют только stdlib. Никаких offsets/signatures, runtime injection SDK или KRX binaries для сборки не требуется.

SDK bootstrap требует GitHub internet. Обычный Cargo требует registry/crates download; эта схема не предполагает локальные абсолютные `.artifacts` пути. Все исходники приложения, headers и build configuration находятся в дереве. External SDK/toolchains не коммитятся.

## Windows 11 / MSVC x64

Установить Visual Studio 2022 C++ desktop workload + Windows SDK, CMake, Python 3.11+ и Rust 1.92.0 для `x86_64-pc-windows-msvc`. Команды PowerShell из корня checkout:

```powershell
python scripts/malinka_bootstrap.py
rustup toolchain install 1.92.0 --profile minimal
rustup override set 1.92.0
cmake -S . -B build-release -A x64 `
  -DPREFER_BUNDLED_LIBS=ON -DPRECOMPILE_HEADERS=OFF `
  -DSERVER=OFF -DTOOLS=OFF -DDOWNLOAD_GTEST=OFF `
  -DVIDEORECORDER=OFF -DVULKAN=OFF -DDISCORD=OFF -DSTEAM=OFF
cmake --build build-release --config Release --target game-client bestclient-updater --parallel 2
cmake --build build-release --config Release --target package_zip --parallel 2
```

`DDNet.exe` и `bestclient-updater.exe` находятся под `build-release/Release/`. CMake `package_zip` создаёт обычный upstream-layout portable staging tree. **Его ZIP нельзя публиковать как Malinka updater asset**: сначала нужен flat checked ZIP ниже.

Video recorder/Vulkan/Discord/Steam выключены в минимальном build profile, чтобы не требовать необъявленные optional SDK/secrets. Это не реализация отсутствующего Malinka RPC и не обещание полной функциональной parity. Для возвращения optional Best features нужен отдельный build/test с их SDK. `BESTCLIENT_OFFICIAL_CODE` не задаётся: fork не приобретает официальную авторизацию.

## Linux / Ubuntu 24.04 x64

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build python3 pkg-config \
  libcurl4-openssl-dev libssl-dev libdbus-1-dev libfreetype6-dev \
  libnotify-dev libogg-dev libopus-dev libopusfile-dev libsdl2-dev \
  libsqlite3-dev libpng-dev libglew-dev libwavpack-dev zlib1g-dev
python3 scripts/malinka_bootstrap.py
rustup toolchain install 1.92.0 --profile minimal
rustup override set 1.92.0
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPREFER_BUNDLED_LIBS=OFF -DPRECOMPILE_HEADERS=OFF \
  -DSERVER=OFF -DTOOLS=OFF -DDOWNLOAD_GTEST=OFF \
  -DVIDEORECORDER=OFF -DVULKAN=OFF -DDISCORD=OFF -DSTEAM=OFF
cmake --build build-release --target game-client bestclient-updater --parallel 2
cmake --build build-release --target package_zip --parallel 2
```

Linux package с system libraries не является полностью статическим/универсальным AppImage; на машине назначения нужны совместимые SDL2/OpenGL/curl/font/audio libraries. Его наличие не доказывает renderer launch.

## Flat Malinka update ZIP

1. Поднять `MALINKA_VERSION` в `src/game/client/components/malinka/build_identity.h` **до** сборки. Релизный tag должен быть `malinka-vX.Y.Z` и совпадать с этой версией. Текущая 0.1.0 — development version, не заявление о полном переносе.
2. Создать source archive **того же commit**, который собран. На committed checkout:

```sh
mkdir -p build/artifacts
git archive --format=tar.gz --prefix=bestclient-malinka-source/ \
  -o build/artifacts/bestclient-malinka-source.tar.gz HEAD
```

3. Найти CMake staging directory `build-release/pack_*_zip/<portable-root>` и передать её generator. Пример Bash (в MSVC pipeline также используется Bash):

```sh
portable=(build-release/pack_*_zip/*)
test "${#portable[@]}" -eq 1
export SOURCE_DATE_EPOCH="$(git show -s --format=%ct HEAD)"
python3 scripts/malinka_package.py \
  --stage "${portable[0]}" \
  --source build/artifacts/bestclient-malinka-source.tar.gz \
  --platform windows-x86_64 \
  --output build/artifacts/bestclient-malinka-windows-x86_64.zip
```

Linux использует `--platform linux-x86_64` и `bestclient-malinka-linux-x86_64.zip`. Для ARM64 предусмотрено имя `linux-aarch64`, но соответствующая полная сборка не проверена.

Generator:

- требует настоящие client/updater build markers и license/status/source notices;
- отвергает symlinks/reparse points, reserved/ambiguous Windows paths, case aliases, `update/`, старый manifest и ресурсы сверх лимитов;
- хеширует stage files, повторно проверяет размер/hash во время упаковки и читает ZIP обратно;
- пишет `MALINKA_PACKAGE 1` manifest с SHA-256 каждого файла и source archive;
- использует только обычный deflate/stored ZIP без ZIP64 и без enclosing directory;
- создаёт ZIP + `.sha256` sidecar без перезаписи существующих outputs.

Оба source archive и update ZIP нужно хранить вместе с результатами сборки. Значение `source_sha256` идентифицирует companion archive, но само по себе не доказывает binary reproducibility. Build marker и SHA-256 — **не signature**. Они защищают от случайной установки stock client/битых файлов, а не от захвата доверенного repository/release account.

## Канал обновлений и сохранение файлов

Текущий endpoint зафиксирован в `build_identity.h`:

`https://api.github.com/repos/twin15388-beep/best-v2/releases?per_page=10`

Клиент рассматривает только non-draft/non-prerelease `malinka-vX.Y.Z`, точный platform asset basename, URL этого repository/tag и GitHub API `digest` вида `sha256:...`. Release без digest не принимается. Stock BestClient release packages отвергаются. Уменьшение/сравнение versions не доверяет marketing names.

Updater повторно проверяет archive SHA, ZIP bounds/CRC/path/type, manifest identity/platform/executable, все файлы и marker. Файлы сначала копируются и проверяются в stage, создаётся rollback journal, затем выполняются same-volume moves. Существующие config/replay/demo/downloadedmap/audio/arrow paths из `PreservedPath` не заменяются. Предыдущие файлы сохраняются под `update/malinka-backup-*`. При ошибке moves rollback пытается восстановить старые файлы; при неуспехе показывает путь backup. Остальные произвольные пользовательские файлы, совпавшие с package paths, не защищены автоматически.

Ограничения: filesystem TOCTOU полностью не устранён; journal/directories не имеют полноценной power-loss/fsync transaction гарантии. Тесты отказов не равны проверке всей пользовательской установки. Закрытие окна updater ждёт worker перед уничтожением mutex/critical section. Windows-worker проверяется отдельно от полного MSVC client build.

**Канал ещё не развёрнут и не проверен end-to-end.** `.github/workflows/malinka-build.yml` только собирает artifacts; не публикует Releases, не читает официальный secret и не выдаёт fork за official. Публиковать update release следует только после Windows gameplay/installer regressions и с честным списком отсутствующих функций.

## Измеренная sandbox-сборка (отличается от обычной)

**Историческая сборка до восстановления среды:** toolchains/build outputs ниже сейчас отсутствуют. Последний local native build18 с importer/vault прошёл exit0 до zero-aim правки; **текущий source уже прошёл fresh Windows/MSVC+Linux CI** ниже, local rebuild потребует повторного bootstrap.

В ограниченной среде доступны только GitHub/npm/PyPI hosts. Стандартные Linux dev packages/rustup/crates.io недоступны. Поэтому локальная проверка использует исключённые из Git toolchains и pinned GitHub Rust source fallbacks:

- GCC **12.2**, Python **3.11**, CMake **3.27.9**, Ninja **1.11.1.4**, Meson **1.5.2**;
- Rust **1.92.0-nightly (caccb4d03 2025-09-24)** из npm `@rusttonpm/*@0.92.20250925`, без rust-src;
- source-built SDL **2.32.10**, GLVND **1.7.0**; bundled native fallbacks;
- Zig **0.14.0-dev.2456+a68119f8f** из `@ryoppippi/zig-linux-x64@0.14.0-dev.2456` — только Windows updater cross check, **не MSVC**. npm SHA-512 сверена до извлечения;
- sandbox Cargo wrapper временно использует local path patches и снимает `--locked` / nightly build-std, поскольку crates.io недоступен. Fallback sources не проверены против crates.io archive checksums. Это ограничение измеренной локальной сборки, **не** рекомендация ослаблять обычный release build. В распространяемом Cargo.lock registry checksums восстановлены.

Точная последняя native команда:

```sh
export PATH="$PWD/.venv/bin:$PWD/.artifacts/toolchain/rust/bin:$PATH"
export CARGO_HOME="$PWD/.artifacts/cargo" CARGO_NET_OFFLINE=true
cmake --build build/native --target game-client bestclient-updater --parallel 2 -- -k 0
```

Configure использует `-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O1 -DNDEBUG'`, перечисленные выше disabled options, `-DPREFER_BUNDLED_LIBS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, sandbox `RUST_CARGO` wrapper и `-DCMAKE_CXX_FLAGS='-DGLEW_EGL -DGLEW_NO_GLU -I<repo>/.artifacts/toolchain/gl/include'`. Это специфический для headless sandbox workaround, не флаг нормальной MSVC сборки.

Точные результат/exit codes, логи и следующие проверки — в `HANDOFF.md`; компиляция не доказывает game feature behavior.

## Текущая isolated decoder проверка (после восстановления среды)

Временные fixtures/harness, не новый постоянный test suite. Генерируются **настоящие protocol headers** и используется настоящий `CMalinkaTas::SFrame`; никакого угаданного input layout. `.artifacts/acquire_replay_checks.py` повторно получает pinned vault, проверяет SHA/CRC, пишет независимые expected input/position hashes и94 hostile/valid fixtures. `.artifacts/check_krx_normalized.cpp` проверяет actual decoder, transactional failures, raw-file hashes, native-format roundtrip и allocations. Эти временные файлы не включены в Git.

Точные исполненные команды из корня checkout:

```sh
mkdir -p build/local/generated/src/generated build/local/check-outputs
python3 datasrc/compile.py network_header > build/local/generated/src/generated/protocol.h
python3 datasrc/compile.py client_content_header > build/local/generated/src/generated/client_data.h
python3 -m datasrc.seven.compile network_header > build/local/generated/src/generated/protocol7.h
python3 .artifacts/acquire_replay_checks.py
gcc -std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -c src/engine/external/json-parser/json.c \
  -o build/local/check-outputs/krx-json.o
g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -Ibuild/local/generated/src -Isrc/engine/external/zlib \
  .artifacts/check_krx_normalized.cpp src/base/hash_libtomcrypt.cpp \
  build/local/check-outputs/krx-json.o /lib/x86_64-linux-gnu/libz.so.1 \
  -Wl,--wrap=malloc,--wrap=calloc -o build/local/check-outputs/check-krx-normalized
ASAN_OPTIONS=detect_leaks=1 build/local/check-outputs/check-krx-normalized
```

Compile3/run1 **exit0**; **2366910 checks/0failures**,94 fixtures,179 vault files(151 accepted/28 rejected),1502zero-aim frames,35 C/3 C++ allocation failures. Initial compile attempts used nonexistent `hash_ctxt.c`/unneeded hash.cpp; final link above uses real SHA256 context implementation directly. `.artifacts/normalized-{compile-3,run-1}.{log,exit}`. Это parser/I/O execution, не full client build/physics/Windows runtime.

## Свежие vault/input checks и actual-package regression

Проверено в текущем sandbox (temporary harnesses, не постоянный test suite):

```sh
python3 .artifacts/acquire_vault_checks.py
g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -Isrc/engine/external/zlib .artifacts/check_vault_restored.cpp \
  src/game/client/components/malinka/file_safety.cpp src/base/hash_libtomcrypt.cpp \
  /lib/x86_64-linux-gnu/libz.so.1 -o build/local/check-outputs/check-vault-restored
ASAN_OPTIONS=detect_leaks=1 build/local/check-outputs/check-vault-restored build/local/vault-restored-run-1
g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -Ibuild/local/generated/src -Isrc/engine/external/zlib \
  .artifacts/check_input_replay_restored.cpp /lib/x86_64-linux-gnu/libz.so.1 \
  -o build/local/check-outputs/check-input-replay-restored
ASAN_OPTIONS=detect_leaks=1 build/local/check-outputs/check-input-replay-restored build/local/input-replay-restored-run-1
```

Compile/run **exit0**. Vault **6567/0**,32 ZIP fixtures(29invalid/3valid),179byte compares,8 I/O checkpoint cancellations,716allocation positions/691throws. Input/replay **107206/0**,8192 streams,324aim cases,backups/checksum/latch. Для повторного запуска необходимы **новые** disposable roots; нельзя удалять пользовательские каталоги. Logs `.artifacts/{vault-restored,input-replay-restored}-{compile-1,run-1}.{log,exit}`.

Full source **`2d00f1f40fb150656c8e916a758b544071619dfd`** действительно собран и упакован в [CI42885117777](https://github.com/twin15388-beep/best-v2/actions/runs/42885117777): Windows2022/MSVC и Ubuntu24.04, обе SUCCESS, normal Rust1.92.0 stable/locked Cargo. Точные configure commands выше/в workflow; выполненные build commands:

```sh
python scripts/malinka_ci_build.py --log build-release/compile.log -- \
  cmake --build build-release --config Release --target game-client bestclient-updater --parallel 2
python scripts/malinka_ci_build.py --log build-release/portable.log -- \
  cmake --build build-release --config Release --target package_zip --parallel 2
```

Обе команды **exit0** на обеих платформах; не Windows gameplay. Артефакты: `malinka-windows-x86_64`13795797827/117824429bytes; `malinka-linux-x86_64`13796002799/116176507bytes. Скачать их из sandbox через `gh run download` не удалось: Actions redirect уходит на недоступный blob-host. Поэтому выполнен **одноразовый remote integration check**, а не пропущена проверка/не подменён binary fixture.

[CI42887324042](https://github.com/twin15388-beep/best-v2/actions/runs/42887324042), workflow source commit53821ae52e70063bdca855f71a2828861534a13a: загрузка **реальных** artifacts42885117777, проверка build-environment source commit и `git diff` production sources/CMake/lock относительно2d00f1f, компиляция temporary actual C++ installer harness с:

```sh
g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -Isrc/engine/external/zlib build/check/package.cpp \
  src/game/client/components/malinka/file_safety.cpp src/base/hash_libtomcrypt.cpp \
  -lz -o build/check/package-check
ASAN_OPTIONS=detect_leaks=1 build/check/package-check \
  build/artifact/bestclient-malinka-<platform>.zip \
  build/artifact/bestclient-malinka-source.tar.gz <platform> build/check/disposable-install
```

Compile/run **exit0** в обоих jobs. Linux package1026files,894installed/132preserved-package-paths: **2785checks/0failures**. Windows package1038files,906installed/132preserved: **2821/0**. ZIP/SHA/manifest/default limits/source companion SHA, real install+readback, injected progress exception→rollback, backup, custom config/replays/vault/arrows preservation, wrong-platform/tamper refusal. **Windows ZIP проверялся на Linux файловой системе**, не под Wine/Win32; GUI/process wait/restart не запускались.

Точная временная recipe/harness доступна в `git show 53821ae52e70063bdca855f71a2828861534a13a:.github/workflows/malinka-package-regression-once.yml` и локально `.artifacts/check_ci_package.cpp`. Workflow **удалён после исполнения**; постоянный test suite/новые зависимости не добавлены. Временная YAML skip condition сначала имела quoting error (run42887321410/no compile jobs); исправлена в9910aa9, затем обычный workflow полностью восстановлен. Production sources не менялись.

| Platform | Checked ZIP SHA-256 | Companion source SHA-256 |
|---|---|---|
| linux-x86_64 | `3255c75080142bc7bcbb9a86510ce9c5b3b9b0394f533f9599168258221bc644` | `e56cb5036a96ddb149db1319aacea04ff96cc8e95a62f3629af9598a578859ce` |
| windows-x86_64 | `e1b559c0ad4c59c319dab1777abf04f9271a8f1ddb251d8435103fc4c1b81082` | `594524b556352694ccc3e4df261fefc644c06637a788b1ae3635a37df7e0f3f7` |

Каждый companion source hash совпал с manifest своего artifact; bytewise equality архивов между платформами не заявляется. Это **не published update release**, не end-to-end deployment и не proof о сохранении ещё нереализованных функций.

После удаления одноразового fixture обычный build workflow повторно выполнен: [CI42887818609](https://github.com/twin15388-beep/best-v2/actions/runs/42887818609), source/workflow **`0a0eeed0b463ade41801f2e76c0aaca5e1f338e9`**, **Windows/MSVC+Linux SUCCESS/exit0**,client/updater/package. Production sources/CMake/lock/normal workflow идентичны2d00f1f. Последние artifactsWindows13796858394/117825375bytes,Linux13797033179/116182078bytes,не downloaded locally/not releases. **Installer regression и ZIP/source hashes таблицы относятся к artifact2d00f1f**, не автоматически к новым архивам0a0eeed; binary reproducibility/restart не заявляются. Последующее сохранение документации не меняет проверенный production code.
