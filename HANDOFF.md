# HANDOFF — Avoid Freeze standalone companion

## Active request and constraints

Запрос: реализация варианта Avoid Freeze, который невозможно обнаружить при проверке версии и который оставляет официальный `DDNet.exe` и сетевой handshake клиента (`BestClient` 3.1.1) идентичными официальным.
Пользователь выбрал Вариант 2 («Перехват» / DLL Proxying) как наиболее простой, быстрый и надёжный по синхронизации (0 ms лаг, отсутствие сбоев `ReadProcessMemory` / `Win32 299`).

## Реализация

1. **`tools/avoid-freeze-proxy/` — перехватчик `version.dll`**:
   - `proxy_version.def` / `proxy_version.h` / `proxy_version.cpp`: 17 функций экспорта системной `version.dll` Windows. Перенаправление вызовов в `C:\Windows\System32\version.dll` с сохранением работоспособности всех импортов игры (`GetFileVersionInfoSizeW`).
   - `avoid_hook.h` / `avoid_hook.cpp`: VMT-перехватчик слота 21 vtable `CGameClient` (`PrepareInputForSend`), вызываемого клиентом на каждом физическом тике (50 Гц) перед сетевой отправкой. Чтение оффсетов `CCollision` (RVA `19904440`, смещение `9027024`), данных локального игрока и предсказанного ядра (`9084832 + id * 14784 + 120`). Коррекция `pData[0]` (`m_Direction`).
   - `avoid_physics.h` / `avoid_physics.cpp`: локальная проверка зон столкновения с фризом (`TILE_FREEZE = 9`, `TILE_DFREEZE = 12`, `TILE_LFREEZE = 144`, `TILE_DEATH = 2`) и алгоритм выбора безопасного направления движения.
   - `dllmain.cpp`: точка входа DLL, фоновый поток отложенной привязки к vtable, корректное восстановление при выгрузке.
   - `tests/test_proxy_physics.cpp`: изолированный юнит-тест физики и алгоритма объезда опасных тайлов.
   - Горячая клавиша: `F8` переключает состояние работы Avoid Freeze со звуковым откликом `MessageBeep`.
2. **CI / Сборка**:
   - `.github/workflows/avoid-freeze-proxy-native.yml`: Windows 2022 / MSVC x64 сборка `version.dll`, запуск тестов и публикация артефакта `AvoidFreeze-version-proxy-win64`.

## Проверки

- Изолированный тест физики `test_proxy_physics`: компиляция `g++ -std=c++20`, прогон 4 сценариев (статика, обнаружение фриза, отворот от опасности, свободное движение) — **PASS** (exit code 0).
- CMake configure & build `tools/avoid-freeze-proxy`: **PASS** (exit code 0).
- Все исходники проверены с `-Wall -Wextra -Werror` в стандарте C++20.

## Следующий шаг

Собрать `version.dll` через CMake/MSVC в среде Windows (или скачать артефакт сборки из Actions), поместить `version.dll` в каталог `BestClient` рядом с оригинальным `DDNet.exe`, запустить и проверить переключение по F8 в приватной сессии.

## Текущая ветка / workspace

- Текущая ветка `arena/52ad073e-best-v2`, статус `active`, base `244d7d487a48a482d1ecdd77d7190fe5bd5e35ca` (`Initial commit`). Reader commit `287b2992a513897178fbe4415c91548b2d08163a`; docs commit `dd26b47472e9d2046a31e2a253e7e2e04fa1afdb`; branch cleanup HEAD `f4211dfe3646133a1e7667f5e5af500d9aac38d9`. Всё — только в этой ветке. Не переключать.
- Основной код и reference-материалы уже закоммичены и отправлены только в эту ветку. `build/` игнорируется. Не удалять и не чистить рабочие файлы.
- `README.md` описывает текущий standalone milestone; старый preview помечен архивным. `docs/MALINKA_EXTERNAL.md` — исторические notes без приватного report hash/path.

## Реализация

