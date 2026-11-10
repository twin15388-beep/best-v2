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

**Историческая сборка до восстановления среды:** toolchains/build outputs ниже сейчас отсутствуют. Последний native build18 с importer/vault прошёл exit0 до zero-aim правки; текущий full build требует свежего CI либо повторного bootstrap.

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
