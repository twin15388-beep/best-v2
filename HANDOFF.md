# HANDOFF — Avoid Freeze standalone companion

## Active request and constraints

Продолжать работу над отдельным внешним Avoid Freeze helper рядом с официальным BestClient. Он не должен заменять или менять `DDNet.exe`/файлы клиента, внедрять DLL или записывать в память игры. Первый тестовый этап — строго observe-only, без отправки игрового ввода. Не использовать публичные серверы и не заявлять непроверенные пользовательские карты/gameplay outcomes. Пользователь решил обходиться без PC-агента. Ответ пользователю — на русском, первая строка `[Aero]`, обращение «бро».

Старый preview в `tools/malinka-helper/` содержит opt-in `SendInput`-путь и служит только историческим/reference материалом. Для текущей работы использовать только `avoid-freeze/`; не запускать старый preview и не активировать его input-флаги.

## Текущая ветка / workspace

- Текущая ветка `arena/52ad073e-best-v2`, статус `active`, base `244d7d487a48a482d1ecdd77d7190fe5bd5e35ca` (`Initial commit`). Исходный код, успешно прошедший последний native CI: commit `78c3bcb7af338905723384426dc2f7ec2af03479`; последующие изменения — только документация и не меняют binary target. Не переключать ветки.
- Основной код и reference-материалы уже закоммичены и отправлены только в эту ветку. `build/` игнорируется. Не удалять и не чистить рабочие файлы.
- `README.md` описывает текущий standalone milestone; старый preview помечен архивным. `docs/MALINKA_EXTERNAL.md` — исторические notes без приватного report hash/path.

## Реализация

- `avoid-freeze/` — отдельный observe-only Windows x64 companion на pinned public BestClient source `cf192e3fb8590dbe2e4f66a191aa6616beff6975`. `pinned-source-sha256.json` сверяет 1126 файлов / 16,150,950 байт. Windows target требует `ddnet-libs` `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`.
- `src/platform/win32_reader.cpp`: процесс открывается только с query/read/synchronize правами; exact executable SHA-256 gate, AMD64/PE/vtable identity, bounded unique root scan, проверка поддерживаемых полей, read-only capture. Double-read — эвристика согласованности, не атомарный snapshot.
- `src/platform/measured_layout.h` / `layout_check.cpp`: смещения object-relative, не RVAs. Native MSVC x64 self-test сравнивает все используемые reader member offsets для `IClient`, `CGameClient`, `CGameWorld`, `CCharacter`, `CCharacterCore`, `CCollision` и client data; также проверяет внутренние поля world-config и `CNetObj_PlayerInput`. Этот compile-time check прошёл, но не доказывает runtime layout объекта в пользовательском процессе.
- `src/main.cpp` принимает только `--observe-only --pid N [--run-ms ...]` или `--self-test-layout`; ввода, DLL injection и process-write APIs в текущем target нет. При ошибке reader выдаёт `BLOCKED`; `RESULT` отмечает `sends_input=false` и `gameplay_outcome_verified=false`.
- Policy ограничена одним предсказанным персонажем/статической картой на pinned physics и fail-closed gates. Результат — advisory direction, не полный Avoid и не гарантия исхода.
- `OBSERVE-ONLY.cmd` / `TEST-READ-ME.txt` разрешают только опциональную local/private проверку с artifact успешного run; передавать PID или просить запускать несобранную версию нельзя.

## Проверки

- Pinned-source verifier: PASS — 1126 файлов / 16,150,950 байт.
- Linux GNU CMake policy/test build: PASS; `ctest` 1/1, test binary сообщает 9 изолированных assertions.
- `src/main.cpp`: GNU C++20 syntax-only compile с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`: PASS.
- GitHub Actions run [43243871336](https://github.com/twin15388-beep/best-v2/actions/runs/43243871336) на commit `78c3bcb7af338905723384426dc2f7ec2af03479`: **PASS** — Windows 2022/MSVC x64 build, Windows CTest, comprehensive `--self-test-layout`, source/import audit на write/injection/input APIs и upload artifact. Это compile/layout/API checks; runtime process capture и gameplay test не было.
- Artifact: `AvoidFreeze-experimental-observe-only-win64`, ID `13954958609`, size 445,499 bytes, metadata digest `sha256:2c257ef48d7ba5cca3d4f40be581b5228b66125d2235dcca015a371bc8ec0c2c`, expires `2026-12-12T15:07:37Z`. Digest относится к ZIP, не к EXE; EXE `SHA256SUMS.txt` включён внутрь пакета. Сам ZIP в workspace не загружался, поэтому EXE hash здесь не измерен. Старый Azure-Blob download workaround не повторять.
- Ранее MSVC CI нашёл и помог исправить array alias и незаквалифицированный offset; последний успешный run прошёл со всеми добавленными layout checks.
- В Linux sandbox нет системного CMake/MSVC/Windows SDK. Для Linux проверки CMake временно установлен из PyPI в `/tmp/avoidfreezecmake` (не в репозиторий).

## Следующий шаг

Native build и compile-time layout self-test прошли. Дополнительной реализации перед необязательной проверкой не требуется. Если пользователь сам хочет manual check, использовать только artifact успешного Actions run и сверить EXE по приложенному `SHA256SUMS.txt`; затем допустим строго observe-only запуск на local/private карте. Ни runtime reader, ни игровой результат не проверены. При download-проблеме не обходить TLS/security controls и не отключать Defender/SmartScreen; Dropbox mirror сейчас отсутствует, так как artifact ZIP в workspace не получен.
