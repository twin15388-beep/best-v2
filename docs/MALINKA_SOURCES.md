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

### Avoid 0.1.1: independent repair, not recovered original code

`avoid_policy.h` и переписанный bot consumer `avoidfreeze.cpp` реализуют собственный bounded UCT/constant-input search по семантике зафиксированных docs. Source blobs для `features/goresbot/legit.md`: `bb5e2342c09f13002bc2b98848ec6baa203b8b98`; `blatant.md`: `bdc356e2aa2e40234ea89145df6b84f39ed1dfad`. Нормализация весов, budgets, candidate ordering и NSIF revalidation являются решениями этой адаптации, а не измеренными constants закрытого EXE. Сохранённые movement probes всё ещё атрибутированы Pasta. Точная 1.32 UI/ranges/physics equivalence не заявляется.

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

## Community replay vault и измеренная схема

Источник: <https://github.com/krxclient/krx-replays>, snapshot **`ed7bfe99968c3107d05e57c0c06c3ef3e90f3493`**, Git tree **`c8fe016b2b0f03056c36c3b86e79e5f90500bcc8`**,333 entries, non-truncated. Archive:

`https://codeload.github.com/krxclient/krx-replays/zip/ed7bfe99968c3107d05e57c0c06c3ef3e90f3493`

Повторно скачан и проверен: **11528399 bytes**, SHA-256 **`cc5c7f41380decf5cbcacc9aab3c0dc50a80e0565e6e4dff778f99d21de0ca02`**.179 `.tas`,11492809 expanded file bytes,max member476828; ZIP CRC clean. JSON max22260071bytes/45958frames. Это текущий community snapshot, **не доказанный набор именно KRX1.32**.

| Sample | Размер | SHA-256 | Frames / Git blob |
|---|---:|---|---|
| `069` | 16967 | `9ea981d09031d6bf9f9594b1e2ce455311a2a90cdf283a26fb5c95412f06b99b` | 1895 / `b9895f2c15e077bf9e40e5a894fb79d11db3ec35` |
| `003` | 74719 | `335f12a54417748e14220327dce938ce7acf2630c830ca01579b72441d16d205` | 7248 / `8c75dfc1bac99232d6b59ac93f49b15d87c1041f` |

Наблюдается **LE32 expanded length по file offset0** + один zlib stream/JSON, `info`,10 named input fields и `pos{x,y}`. Это file schema, не game object offsets. Полный initial physics world,map SHA,ticks/position phase отсутствуют; они не выдумываются. Roots:152 `info/replay_local`;25 с `conditions_local`;1 с conditions+both tracks;1 dummy track без conditions.

Строгий C++ decoder до normalization принимал49/179;102 plain-root файла были отклонены только из-за нулевого aim,1 из-за `INT_MIN` aim,27 из-за неподдержанных roots. После **узкой явной normalization** `(0,0)→(0,-1)` actual ASan+UBSan+LSan batch принимает **151/179**, отклоняет **28**, обрабатывает1502 zero-aim frames. Independent Python hashes сверяют все10inputs/tick/position, оригиналы не меняются. Другие invalid inputs не clamp-ятся; conditions/dummy roots не игнорируются.

