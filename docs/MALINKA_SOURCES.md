# Malinka: происхождение исходников и границы переноса

Дата фиксации: **2026-11-10**. Это изменённый fork BestClient, **не официальный KRX** и не восстановленные исходники защищённого KRX Ultimate 1.32. Присланные бинарники не выдаются за пересобранные из этого дерева.

## Зафиксированные публичные источники

| Назначение | Репозиторий / commit | SHA-256 скачанного source tarball |
|---|---|---|
| Основа BestClient v3.1.1 | `BestProjectTeam/BestClient` — `cf192e3fb8590dbe2e4f66a191aa6616beff6975` | `f34490efed9281e87e338be3b0e852dace4963475ffcd2428f66c62281c782fa` |
| Сторонний донор KRX-style компонентов Pasta-Client | `iwasinabyssidk-hub/Pasta-Client` — `dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f` | `415b0c30571d73db2454883db94d24427d4f7545060e25e31ae8c9ae5378cac1` |
| Документация / changelog KRX, только исследование | `krxclient/krx-docs` — `8c193ca2a27cc7ba5d4e2d6ad7686c6e99910c43` | `dd039dada33d6e4c97b99d535940c7d389d5383b683f47f7192e1a59b5f2b2e1` |
| SDK зависимостей | `ddnet/ddnet-libs` — `c0e6703fbcdbe03df2f26875427ec3951ce4ec21` | `5539f55924a88c53f09bb3f2b9fbb8e20d87351aee43210eceddc7994385806f` |

Адреса:

- <https://github.com/BestProjectTeam/BestClient/tree/cf192e3fb8590dbe2e4f66a191aa6616beff6975>
- <https://github.com/iwasinabyssidk-hub/Pasta-Client/tree/dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f>
- <https://github.com/krxclient/krx-docs/tree/8c193ca2a27cc7ba5d4e2d6ad7686c6e99910c43>
- <https://github.com/ddnet/ddnet-libs/tree/c0e6703fbcdbe03df2f26875427ec3951ce4ec21>

Для получения неизменяемого tarball: `https://codeload.github.com/<owner>/<repo>/tar.gz/<commit>`. Указанные hash относятся к реально скачанным архивам; GitHub может изменить упаковку tarball без изменения commit. Bootstrap при несовпадении hash останавливается, а не обновляет pin автоматически.

Best-основа содержит DDNet **20.1.1** / TClient **10.9.0**, а KRX 1.32 по changelog — DDNet **18.9.1**. Одинаковая физика, input timing и бинарная совместимость не предполагаются. Pasta датирован 2026 годом, экспериментален и не является исходниками официального KRX 1.32. Более поздние функции его донора не подтверждают состав KRX 1.32.

## Что изменено по сравнению с донором

`src/game/client/components/malinka/` и `src/engine/shared/config_variables_malinka.h` содержат переименованную и существенно изменённую адаптацию Pasta. Переработаны независимые snapshot мира, TAS/replay I/O, управление input streams, завершение потоков benchmark, обработка неполных/невалидных данных, renderer consumers и UI gates. В заголовках/исходниках сохранена отметка об исходном доноре.

Изменены также Best menu dispatch/navigation/search, приоритетный `Insert` input path, client registration, controls, camera, player/projectile/trail rendering, prediction entities/gameworld/collision и собственный updater. Новые `file_safety.h`, `simulation_step.h`, `build_identity.h`, package installer/ZIP parser и release scripts не следует приписывать разработчикам оригинального KRX.

`docs/BESTCLIENT_UPSTREAM.md` — сохранённый README исходной Best-основы. Его screenshots и утверждения описывают upstream, а не результаты проверки этого fork.

## Входные бинарники: измерено отдельно

### KRX

- ZIP: `KRX-Client-18.9.1-win64-ultimate-v1.32.zip`, **80079820** байт.
- ZIP SHA-256: `77aec68613b9f0f01dd43ec27f955e21a6546076ef539863025ea6140ace67e9`.
- Все **973** ZIP CRC проверены; C/C++ sources, PDB и build project в архиве не найдены.
- `KRX Client.exe`: **14364176** байт, SHA-256 `f9ec1d9f050e4ecd9f426d0ae71abaf5d91972a082636822883d419856858c85`.
- PE32+ x64; PE header file offset `0x138`; preferred ImageBase `0x140000000`; SizeOfImage `0x211f000`; entry RVA `0x15f2058`; debug directory отсутствует.
- `.boot`: RVA `0x15f2000`, raw file offset `0x286e00`, raw size **11714560**. Это сведения из файла, **не runtime VA и не offsets игровых объектов**.
- Упаковка/защита наблюдается статически; конкретный protector и runtime layout не установлены. EXE не исполнялся и не распаковывался в Windows debugger.

### Best

Dropbox сообщил ZIP **120621904** байта и content hash `d9e6b2e9d31e2beb61b7dfd4d4b858ead344eacd8f9cfc486ccd80f085d07542`. Download-коннектор отказал из-за лимита **104857600** байт. Этот архив не исследован; совпадение с публичным v3.1.1 **не подтверждено**. Публичная основа выбрана после указания продолжать реализацию самостоятельно, а не выдана за идентичную входному EXE.

Архивы/EXE, ссылки с Dropbox access parameters, scratch-данные и локальные toolchains в Git не включены.

## Официальные replay samples: только измеренная схема

Из `krxclient/krx-replays` исследованы два файла:

| Sample | Размер | SHA-256 | Frames |
|---|---:|---|---:|
| `069` | 16967 | `9ea981d09031d6bf9f9594b1e2ce455311a2a90cdf283a26fb5c95412f06b99b` | 1895 |
| `003` | 74719 | `335f12a54417748e14220327dce938ce7acf2630c830ca01579b72441d16d205` | 7248 |

Наблюдается LE32 length + zlib-compressed JSON, корень `info` / `replay_local`, десять input fields и `pos{x,y}`. Полный начальный physics world, ticks и dummy track в этих samples отсутствуют. Это не доказательство совместимости всех форматов Ultimate 1.32. Текущий Malinka replay format отдельный; официальный importer/vault ещё не завершён.

## Лицензии и атрибуция

- `LICENSE` сохраняет MIT notice BestProject (2026).
- `license.txt` сохраняет исходные Teeworlds/DDRace/DDNet zlib-style notices и notices assets/fonts/readerwriterqueue. Pasta распространяет ту же inherited notice; происхождение модифицированных компонентов указано отдельно, без заявления об официальном KRX.
- Notices в `src/engine/external/`, `data/` и SDK сохранены. На данные/шрифты распространяются их собственные условия, а не одна общая MIT-лицензия.
- `src/base/hash_libtomcrypt.cpp` содержит public-domain SHA-256 adaptation с исходной атрибуцией; JSON parser сохраняет BSD notice. Никаких KRX EXE/DLL/resources в пакет исходников не добавлено.

Собственная сборка не получает `BESTCLIENT_OFFICIAL_CODE`. Ни чужие credentials, ни подтверждение официального статуса не воссозданы. Remote `fake` badge — отдельная неразрешённая зависимость внешнего сервиса; см. `MALINKA_STATUS.md`.
