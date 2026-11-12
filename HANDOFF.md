# HANDOFF — Avoid Freeze standalone companion

## Active request and constraints

Продолжать работу над отдельным внешним Avoid Freeze helper рядом с официальным BestClient. Он не должен заменять или менять `DDNet.exe`/файлы клиента, внедрять DLL или записывать в память игры. Первый тестовый этап — строго observe-only, без отправки игрового ввода. Не использовать публичные серверы и не заявлять непроверенные пользовательские карты/gameplay outcomes. Пользователь решил обходиться без PC-агента. Ответ пользователю — на русском, первая строка `[Aero]`, обращение «бро».

Старый preview в `tools/malinka-helper/` содержит opt-in `SendInput`-путь и служит только историческим/reference материалом. Для текущей работы использовать только `avoid-freeze/`; не запускать старый preview и не активировать его input-флаги.

## Текущая ветка / workspace

- Текущая ветка `arena/52ad073e-best-v2`, статус `active`, base `244d7d487a48a482d1ecdd77d7190fe5bd5e35ca` (`Initial commit`). Последний reader-кодовый commit `287b2992a513897178fbe4415c91548b2d08163a`; он прошёл native CI. Не переключать ветки.
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
- GitHub Actions run [43294756391](https://github.com/twin15388-beep/best-v2/actions/runs/43294756391) на reader commit `287b2992a513897178fbe4415c91548b2d08163a`: **PASS** — Windows 2022/MSVC x64 build, Windows CTest, comprehensive `--self-test-layout`, forbidden-API import audit и artifact upload. Это подтверждает сборку/тесты/layout/API, но не работу retry на живом процессе.
- Artifact для этого run: `AvoidFreeze-experimental-observe-only-win64`, ID `13978617746`, size 446,760 bytes, ZIP metadata digest `sha256:4a2c2dd97f11b336471fabd6d59d00732109a77613f252abaca6f4fb955ac721`, expires `2026-12-12T19:55:29Z`. Digest относится к Actions ZIP, не к вложенному EXE; пакет содержит `SHA256SUMS.txt`.
- Пользовательский запуск предыдущего artifact `43243871336` прошёл выбор процесса, но завершился `BLOCKED` на root scan: `ReadProcessMemory ... Win32 299` (ERROR_PARTIAL_COPY); строки `RESULT` не было. Это первый live failure, а не ошибка пользователя; фактический capture/gameplay не подтверждён. Новая page-bounded retry реализация пока протестирована только в native CI.
- Один-shot Dropbox bridge commits `a9a75e3` / `5120580` и cleanup `e20ee63`: ZIP удалён из текущего tree, но binary остаётся в веточной истории, как было согласовано. Не переименовывать ветку.
- В Linux sandbox нет системного CMake/MSVC/Windows SDK. Для Linux проверки CMake временно установлен из PyPI в `/tmp/avoidfreezecmake` (не в репозиторий).

## Следующий шаг

После Win32 299 в предыдущем runtime reader добавлен page-bounded retry; новый Windows/MSVC build прошёл CI, но retry ещё не проверен на живом процессе. Текущий артефакт (run `43294756391`) — единственный подходящий для будущей необязательной local/private проверки; предыдущую версию не запускать повторно. Никаких изменений клиента и обхода Defender/SmartScreen не делать. Не запрашивать повтор логов с PID/адресами/аккаунтом/приватными путями; если пользователь решит прислать результат, достаточно очищенной строки `BLOCKED` или `RESULT`.
