# HANDOFF — Avoid Freeze standalone companion

## Active request and constraints

Продолжать работу над отдельным внешним Avoid Freeze helper рядом с официальным BestClient. Он не должен заменять или менять `DDNet.exe`/файлы клиента, внедрять DLL или записывать в память игры. Первый тестовый этап — строго observe-only, без отправки игрового ввода. Никаких публичных серверов или обещаний ban-safety; реальные пользовательские карты и gameplay outcomes не заявлять непроверенными. Пользователь решил обходиться без PC-агента. Ответ пользователю — на русском, первая строка `[Aero]`, обращение «бро».

Старый preview в `tools/malinka-helper/` содержит opt-in `SendInput`-путь и служит только историческим/reference материалом. Для текущей работы использовать только новую папку `avoid-freeze/`; не запускать старый preview и не активировать его input-флаги.

## Текущий branch / workspace

- Текущая ветка: `arena/52ad073e-best-v2`, статус `active`, base и последний известный HEAD: `244d7d487a48a482d1ecdd77d7190fe5bd5e35ca` (`Initial commit`). Не переключать ветки.
- Большая часть рабочего дерева (`avoid-freeze/`, `tools/`, `docs/`, workflow, handoff и ignore-файл) ещё не была частью исходного единственного README-коммита; Arena сохраняет изменения автоматически. Не удалять и не чистить эти файлы.
- Изменённый `README.md` теперь направляет к новому standalone milestone и явно помечает старый preview как reference-only.

## Реализация

- `avoid-freeze/` — отдельная реализация с pinned public BestClient source `cf192e3fb8590dbe2e4f66a191aa6616beff6975`; `pinned-source-sha256.json` проверяет 1126 файлов / 16,150,950 байт. Windows-ветка дополнительно требует `ddnet-libs` `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`.
- `src/platform/win32_reader.cpp` — Windows x64 reader: открытие только с query/read/synchronize правами, exact executable SHA-256 gate, AMD64/PE и vtable checks, ограниченный уникальный root scan и строго read-only снимок. Double-read — лишь эвристика согласованности, не атомарный snapshot.
- `src/platform/measured_layout.h` / `layout_check.cpp` содержат кандидатные MSVC x64 layout values, перенесённые из прежнего измерительного/reference preview. Native Windows компилятором и self-test в этой сессии **не проверены**; пока не называть layout подтверждённым.
- `src/main.cpp` принимает только `--observe-only --pid N [--run-ms ...]` или `--self-test-layout`; ввод не реализован. При ошибке reader выдаёт `BLOCKED`; `RESULT` прямо сообщает `sends_input=false` и `gameplay_outcome_verified=false`.
- Policy считает только узкий случай одного предсказанного персонажа и статической карты на pinned physics; прочие entity/layer/mode gates fail closed. Результат — совет направления, не полный Avoid и не гарантия исхода.
- `OBSERVE-ONLY.cmd` и `TEST-READ-ME.txt` описывают только будущую local/private проверку, которая запрещена до успешных native build и layout self-test. Не просить пользователя запускать текущую несобранную версию.

## Проверки, выполненные в этой сессии

- Pinned-source verifier: PASS — 1126 файлов / 16,150,950 байт.
- Linux GNU CMake build policy/test targets: PASS. `ctest`: 1/1 прошёл; test binary подтверждает 9 изолированных assertions.
- `src/main.cpp`: отдельный GNU C++20 syntax-only compile с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`: PASS после исправления знакового преобразования в JSON escaping.
- `git diff --check`: запускался до последних edits; повторить перед завершением.
- В окружении нет установленного системного CMake/MSVC/Windows SDK. Для Linux проверки CMake временно установлен из PyPI в `/tmp/avoidfreezecmake` (не в репозиторий).
- Не выполнялись: Windows/MSVC compile, native layout self-test, runtime process capture, Windows artifact packaging, реальный gameplay. Предыдущий успешный Actions artifact/run `43077478171` относится к старому reference preview на другом commit, не к этой новой реализации.

## Workflow / следующий проверяемый шаг

`.github/workflows/avoid-freeze-native.yml` добавлен для Windows 2022 / MSVC x64: проверяет pinned checkout, собирает executable и policy tests, запускает ctest и `--self-test-layout`, проверяет исходники/import table на write/injection/input API и только после прохождения создаёт экспериментальный observe-only artifact. Этот workflow ещё не выполнялся для текущего дерева: локальные изменения не были отправлены на GitHub. Windows-кандидатные offsets и workflow поэтому остаются unverified.

Следующий шаг — запустить/проверить именно этот workflow на текущей ветке, а не загружать старый artifact. Если native build, ABI self-test или аудит не проходят — исправить и повторить; пока это не пройдено, не передавать пользователю EXE и не просить process PID. Только после успеха можно обсуждать hash-идентифицированный bundle и отдельный ручной тест на local/private карте; старый Azure Blob download/Dropbox поиск не повторять.
