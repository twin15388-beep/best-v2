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

1. Поднять `MALINKA_VERSION` в `src/game/client/components/malinka/build_identity.h` **до** сборки. Релизный tag должен быть `malinka-vX.Y.Z` и совпадать с этой версией. Текущая 0.1.1 — development version, не заявление о полном переносе.
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

## Получение Windows файла без Actions/Azure URL

Пользователь сообщил `ERR_SSL_PROTOCOL_ERROR` на artifact blob-host. Не отключать TLS. Тот же main ZIP из run42887818609 перенесён в файлы чата и подключённый Dropbox,без нового релиза/автообновления/пересборки. `bestclient-malinka-windows-x86_64.zip` **60418532bytes**,SHA256 **`653c7ed5853e6ae4bce948b0525442c1ac3c6305b62099e4c17a8c0d4862fb81`**;`DDNet.exe` **8948224bytes**,SHA256 **`b4a4cf17c450b57a68e43947176215f0321d61bf8c5bc2a4e4d6ac7db4a0e0d0`**. Это inner клиентский ZIP,а не outer117825375-byte Actions bundle.

One-shot transfer run42897858639 SUCCESS;точная recipe в `git show 805eedbe5b4048e21a0f7a57a5cce2de13b5ffe7:.github/workflows/malinka-delivery-once.yml`. Transfer-only bounded unreferenced Git blobs не включались ни в binary commits,ни в branch refs;workspace verification per-chunk/whole SHA/CRC **exit0**. Temporary workflow удалён,normal workflow восстановлен. Fresh actual C++ `Package::ExtractZip`+`Package::Verify` под ASan+UBSan+LSan проверили **1038 manifest files/exit0**,не Win32 runtime. Exact commands и local logs — в HANDOFF. Access/share links не хранятся в Git.

Распаковать весь ZIP в **новую тестовую папку**,не заменять основной BestClient;запускать `DDNet.exe` вместе с DLL и `data`. Чтобы не читать/переписывать обычный профиль,**перед первым запуском в тестовой копии** заменить `storage.cfg` на:

```text
add_path $CURRENTDIR
add_path $DATADIR
```

Запускать из этой тестовой папки. Это отдельный локальный профиль,не доказательство проверки в игре;в оригинальном ZIP storage.cfg не менялся.


## Avoid / Insert: проверка исправлений 0.1.1

