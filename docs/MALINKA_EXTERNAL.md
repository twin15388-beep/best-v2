# Внешний помощник над неизменённым официальным клиентом

Дата: 2026-11-10 UTC. Активный запрос: отдельная программа Avoid Freeze без замены/изменения файлов настоящего клиента. Пользователь делегировал выбор доступа («как знаешь»). Выбрана read-only архитектура без DLL injection/patching. **Полноценный внешний Avoid НЕ реализован. Диагностический этап не является согласованным сокращением этого требования.**

## Реально добавлено

Независимый проект [`tools/malinka-helper`](../tools/malinka-helper/README.md), **0.1.0-diagnostic**. Не путать с ранее доставленным **изменённым клиентом 0.1.1**.

- Собственный native Win32 GUI, resize/DPI, topmost toggle, async/cancellable inspection, собственная cleanup/lifetime. Явный запуск выбранного EXE без добавленных параметров; выход помощника не завершает клиента.
- Read-only file SHA-256/PE identity, `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE)`, bounded module/header reads. Никаких VM_WRITE/VM_OPERATION/debug privilege/hooks/remote threads/SendInput.
- Проверенный **точный публичный official EXE**, а не произвольная версия по строке имени. ASLR-adjusted measured vtable проверяется в загруженном модуле; поиск private-memory pointer candidates ограничен 512 MiB/65536 queries/1500 ms/32 candidates. Partial reads не сканируются. **Ни один candidate не объявляется валидированным CGameClient/world/character.**
- Стандартный `MumbleLink` section, свой `FILE_MAP_READ` view, bounded double collect/validation/freshness. Можно создать consumer-owned section до запуска игры. Нет PID автора и physics seqlock; координаты — render position, не velocity/physics snapshot. Данные не используются для управления.
- Новые JSON-отчёты только по явному выбору пользователя, CREATE_NEW/no-replace, отказ внутри выбранной client directory и через junction. Game EXE/DLL/config/storage не переписываются. Собственные обычные сохранения настроек самой игрой не отключаются.
- Неизвестная сборка: `blocked_unknown_build`. Offsets персонажа/collision/tuning/input — `null`, не фиктивные нули. `automatic_avoid.enabled=false` всегда. Remote badge не подменяется.

## Измеренная привязка и область поддержки

Public source: BestProjectTeam/BestClient v3.1.1, commit **cf192e3fb8590dbe2e4f66a191aa6616beff6975**. Public release asset **730984669**, ZIP 65165594 bytes/SHA256 **51688c041453d989522658cf46ae21b08ef46b28786da204b82e46c5166d7059**. Внутри `bestclient-3.1.1-win64/DDNet.exe`, 26206720 bytes/SHA256 **50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f**, AMD64, SizeOfImage **50638848**.

Measured RTTI: primary vtable file offset **19898296**, RVA **19904440**; complete-object-locator RVA **24123696**, type descriptor RVA **25441720**. Runtime VA = проверенная module base + RVA, не preferred image base. Имена/назначение методов по индексам не предполагаются. Константы централизованы в `core.h`; полная provenance `provenance/public-3.1.1-image.json` получена CI **43002224833/check130460886361**, success. PDB/debug files в этом asset не найдены.

**Эквивалентность с пользовательским Dropbox BestClient ZIP (120621904 bytes) не установлена. Профиль не распространяется автоматически на него, другие v3.1.1 или обновления.** File identity/header match не доказывает, что процесс не модифицирован третьей стороной, и не доказывает внешний badge.

## Native Windows build и isolated checks

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

## Что блокирует реальный Avoid / ближайший шаг

**Нет validated character core/velocity/hook/freeze/tick, цельного map/collision/tuning snapshot, verified bindings и независимой physics/input integration.** Перенос внутреннего `CMalinkaAvoidFreeze::ApplyAvoid` в пассивное окно не решает это. Mumble rendered position недостаточно; screen-only automation была бы отдельным приближённым проектом, не доказанным эквивалентом.

Ближайшее измерение: помощник **вне папки официального клиента**, официальный клиент на карте → ввести реальный PID → повторная проверка → новый JSON-отчёт вне игры. Он даст точную build identity и bounded root candidates, **не готовую физику**. Затем соответствующий source/compiler layout или matching symbols, независимая runtime проверка local ID/dummy/pos/vel/tick/freeze, session/map change invalidation; лишь после этого physics prediction, ordinary input + focus/physical-key conflict/watchdog/release и сравнение freeze outcomes на одинаковых картах. Не просить исходники повторно, не применять guessed signatures/offsets и не включать ввод по одной Mumble координате.

**not verified in-game. Полноценный Avoid, совместимость с пользовательской официальной сборкой и настоящий badge со второго неизменённого клиента пока не подтверждены.**
