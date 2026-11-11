# Внешний помощник над неизменённым официальным клиентом

Первый diagnostic package: 2026-11-10 UTC; ниже учтён пользовательский отчёт с timestamp 2026-11-11. Активный запрос: отдельная программа Avoid Freeze без замены/изменения файлов настоящего клиента. Пользователь делегировал выбор доступа («как знаешь»). Выбрана внешняя read-only архитектура без DLL injection/patching и только обычный системный ввод при явном разрешении. **Полноценный внешний Avoid НЕ реализован. Static-map preview draft native-собран, прошёл MSVC ABI self-check и no-input LiveReader probe на точном public release image в controlled session; user-specific runtime и реальные Avoid outcomes не проверены. Diagnostic helper не является заменой требования.**

## Реально добавлено

Независимый проект [`tools/malinka-helper`](../tools/malinka-helper/README.md), исходники **0.1.1-diagnostic**, исправляющие устаревшее сохранение отчёта. Исходный пакет **0.1.0-diagnostic** сохранён как исторический результат ниже. Обе версии — **не ранее доставленный изменённый игровой клиент 0.1.1**.

- Собственный native Win32 GUI, resize/DPI, topmost toggle, async/cancellable inspection, собственная cleanup/lifetime. Явный запуск выбранного EXE без добавленных параметров; выход помощника не завершает клиента.
- Read-only file SHA-256/PE identity, `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE)`, bounded module/header reads. Никаких VM_WRITE/VM_OPERATION/debug privilege/hooks/remote threads/SendInput.
- Проверенный **точный публичный official EXE**, а не произвольная версия по строке имени. ASLR-adjusted measured vtable проверяется в загруженном модуле; поиск private-memory pointer candidates ограничен 512 MiB/65536 queries/1500 ms/32 candidates. Partial reads не сканируются. **Ни один candidate не объявляется валидированным CGameClient/world/character.**
- Стандартный `MumbleLink` section, свой `FILE_MAP_READ` view, bounded double collect/validation/freshness. Можно создать consumer-owned section до запуска игры. Нет PID автора и physics seqlock; координаты — render position, не velocity/physics snapshot. Данные не используются для управления.
- Новые JSON-отчёты только по явному выбору пользователя, CREATE_NEW/no-replace, отказ внутри выбранной client directory и через junction. Game EXE/DLL/config/storage не переписываются. Собственные обычные сохранения настроек самой игрой не отключаются.
- Неизвестная сборка: `blocked_unknown_build`. Offsets персонажа/collision/tuning/input — `null`, не фиктивные нули. `automatic_avoid.enabled=false` всегда. Remote badge не подменяется.

## Измеренная привязка и область поддержки

Public source: BestProjectTeam/BestClient v3.1.1, commit **cf192e3fb8590dbe2e4f66a191aa6616beff6975**. Public release asset **730984669**, ZIP 65165594 bytes/SHA256 **51688c041453d989522658cf46ae21b08ef46b28786da204b82e46c5166d7059**. Внутри `bestclient-3.1.1-win64/DDNet.exe`, 26206720 bytes/SHA256 **50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f**, AMD64, SizeOfImage **50638848**.

Measured RTTI: primary vtable file offset **19898296**, RVA **19904440**; complete-object-locator RVA **24123696**, type descriptor RVA **25441720**. Runtime VA = проверенная module base + RVA, не preferred image base. Имена/назначение методов по индексам не предполагаются. Константы централизованы в `core.h`; полная provenance `provenance/public-3.1.1-image.json` получена CI **43002224833/check130460886361**, success. PDB/debug files в этом asset не найдены.

**Новый пользовательский report совпадает с этим EXE по SHA256, размеру и загруженным headers/vtable.** Равенство всего Dropbox BestClient ZIP (120621904 bytes) не установлено, и профиль не распространяется автоматически на иные EXE/обновления. File identity/header match не доказывает, что процесс не модифицирован третьей стороной, и не доказывает внешний badge.

