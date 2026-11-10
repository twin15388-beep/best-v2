# BestClient + malinka (experimental)

Изменённый fork публичного BestClient **v3.1.1** с вкладкой `malinka` и адаптированными KRX-style компонентами. **Это не официальный KRX и пока не полный перенос KRX Ultimate 1.32.** Исходники защищённого KRX не восстановлены; совпадение с присланным бинарником BestClient не доказано.

- [Фактический статус, недостающие функции и проверки](docs/MALINKA_STATUS.md)
- [Сборка Windows/MSVC и Linux, SDK bootstrap, update packages](docs/MALINKA_BUILD.md)
- [Происхождение, pinned commits, hash и лицензии](docs/MALINKA_SOURCES.md)
- [Исходный README BestClient](docs/BESTCLIENT_UPSTREAM.md)
- [Текущая передача задачи и exact build/test logs](HANDOFF.md)

`Insert` скрывает Malinka UI до **полного перезапуска процесса**; reconnect его не возвращает. Это скрытие UI, не выключение функций. Не проверено в реальном игровом окне.

Официальные BestClient EXE updates не должны заменять этот fork: используется отдельный checked Malinka channel. Пакеты/rollback реализованы, но опубликованный и end-to-end проверенный update service пока отсутствует. Нормальный remote BestClient badge требует авторизации внешним сервисом; локальная подмена `fake` не применяется и не выдаётся за решение.

В F1 команда `malinka_status` перечисляет недоступные controls. Их gating — промежуточная мера, а не замена требования «все функции». Успешные сборки и isolated checks **не означают проверку gameplay**.

## Быстрый build

Python 3.11+, CMake, MSVC x64 либо Linux C++20 toolchain, Rust 1.92.0. Для свежего checkout:

```sh
python scripts/malinka_bootstrap.py
```

Дальнейшие platform-specific команды — в [MALINKA_BUILD.md](docs/MALINKA_BUILD.md). SDK/build outputs не хранятся в Git. Build workflow создаёт experimental artifacts без публикации release и без official credentials.

Сохранены `LICENSE` (BestProject MIT), `license.txt` (Teeworlds/DDRace/DDNet и asset notices) и происхождение существенно изменённой Pasta-Client adaptation.