Provenance normalization: DDNet **18.9.1**, commit **`3b4b69c13365f4efd76b619c3762b3a8306569b4`**, файл [`src/game/client/prediction/entities/character.cpp`](https://github.com/ddnet/ddnet/blob/3b4b69c13365f4efd76b619c3762b3a8306569b4/src/game/client/prediction/entities/character.cpp), `OnPredictedInput` lines514–515 и `OnDirectInput` lines537–538. Оба меняют нулевой target на `(0,-1)`, как текущая Best prediction. Это измеренное правило DDNet input canonicalization, **не установленная реализация всего закрытого KRX**.

Importer оставляет записи preview-only до полной current-world physics validation; native `MALINKATAS2` — отдельный формат. Vault sync использует измеренный GitHub commits/main `Accept: application/vnd.github.sha` (40 lowercase hex), затем SHA-pinned archive, сохраняет только raw snapshots. HTTPS/CRC/hash/limits не равны авторизации или оригинальной game/server equivalence. Точные gates и проверки: [MALINKA_STATUS.md](MALINKA_STATUS.md).

## Лицензии и атрибуция

- `LICENSE` сохраняет MIT notice BestProject (2026).
- `license.txt` сохраняет исходные Teeworlds/DDRace/DDNet zlib-style notices и notices assets/fonts/readerwriterqueue. Pasta распространяет ту же inherited notice; происхождение модифицированных компонентов указано отдельно, без заявления об официальном KRX.
- Notices в `src/engine/external/`, `data/` и SDK сохранены. На данные/шрифты распространяются их собственные условия, а не одна общая MIT-лицензия.
- `src/base/hash_libtomcrypt.cpp` содержит public-domain SHA-256 adaptation с исходной атрибуцией; JSON parser сохраняет BSD notice. Никаких KRX EXE/DLL/resources в пакет исходников не добавлено.

Собственная сборка не получает `BESTCLIENT_OFFICIAL_CODE`. Ни чужие credentials, ни подтверждение официального статуса не воссозданы. Remote `fake` badge — отдельная неразрешённая зависимость внешнего сервиса; см. `MALINKA_STATUS.md`.


## Дополнительная сверка Avoid controls по изображениям

Повторно просмотрены PNG из того же pinned docs commit `8c193ca2a27cc7ba5d4e2d6ad7686c6e99910c43`:

- `images/legit-menu.png`, Git blob `6199bec3090b470110bef5a2b97d9c78360e005d`, 196707 bytes: видны **Quality 1000**, **Life Priority 298**, а также Check Ticks 7, Randomness 6, Direction Priority 235, Hook Priority 504. Это значения на изображении, **не доказанные defaults или полные ranges бинарника 1.32**.
- `images/blatant-menu.png`, blob `119c7153e63f741367d874db31593775160c5daf`, 245167 bytes: видны Check Ticks 26, Kick in Ticks 27, Unfreeze Ticks 26, Segments 12, FOV 360. Там Auto Aim и Aim Assist — отдельные checkboxes; в этой адаптации пока взаимоисключающий mode. Точное совместное поведение оригинала не установлено.
- `images/avoid-menu.png`, blob `67459a7beb686df98252f088361fa2760dc0fcf9`, содержит Pilot Bot, появившийся позже 1.32. Поэтому screenshot не выдаётся за точное меню именно 1.32.

Для независимого Legit planner расширены Quality **1..1000** и Life Priority **0..1000**, чтобы как минимум принимать измеренные 1000/298. Это выбранные bounded ceilings, не восстановленные original maxima. Quality действительно задаёт число rollout-ов (1000 максимум), tree имеет не более1001 nodes; Life выше100 не clamp-ится. Старые defaults16/13 и относительный scale ранее сохраняемых life weights не изменены. Большие Quality/Check Ticks могут задерживать кадры; UI tooltip предупреждает об этом. Ни performance на Windows, ни exact KRX decision parity этими изображениями не доказаны.


## Provenance доставленного 0.1.1 source archive

Companion `bestclient-malinka-source.tar.gz` получен **без перепаковки** из Windows artifact13807725269/run42908808859/source`facfc8563106d5421de5c73f861e2e5df342f6ad`:57789009bytes,SHA256 `d605a975ecbcf0e8c4ffd3b1ab4abbe6a8d7e523796d2a38c3f8d97d501e3c7a`, совпал с `source_sha256` в manifest доставленного ZIP. Сверены все2375paths против exact committed Git tree:822archive blobs совпадают побайтово,1553text files отличаются **только LF→CRLF**. Проверка допускает ровно это преобразование, не произвольную whitespace normalization; бинарники, прочие content differences, пропуски/дубликаты/чужиеpaths/links отвергаются. Это наблюдение Windows archive, не proof byte-for-byte binary reproducibility. Для исходных LF bytes и Linux checkout использовать указанный commit Git; SHA Cargo.lock в документации относится к tracked LF blob, не CRLF bytes внутри этого tar. Позднейшие docs/workflow cleanup не включены в этот source archive; production sources/CMake/Cargo.lock после него не менялись.