## Полученный пользовательский отчёт и найденный дефект

Первый JSON **уже получен**, повторно просить его или исходники не нужно. Точный приватный экземпляр `.artifacts/external-helper/incoming/MalinkaHelper-report-01.json`: 2160 bytes, SHA256 **70ad1673df0d577508faa1a8f625b1c8bf786e0f47f2cfd3b16e5808e937046e**. Личный путь, PID и runtime addresses в Git не публикуются. Это пользовательское диагностическое свидетельство, не независимо воспроизведённая проверка gameplay.

- EXE SHA/size/image, read-only handle, loaded headers/vtable совпали; ошибки чтения **0**.
- `root_scan: complete`, **13201408 bytes** attempted/read, **1069 regions**, **0 candidates**. В старой схеме нет начала/конца сканирования. `complete` — лишь конец перечисления, не atomic snapshot.
- `measured_at_utc=2026-11-11T10:55:36.038Z`; FILETIME процесса соответствует `2026-11-11T10:55:36.0224525Z`. Начало всей проверки — через **15.5475 ms** после создания процесса. Это не время начала самого сканирования.
- `Mumble: offline`, mapping создан helper; character/collision/tuning/input остаются `null`. Не установлено, находился ли клиент на карте при последующем сохранении, почему нет Mumble writer context и почему поиск не дал кандидатов.
- Из кода **подтверждён дефект helper 0.1.0**: `Launch → Start` выполнял один ранний проход, а `Save` обновлял только Mumble и записывал прежний process report. Поэтому этот JSON не доказывает отсутствие объекта после полной инициализации. Причина **самого** нулевого результата всё ещё неизвестна; слишком ранний проход — гипотеза, не доказательство.

### Исправление 0.1.1-diagnostic

`main_win32.cpp: Save → SaveTo → Start → Complete` заново измеряет выбранный процесс **после выбора имени отчёта**. Сохранение привязано к ранее измеренным PID + creation FILETIME; PID reuse отклоняется до чтения нового image/game addresses. При выходе/ошибке идентификации старый результат не подставляется, GUI не пишет его вместо нового. File-only save тоже повторяет file inspection. `InspectProcess` проверяет liveness после сканирования; кандидаты при завершении цели очищаются.

Схема **2**: начало/конец inspection, monotonic duration, время записи, причина измерения, начало/конец scan, возраст процесса в начале inspection/scan. Неизмеренные значения — `null`, 0 ms не превращаются в unavailable. При startup zero matches текст не делает вывода об отсутствии объекта позже. Формат input-independent: отправки клавиш/записи/injection по-прежнему нет.

### Независимая acquisition layout из public source

**CI43010774923/job130489525339 SUCCESS/watch0**, source `cf192e3fb8590dbe2e4f66a191aa6616beff6975`, MSVC/x64/C++20/Release. Generated headers получены pinned `datasrc`, не из modified game sources этого repo. Рецепт: `git show b8a4be09b943510b844705ebe4bacaef5474d735:.github/workflows/malinka-companion-layout.yml`.

Скомпилированы **два** варианта: без `CONF_VIDEORECORDER` `sizeof(CGameClient)=11047360`, с ним **11064576**. Соответствующие `m_Snap` member offsets **9013520 /9030736**. Это **member offsets, не RVA**. `LiveReader` использует `with_video` candidate; его process/collision/character/tuning поля частично прошли runtime probe на точном public EXE SHA выше: 250 frames, 249 matched transitions, 0 mismatches. Это не валидирует все CGameClient members или полную build configuration; JSON сохраняет `runtime_validated=false` для полного layout и описывает ограниченный scope. Полные compiler measurements/provenance: [`MALINKA_EXTERNAL_LAYOUT.json`](MALINKA_EXTERNAL_LAYOUT.json). Нельзя брать offsets из production `src` этого изменённого repo.

