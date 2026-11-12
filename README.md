# best-v2

## Avoid Freeze — текущий standalone milestone

Новая отдельная реализация находится в [`avoid-freeze/`](avoid-freeze/README.md). Это companion-процесс для узкого публичного BestClient-профиля: он только читает разрешённые данные процесса, локально рассчитывает возможное направление и выводит advisory-результат.

- В текущем milestone нет отправки клавиш или другого игрового ввода, DLL injection, записи в память процесса, патча или замены файлов клиента.
- Процесс открывается с query/read/synchronize правами; неизвестный образ и неподдерживаемые состояния должны завершаться `BLOCKED`.
- Это не полная Avoid-система и не гарантия исхода: расчёт ограничен одним предсказанным персонажем и небольшим проверенным набором статических условий. Реальные пользовательские карты, сессии и gameplay outcomes не проверены.
- На публичном сервере не запускать. Первый ручной запуск возможен только после успешной native MSVC x64-сборки и layout self-test, в отдельной local/private среде.

**Статус проверок:** pinned source и Linux policy/tests проверены; `ctest` прошёл 1/1 (9 изолированных assertions). Native workflow **43243871336** прошёл на Windows 2022/MSVC x64: build, Windows `ctest`, `--self-test-layout` для всех используемых reader offsets и аудит исходников/import table. Он подтверждает compiler-derived layout checks только для этого build; процесс пользователя, runtime capture и игровые исходы не тестировались. Создан experimental artifact [`AvoidFreeze-experimental-observe-only-win64`](https://github.com/twin15388-beep/best-v2/actions/runs/43243871336) (445,499 bytes; не release); внутри `SHA256SUMS.txt` для проверки EXE. Не использовать на публичном сервере.

## Старый preview — только справочный материал

[`tools/malinka-helper/`](tools/malinka-helper/README.md) — сохранённый прежний экспериментальный preview, а не текущая standalone-реализация. В нём есть отдельный opt-in путь игрового ввода; не запускайте этот старый preview и не используйте его input-флаги. Для дальнейшей работы ориентироваться на `avoid-freeze/` и его observe-only ограничения.
