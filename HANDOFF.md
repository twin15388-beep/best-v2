# HANDOFF — Avoid Freeze standalone companion

## Active request and constraints

Продолжать работу над отдельным внешним Avoid Freeze helper рядом с официальным BestClient. Он не должен заменять или менять `DDNet.exe`/файлы клиента, внедрять DLL или записывать в память игры. Первый тестовый этап — строго observe-only, без отправки игрового ввода. Не использовать публичные серверы и не заявлять непроверенные пользовательские карты/gameplay outcomes. Пользователь решил обходиться без PC-агента. Ответ пользователю — на русском, первая строка `[Aero]`, обращение «бро».

Старый preview в `tools/malinka-helper/` содержит opt-in `SendInput`-путь и служит только историческим/reference материалом. Для текущей работы использовать только `avoid-freeze/`; не запускать старый preview и не активировать его input-флаги.

## Текущая ветка / workspace

- Текущая ветка `arena/52ad073e-best-v2`, статус `active`, base `244d7d487a48a482d1ecdd77d7190fe5bd5e35ca` (`Initial commit`). Исходный код, успешно прошедший native CI: commit `a08503a2e59f39551523c85ef0cef3b2f695281f`; последующие изменения — только документация и не меняют binary target. Не переключать ветки.
- Основной код и reference-материалы уже закоммичены и отправлены только в эту ветку. `build/` игнорируется. Не удалять и не чистить рабочие файлы.
- `README.md` описывает текущий standalone milestone; старый preview помечен архивным. `docs/MALINKA_EXTERNAL.md` — исторические notes без приватного report hash/path.

## Реализация

- `avoid-freeze/` — отдельный observe-only Windows x64 companion на pinned public BestClient source `cf192e3fb8590dbe2e4f66a191aa6616beff6975`. `pinned-source-sha256.json` сверяет 1126 файлов / 16,150,950 байт. Windows target требует `ddnet-libs` `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`.
- `src/platform/win32_reader.cpp`: процесс открывается только с query/read/synchronize правами; exact executable SHA-256 gate, AMD64/PE/vtable identity, bounded unique root scan, проверка поддерживаемых полей, read-only capture. Double-read — эвристика согласованности, не атомарный snapshot.
- `src/platform/measured_layout.h` / `layout_check.cpp`: кандидаты смещений object-relative, не RVAs. Native MSVC x64 layout self-test прошёл для этих compile-time checks; это не доказывает runtime layout объекта в пользовательском процессе.
- `src/main.cpp` принимает только `--observe-only --pid N [--run-ms ...]` или `--self-test-layout`; ввода, DLL injection и process-write APIs в текущем target нет. При ошибке reader выдаёт `BLOCKED`; `RESULT` отмечает `sends_input=false` и `gameplay_outcome_verified=false`.
- Policy ограничена одним предсказанным персонажем/статической картой на pinned physics и fail-closed gates. Результат — advisory direction, не полный Avoid и не гарантия исхода.
- `OBSERVE-ONLY.cmd` / `TEST-READ-ME.txt` разрешают только опциональную local/private проверку с artifact успешного run; передавать PID или просить запускать несобранную версию нельзя.

## Проверки

- Pinned-source verifier: PASS — 1126 файлов / 16,150,950 байт.
- Linux GNU CMake policy/test build: PASS; `ctest` 1/1, test binary сообщает 9 изолированных assertions.
- `src/main.cpp`: GNU C++20 syntax-only compile с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`: PASS.
- GitHub Actions run [43241468940](https://github.com/twin15388-beep/best-v2/actions/runs/43241468940) на commit `a08503a2e59f39551523c85ef0cef3b2f695281f`: **PASS** — Windows 2022/MSVC x64 build, Windows CTest, `--self-test-layout`, source/import audit на write/injection/input APIs и upload artifact. Это compile/layout/API checks; не было runtime process capture или gameplay test.
- Artifact: `AvoidFreeze-experimental-observe-only-win64`, ID `13955341157`, size 444,952 bytes, metadata digest `sha256:d2cbca84f77c2c3cfd12a3d3d774655f7cad5dd63ebbf666776c5bddc62fdc2c`, срок хранения до 2026-12-12. Это SHA-256 zip artifact, не EXE hash; EXE `SHA256SUMS.txt` включён внутрь пакета. Сам ZIP в workspace не загружался, поэтому исполняемый SHA здесь не измерен. Не повторять старый Azure-Blob download workaround.
- MSVC build первоначально поймал и исправил ошибки private-member array aliases и незаквалифицированного offset. Последний успешный run включает эти исправления.
- В Linux sandbox нет системного CMake/MSVC/Windows SDK. Для Linux проверки CMake временно установлен из PyPI в `/tmp/avoidfreezecmake` (не в репозиторий).

## Следующий шаг

Native build и compile-time layout self-test прошли. Дополнительной реализации перед необязательной проверкой не требуется. Если пользователь сам хочет manual check, использовать только artifact успешного Actions run и сверить EXE по приложенному `SHA256SUMS.txt`; затем допустим строго observe-only запуск на local/private карте. Ни runtime reader, ни игровой результат не проверены. При download-проблеме не обходить TLS/security controls и не отключать Defender/SmartScreen; Dropbox mirror сейчас отсутствует, так как artifact ZIP в workspace не получен.