До успешного acquisition были две fixture/config ошибки: первая failure annotation обрезалась warnings, затем выявлен `SDL_audio.h` include path и исправлен на pinned `ddnet-libs/sdl/include/windows`. Сначала успешен no-video run **43009740471**, затем оба варианта выше. Это не compilation игрового клиента и не доказательство Avoid.

## Draft `MalinkaAvoidPreview` — ограничен; observe-only checked, gameplay не проверен

Добавлен необязательный исходник [`tools/malinka-helper/avoid`](../tools/malinka-helper/avoid/README.md); по умолчанию `MALINKA_BUILD_AVOID_PREVIEW=OFF`. Это отдельный процесс над неизменённым клиентом, а не новый client build. Он не использует process-write/injection; реальная отправка только обычных клавиш `SendInput` после `--enable-input` и явного F8-arm (либо отдельно запрошенного `--arm-once-ready`). Native MSVC workflow публикует 30-дневный экспериментальный test artifact (не release) на [странице workflow](https://github.com/twin15388-beep/best-v2/actions/workflows/malinka-avoid-preview-native.yml); начинать только с `OBSERVE-ONLY.cmd`. Контролируемый no-input probe exact public image не подтверждает пользовательский runtime и полный layout. До любого input mode сначала нужны observe-only проверка intended user process/session и его supported map/binds, затем сравнение predicted и actual результатов в одинаковых no-input freeze trials. Только после этого можно отдельно обсуждать input trial с явным разрешением и focus/key-conflict/watchdog/release checks; до тех пор input mode не запускать и программу готовой не считать.

Live reader требует точную проверенную public image identity, один complete root candidate, online client, ровно одного игрока/character entity, совпадающие local ID/dummy/world tick и predicted-client copies, корректные backlinks, стабильные повторные чтения, A/D single-command `+left`/`+right` binds и свежий input. Проверки повторного сбора — эвристика когерентности, не атомарная snapshot.

Preview намеренно поддерживает только static map: game/front tile arrays из whitelist; отсутствие ego quads, других prediction entities/characters, телепортов, speedup, switch, tune, door и extra-door layers. Tune-zone overrides и неподдерживаемые tiles/weapon/input fields fail closed. Один из следующих фактов неизвестен — ввода не будет. Даже в этом режиме policy — 40-tick preview, а не полная KRX/gameplay parity.

Windows target фиксирует MSVC x64 ABI по public source `cf192e3...` и требует recursive `ddnet-libs` submodule **`c0e6703fbcdbe03df2f26875427ec3951ce4ec21`**. Пример подготовки вне папки клиента:

```sh
git clone https://github.com/BestProjectTeam/BestClient.git <public-source-dir>
git -C <public-source-dir> checkout --detach cf192e3fb8590dbe2e4f66a191aa6616beff6975
git -C <public-source-dir> submodule update --init --recursive
cmake -S tools/malinka-helper -B build/avoid-preview -A x64 \
  -DMALINKA_BUILD_AVOID_PREVIEW=ON \
  -DMALINKA_PINNED_SOURCE=<absolute-public-source-dir>
cmake --build build/avoid-preview --config Release --target MalinkaAvoidPreview
```

`PinnedPhysics.cmake` verifies 1126 pinned source files and generates the required protocol headers outside the client. CMake intentionally rejects non-MSVC compiler ABI and a missing/mismatched submodule. [`MALINKA_EXTERNAL_LAYOUT.json`](MALINKA_EXTERNAL_LAYOUT.json) records the partial `with_video` reader probe, but keeps `runtime_validated=false` for the complete compiler-derived layout; exact file identity alone would not prove runtime layout.

Последний Linux ASan/UBSan/leak-detection fixture run: **12,797 checks, 361 runtime-trace transition pairs, 0 mismatches**, synthetic safe direction `-1`, baseline danger tick `36`, `no_game_input=true`. Logs: `.artifacts/external-helper/avoid-fixture-build-6.log` и `avoid-fixture-run-6.log`. Это только policy/importer fixture на сохранённой runtime-3 trace; он не компилирует Windows reader/preview, не проверяет `SendInput` или настоящий Avoid.

Windows/MSVC run **43032866534/job130564101587** прошёл pinned source/submodule checks, CMake configure, native x64 compile и `--self-test-abi`. Затем отдельный no-input runtime run **43034353037/job130569175285** запустил exact public release EXE на owned local freeze map: C++ `LiveReader`/policy получили **250 valid frames, 249 matched transitions, 0 mismatches, 5 unavailable samples, 247 decision evaluations**; `input_events=0`, `corrections=0`, process writes/DLL injection отсутствовали, EXE остался неизменён. Acquisition summary reports `client_exit=0`, `server_exit=1`; server-exit cause не исследовался и не приписывается `LiveReader`. Это partial validation только exercised fields в controlled session, с double-collect heuristic; не проверены user's active process, реальные corrective outcomes и badge. Ранее две configure failures были из-за CRLF; `core.autocrlf=false` это исправил. Zig Windows-GNU checks были лишь syntax diagnostics.

## Исторический Native Windows build 0.1.0 и isolated checks

Source **e8e1547f104c662e2b350a47f852609d3eaf1f61**. CI **43005315923**, job **130471217878**, Windows2022/MSVC x64: **SUCCESS / watch exit0 /1m0s**. MSVC **19.44.35229.0**, Windows SDK **10.0.26100.0**. Standalone commands, каждый exit0:

```powershell
cmake -S tools/malinka-helper -B build-helper-release -A x64
cmake --build build-helper-release --config Release --parallel 2
```

Независимая fixture project с теми же actual `core.cpp` и `win32.cpp`:

```powershell
cmake -S build-helper-check-source -B build-helper-native -A x64
cmake --build build-helper-native --config Release --parallel 2
build-helper-native/Release/helper-core-check.exe
build-helper-native/Release/helper-platform-check.exe <fresh-owned-temp-root>
```

Все exit0: **1080071 portable checks/0 failures**, **43 native Win32 checks/0 failures**. Проверены PE truncation/bounds/alignment/mutations, RTTI fingerprint bounds, finite telemetry/identity/offline/stale/tick wrap/reset, read-only self-process inspection, failed/partial reads, scan budgets/cancellation, new/no-replace report, Unicode and junction/client-directory refusal. Дополнительно **delivered standalone EXE** проверен CLI против себя и реального official EXE **как файла**, imports (нет forbidden injection/input imports), GUI create/resize/self-PID inspection/topmost/close. Синтетические Win32 сообщения, не физические клавиши; official game не запускался.

Финальная recipe с полными одноразовыми fixtures: `git show e8e1547f104c662e2b350a47f852609d3eaf1f61:.github/workflows/malinka-companion-check.yml`. Постоянный новый test suite не добавлен. Scratch `.artifacts/external-helper`, binaries `build/` ignored. До успеха устранены fixture-only CMake Windows path escaping и GetWindowTextW interprocess EDIT limitation; strict packaging обнаружил Windows CRLF, затем helper-local `.gitattributes` зафиксировал LF. Финальные 11 source files — **byte-identical Git blobs**, без EOL исключений.

Постоянный отдельный build-only workflow `malinka-companion-build.yml` после cleanup также исполнен: **CI43006689170/source f3231c0b964e044e73aaeec67162f15f335b95cd SUCCESS**. Обычный standalone build/package, без runtime fixtures. Его другой helper EXE SHA256 `2419916a134f4c00e1f4936d1f4d4914a23bc902a01cb382c044dc8bc3ac3ca7` не заменял доставленный и отдельно runtime-checked e8e1547 архив ниже. Helper source diff пустой; binary reproducibility не заявляется.

Linux/g++12.2, CMake3.31.6/Ninja1.11.1.3: configure/build portable core exit0. ASan/UBSan/LSan actual core compile0/run0/**1080071 checks/0 failures**:

```sh
export PATH="$PWD/.venv/bin:$PATH"
cmake -S tools/malinka-helper -B build/external-helper-local -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/external-helper-local --parallel 2
g++ -std=c++20 -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined \
  -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror \
  -I tools/malinka-helper tools/malinka-helper/core.cpp \
  .artifacts/external-helper/check_core.cpp -o build/external-helper-local/check-core-3
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  build/external-helper-local/check-core-3
```

`core-{configure-2,build-3,check-compile-3,check-run-3}.{log,exit}`. Первое configure не нашло Ninja вне PATH, исправлено PATH; это не новый Windows результат. Linux target не строит GUI.

## Полученный неизменённый diagnostic package

`build/delivery/malinka-helper-e8e1547/MalinkaHelper-0.1.0-diagnostic-windows-x64.zip` — **204963 bytes**, SHA256 **66c9685adf3a81a311c1fd6b984be62ef18376dd23c07eeb2e8cf547bd63c16d**.

Внутри `MalinkaHelper.exe` **355840 bytes**, SHA256 **b0c0ad64259778b57e9b337e42a8de21169f7b4962f82a8c7359554cc5cd70a3**, AMD64/PE32+/GUI. Также README-RU, LICENSE, CHECKS, SOURCE-SHA256 и все **11 точных source/build/provenance files**. ZIP CRC, blob/whole hashes, PE и каждый source path проверены локально, verifier exit0; `.artifacts/external-helper/verified-delivery.json`. Native source/binary не перепаковывались/не подменялись после CI. Это не доказательство binary reproducibility.

ZIP, README-RU и SHA256SUMS доставлены в отдельную Dropbox папку `/MalinkaHelper-diagnostic-e8e1547`, без замены прежних файлов. Все 3 размера и независимо рассчитанные Dropbox4MiB-block content hashes совпали с metadata readback. Создан public viewer/download ZIP link; access parameters в Git не хранятся. Metadata `.artifacts/external-helper/dropbox-delivery.json`.

## Что блокирует полноценный Avoid / ближайшие проверки

Есть policy/static-map draft и трассовая fixture; native MSVC compile, candidate self-check и controlled observe-only C++ `LiveReader` capture на exact public release image прошли. **Пользовательский active process, реальные corrective outcomes и gameplay не проверены.** 250 live frames дали 249 matched transitions/0 mismatches, но double-collect остаётся heuristic и не гарантирует atomic snapshot; 5 unavailable samples отказались fail-closed. Compiler-derived `CGameClient` metadata остаётся `runtime_validated=false` для полного layout; частично runtime-проверены только поля, которые потребовал reader, в одном controlled session.

Следующие gates: (1) observe-only capture именно в intended user process/session, включая supported map/binds и session/map invalidation; (2) одинаковые in-game freeze trials без input с отдельной оценкой predicted vs actual outcomes; (3) только после этого — отдельно разрешённое ordinary-input тестирование с focus/physical-key conflict/watchdog/release. Unsupported entities/layers/zones должны оставаться blocked, не обходиться guessed offsets или приближением.

Первый пользовательский отчёт уже установил exact EXE identity, повторно просить его не нужно. Он был снят в первые миллисекунды startup и не содержит доказательства состояния на карте. Контролируемый CI probe теперь проверил exact public release на owned test map, но не пользовательский active process; для этой среды всё ещё нужен отдельно согласованный **observe-only live on-map** result, не повторная отправка старого JSON. Mumble render telemetry по-прежнему недостаточно; screen-only automation не является доказанным эквивалентом.

**not verified in-game. Официальный badge со второго неизменённого клиента также не подтверждён.**