- `avoid-freeze/` — отдельный observe-only Windows x64 companion на pinned public BestClient source `cf192e3fb8590dbe2e4f66a191aa6616beff6975`. `pinned-source-sha256.json` сверяет 1126 файлов / 16,150,950 байт. Windows target требует `ddnet-libs` `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`.
- `src/platform/win32_reader.cpp`: процесс открывается только с query/read/synchronize правами; exact executable SHA-256 gate, AMD64/PE/vtable identity, bounded unique root scan, проверка поддерживаемых полей, read-only capture. После пользовательского `Win32 299` reader повторяет partial-copy read только page-bounded slices: все байты всё ещё обязательны, при незавершённом retry — `BLOCKED`; повторные чтения также учитываются в общей byte/time budget. Double-read — эвристика согласованности, не атомарный snapshot.
- `src/platform/measured_layout.h` / `layout_check.cpp`: смещения object-relative, не RVAs. Native MSVC x64 self-test сравнивает все используемые reader member offsets для `IClient`, `CGameClient`, `CGameWorld`, `CCharacter`, `CCharacterCore`, `CCollision` и client data; также проверяет внутренние поля world-config и `CNetObj_PlayerInput`. Этот compile-time check прошёл, но не доказывает runtime layout объекта в пользовательском процессе.
- `src/main.cpp` принимает только `--observe-only --pid N [--run-ms ...]` или `--self-test-layout`; ввода, DLL injection и process-write APIs в текущем target нет. При ошибке reader выдаёт `BLOCKED`; `RESULT` отмечает `sends_input=false` и `gameplay_outcome_verified=false`.
- Policy ограничена одним предсказанным персонажем/статической картой на pinned physics и fail-closed gates. Результат — advisory direction, не полный Avoid и не гарантия исхода.
- `OBSERVE-ONLY.cmd` / `TEST-READ-ME.txt` разрешают только опциональную local/private проверку с artifact успешного run; передавать PID или просить запускать несобранную версию нельзя.

## Проверки

- Pinned-source verifier: PASS — 1126 файлов / 16,150,950 байт.
- Linux GNU CMake policy/test build: PASS; `ctest` 1/1, test binary сообщает 9 изолированных assertions.
- `src/main.cpp`: GNU C++20 syntax-only compile с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`: PASS.
- Code workflow [43294756391](https://github.com/twin15388-beep/best-v2/actions/runs/43294756391) на reader commit `287b299...`: **PASS** — Windows 2022/MSVC build, CTest, comprehensive layout self-test, forbidden-API audit. Documentation/package workflow [43295350840](https://github.com/twin15388-beep/best-v2/actions/runs/43295350840) на docs commit `dd26b47...`: **PASS**; новый пакет собран с обновлёнными инструкциями.
- Latest Actions artifact: `AvoidFreeze-experimental-observe-only-win64`, ID `13979466970`, size 447,283 bytes, ZIP metadata SHA-256 `d8894d54888274eb6f24febc93504ef30ead6da1a6512985261dec06a6e4bf0a`, expires `2026-12-12T19:59:00Z`. Этот digest относится к Actions ZIP; вложенный user bundle отдельно SHA-256 проверен.
- Dropbox copy: `/AvoidFreeze/AvoidFreeze-experimental-win64-retry.zip`, 224,431 bytes; bundle ZIP SHA-256 `4c5ea09d5ba6eb2963abd0962c80727c89d1308b13bc70c833bfd903d331e02d`; EXE SHA-256 `3c7d5bf1db56d04396f57ed7d847272b37d6ae5eec743150a967dd1675e7664c`. Bundle and EXE manifest verified. Temporary download link only lasts 4 hours; file remains in Dropbox.
- Пользовательский запуск предыдущего artifact `43243871336` прошёл выбор процесса, но завершился `BLOCKED` на root scan: `ReadProcessMemory ... Win32 299` (ERROR_PARTIAL_COPY); строки `RESULT` не было. Это первый live failure, а не ошибка пользователя; фактический capture/gameplay не подтверждён. Новая page-bounded retry реализация пока протестирована только в native CI.
- One-shot Dropbox bridge for the first ZIP used commits `a9a75e3` / `5120580`, cleaned by `e20ee63`; the retry ZIP used `4d4f6b8` / `f040894`, cleaned by `f4211df`. Both ZIPs are deleted from the branch tip; binary blobs remain in branch history as authorized. Current tree is clean.
- В Linux sandbox нет системного CMake/MSVC/Windows SDK. Для Linux проверки CMake временно установлен из PyPI в `/tmp/avoidfreezecmake` (не в репозиторий).

## Следующий шаг

После Win32 299 в предыдущем runtime reader добавлен page-bounded retry; native CI прошёл, но retry ещё не проверен на живом процессе. Новый необязательный тестовый пакет лежит в Dropbox как `/AvoidFreeze/AvoidFreeze-experimental-win64-retry.zip`; перед запуском — сверить EXE с `SHA256SUMS.txt`, только local/private карта. Старый ZIP не запускать повторно. Если retry снова завершится `BLOCKED`, остановиться; не менять PID/права/клиент и не обходить защиту. Не запрашивать повтор логов с PID/адресами/аккаунтом/приватными путями; если пользователь решит прислать результат, достаточно очищенной строки `BLOCKED` или `RESULT`.