Текущий production source **`facfc8563106d5421de5c73f861e2e5df342f6ad`**: full ordinary [CI42908808859](https://github.com/twin15388-beep/best-v2/actions/runs/42908808859) **SUCCESS Windows2022/MSVC x64 (10m25s) + Ubuntu24.04 (8m43s), watch exit0**, включая client/updater/package и расширенные Quality/Life ranges. Выполненные commands (каждый exit0 на обеих платформах):

```sh
python scripts/malinka_ci_build.py --log build-release/compile.log -- \
  cmake --build build-release --config Release --target game-client bestclient-updater --parallel 2
python scripts/malinka_ci_build.py --log build-release/portable.log -- \
  cmake --build build-release --config Release --target package_zip --parallel 2
```

Windows outer artifact13807725269/117850062bytes, Linux13805474638/116204955bytes. Предыдущиеc4c450b/3c7d586 artifacts нельзя выдавать за facfc85.

Свежая isolated helper проверка расширенных ranges:

```sh
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -Wpedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc \
  .artifacts/avoid-review/check_policy.cpp -o build/local/check-avoid-policy-5
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/local/check-avoid-policy-5
```

**Compile0/run0**, 29656558 checks/0 failures/595204 rollouts. Fixtures/logs ignored; постоянный test suite не добавлен. Current component fixture использует actual client objects из CMake compile_commands/Ninja link, четыре production translation units ASan/UBSan и `-fno-access-control` только для fixture wiring. Не whole-program instrumentation; LSan отключён только для no-renderer component fixture из-за обычной renderer-owned CGameConsole logger ownership, helper LSan включён. Actual Insert renderer/gameplay **not verified in-game**.


Actual component [CI42908966581](https://github.com/twin15388-beep/best-v2/actions/runs/42908966581) **SUCCESS/exit0**, job130154252439, source checkout строго facfc85: **2102 checks/0failures**,41 assisted scenarios,128 handled allocation refusals,2actual maps. ConfigQuality1000/Life298,AFK measured idle1100ms,input-slot/limits/live-immutability,packetFOV1/45/180/360; synthetic Insert → actual handler/tab redirect/F1 filtering/normal completions/explicit+toggle execution/history preservation/reset-latch. Max29.2549ms under Linux instrumentation, не Windows performance proof. Полный normal game-client build,4instrumented compile commands,fixture compile/link/run иhelper compile/run exited0.

Reproduction recipe/embedded disposable C++ and Python: `git show 1d1a032dd96075a621c38a4a6e63f9d92cffdd47:.github/workflows/malinka-avoid-once.yml`. Driver reuses actual CMake compile_commands/Ninja link, `objcopy --redefine-sym main=MalinkaClientMain` only for engine object. `-fno-access-control` только для fixture; production headers не изменяются ради теста. Run env: `ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:alloc_dealloc_mismatch=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; helper LSan включён. Не full-world sanitization, не physical keyboard/renderer/server. В final branch disposable workflow удаляется; исторический commit оставляет recipe доступной без постоянного test suite.


### Доставка и actual-package regression именно facfc85

[Runner transfer42914015262](https://github.com/twin15388-beep/best-v2/actions/runs/42914015262), workflow **c1738ff144d83c2736779da5ea8ecf4a423220c4**, job130171633080 **SUCCESS47s/watch exit0**: проверены native run/source/artifact identity, source manifest SHA, ZIP CRC/PE AMD64/0.1.1 marker, затем по3 bounded unreferenced blobs на оригинальный Windows ZIP и companion tar. Binary payloads не добавлены в commits/trees/refs/releases. В sandbox получены все6chunks с GitOID/SHA256/size checks; подписанный недоступный Azure URL и отключениеTLS не использовались.

| Оригинальный файл | Bytes | SHA-256 |
|---|---:|---|
| bestclient-malinka-windows-x86_64.zip |60425920|`b7d0a2349b6c79c5a87299909d14b678ae5f4150f91a1cb0e99e0bb01225b231`|
| bestclient-malinka-source.tar.gz |57789009|`d605a975ecbcf0e8c4ffd3b1ab4abbe6a8d7e523796d2a38c3f8d97d501e3c7a`|

Оба оригинала повторно проверены локально, ZIP CRC/PE/marker/source manifest SHA. Exact source-content comparison по2375paths:822blobs совпали побайтово,1553text files имеют **только LF→CRLF export** относительно Git blobs. Начальная strict LF-byte assertion на `.cargo/config.toml` закономерно остановила retrieval script после скачивания всех6chunks; последующая проверка допускает только **точное** преобразование каждого bareLF→CRLF, сверяет исходный GitOID/длины/каждыйpath и отвергает иные различия. Оригинальные tar/ZIP/manifest не изменены. Не называть весь tar побайтово идентичным LF Git tree. Для LF builds рекомендуется canonical git checkout facfc85; archive Cargo.lock тоже CRLF, тогда как записанный выше SHA относится к tracked LF blob. `verify-received.exit=0`, детали `.artifacts/avoid-review/verify-received.log`, script`verify_received.py`.

Свежая actual C++ installer fixture из прежней disposable recipe53821ae, compiled с текущими production headers/0.1.1 marker:

```sh
g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -Isrc/engine/external/zlib .artifacts/avoid-review/check_final_package.cpp \
  src/game/client/components/malinka/file_safety.cpp src/base/hash_libtomcrypt.cpp \
  /lib/x86_64-linux-gnu/libz.so.1 -o build/local/check-final-package
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/local/check-final-package \
  build/local/delivery-0.1.1-facfc85/bestclient-malinka-windows-x86_64.zip \
  build/local/delivery-0.1.1-facfc85/bestclient-malinka-source.tar.gz \
  windows-x86_64 build/local/ci-package-facfc85-check-1
```

**Compile0/run0**, **2821 checks/0failures**,1038files/906installed/132preserved. Real ZIP/manifest/hash/source SHA/default limits, wrong platform, modified license tamper refusal, progress exception after first live mutation→rollback +old-file fingerprints, successful install/readback,config/native replay/raw vault/custom arrows/other sentinel preservation и backups. Это **Linux filesystem check Windows package**, не Win32 process wait/UAC/updater window/restart/gameplay. Fresh disposable root обязателен, старый root не удалять ради rerun. Logs `.artifacts/avoid-review/final-package-{compile,run}.{log,exit}`.

Main ZIP открыт в viewer. В Dropbox `/Malinka-test-0.1.1-facfc85` загружены оригинальные ZIP/source +`SHA256SUMS.txt`/`TEST-NOTES-0.1.1.txt`; size и4MiB-block Dropbox content hash всех4files совпали при readback. Public links выданы в чате, access parameters не коммитятся. Companion source включает snapshot именно facfc85, в том числе тогдашний disposable workflow; final branch удаляет temporary workflow/contents-write и восстанавливает обычный workflow изc4c450b byte-for-byte. После docs/workflow-only cleanup повторная native сборка не нужна и не заявляется: `git diff facfc85 -- src CMakeLists.txt Cargo.lock` пустой.
