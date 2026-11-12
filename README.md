# best-v2

## Avoid Freeze — текущий standalone milestone

Новая отдельная реализация находится в [`avoid-freeze/`](avoid-freeze/README.md). Это companion-процесс для узкого публичного BestClient-профиля: он только читает разрешённые данные процесса, локально рассчитывает возможное направление и выводит advisory-результат.

- В текущем milestone нет отправки клавиш или другого игрового ввода, DLL injection, записи в память процесса, патча или замены файлов клиента.
- Процесс открывается с query/read/synchronize правами; неизвестный образ и неподдерживаемые состояния должны завершаться `BLOCKED`.
- Это не полная Avoid-система и не гарантия исхода: расчёт ограничен одним предсказанным персонажем и небольшим проверенным набором статических условий. Реальные пользовательские карты, сессии и gameplay outcomes не проверены.
- На публичном сервере не запускать. Первый ручной запуск возможен только после успешной native MSVC x64-сборки и layout self-test, в отдельной local/private среде.

**Статус проверок:** pinned source и Linux policy/tests проверены; `ctest` прошёл 1/1 (9 изолированных assertions). Обновлённый reader прошёл Windows 2022/MSVC build, Windows `ctest`, `--self-test-layout` и аудит read-only/no-input в [workflow 43294756391](https://github.com/twin15388-beep/best-v2/actions/runs/43294756391); bundle после обновления инструкций собран в [run 43295350840](https://github.com/twin15388-beep/best-v2/actions/runs/43295350840). При `ERROR_PARTIAL_COPY` reader повторяет чтение небольшими page-bounded блоками, не пропуская байты; если полный блок всё равно недоступен, он остаётся fail-closed. Первая ручная проверка предыдущей сборки остановилась на root scan с Win32 299 и не выдала `RESULT`; обновлённый retry в живом процессе ещё не проверен. Свежий experimental artifact `AvoidFreeze-experimental-observe-only-win64` (447,283 bytes; не release; ZIP metadata SHA-256 `d8894d54888274eb6f24febc93504ef30ead6da1a6512985261dec06a6e4bf0a`) включает `SHA256SUMS.txt` для EXE. Это не Avoid-автоматизация и не подтверждение игровых исходов. Не использовать на публичном сервере.

## Старый preview — только справочный материал

[`tools/malinka-helper/`](tools/malinka-helper/README.md) — сохранённый прежний экспериментальный preview, а не текущая standalone-реализация. В нём есть отдельный opt-in путь игрового ввода; не запускайте этот старый preview и не используйте его input-флаги. Для дальнейшей работы ориентироваться на `avoid-freeze/` и его observe-only ограничения.
