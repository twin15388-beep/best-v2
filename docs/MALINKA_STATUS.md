# Malinka: фактический статус и незавершённые требования

Дата: **2026-11-10**. Версия fork **0.1.0**, экспериментальная адаптация. **Запрос «все функции KRX Ultimate 1.32» ещё не выполнен.** Наличие вкладок, config variables и успешной компиляции не означает полного переноса или проверки в игре.

## Критерии исходного запроса

| Требование | Фактический статус |
|---|---|
| Вкладка `malinka` и все функции KRX Ultimate 1.32 | Вкладка и набор реальных consumers добавлены; перенос неполный, точная эквивалентность закрытому KRX не установлена. Отсутствующие функции ниже. |
| Обновления без потери Malinka | Официальный канал замены EXE заблокирован; написаны отдельный fork channel, checked installer/rollback и generator пакетов. Опубликованного и end-to-end проверенного канала релизов пока нет. Произвольные будущие upstream updates не гарантируются. |
| Не получать `fake` у других игроков | **Не выполнено.** Требуется авторизация fork внешним сервисом индикаторов и проверка из второго немодифицированного BestClient. Локальная картинка/возврат `false` не подменяют этот результат. |
| `Insert` скрывает cheat UI до «перезайти» | Session latch и проверки menu navigation/search/dispatch реализованы. Принята интерпретация **до полного перезапуска процесса**, не reconnect. Это **UI-only**: функции не отключаются. В реальном UI ещё не проверено. |

## Реальные реализации в коде, не игровые гарантии

- `aimbot.{h,cpp}` / `controls.cpp`: hook и weapon targeting, FOV/priority/friend filtering, silent/visible aim, bounded scans, auto fire scheduling. Grenade movement prediction и точная KRX-эквивалентность отсутствуют.
- `controls.cpp`, `autoaled.cpp`, `laserunfreeze.cpp`, `autoedge.cpp`: реальные input consumers для basic movement/helpers, nearest-hook/balance/pixelwalk, rehook, auto Aled, laser/shotgun self-hit поиска, basic edge detection. Уровень алгоритмов отличается от официального KRX; расширенные режимы не дописаны.
- `avoidfreeze.cpp`: basic Legit/Blatant candidates; независимые physics copies. Невозможность прогнозирования отличена от безопасного движения: **UNAVAILABLE / SAFE / DANGER**. Отказ capture/alloc не разрешает принять safe candidate. Много настроек этих режимов ещё gated.
- `fent.cpp`: локальный planner, bounded candidate slices, finish/flowfield/path drawing, replay export. Planning и исполнение используют один captured dummy input, записываются обе дорожки. Изменение slot/participant identity останавливает текущий run. Полная KRX Fent Light/jump-render/weapon-priority/unfreeze parity отсутствует.
- `tas.cpp`: отдельный мир recording, main/dummy input tracks, 50 TPS, sliced rewind/validation, playback input domain separation, map hash/physics checks, path/markers/events/sound, resource limits и safe saves. Это собственный `MALINKATAS2`, **не официальный KRX Ultimate format**. Добавлен строгий importer измеренной zlib/JSON-схемы и raw vault sync; replay fake-aim transformation отсутствует. Подробные границы ниже.
- `simulation_step.h`: общий checked direct-input → increment tick → predicted-input → world tick. Direct inputs сортируются по client ID; указатели entities переопределяются после мутаций. Отказ должен остановить/отбросить соответствующую private simulation.
- `prediction/gameworld.*`, `prediction/entity.*`, `collision.*`: независимые config/tunings/mapbugs/PRNG/layers/door data, COW collision, detached entity clones, exception-safe partial clone cleanup, mixed ordinary/private-copy lifetime handling. Нельзя отправлять worker потоки в live map/world.
- `misc.cpp`: настоящий parallel prediction benchmark, до 16 workers / memory bound, 15 s deadline, join/cancellation и собственные snapshots. **Сам benchmark control в игре не запускался.**
- `players.cpp`, `tclient/trails.cpp`, `visuals.cpp`: local tee/hook/weapon/frozen/trail colors, bounded tile outlines, aim/trajectory/self-hit ESP, features HUD/watermark/notifications. Не выдаются за точную отрисовку KRX.
- `items.cpp` → `RenderBulletLine`: **реальный projectile renderer consumer**, не только HUD label. 64 сегмента за последние 2 s, tune-zone curvature/speed и интерполяционное время фактического projectile. Self-only требует достоверного owner; неизвестный owner не считается своим. Неидентичная KRX trail length/style — явное ограничение.
- `RenderFrozenHud`: bounded rows/size, имена/ID/freeze time, skins и team filter. Используется только известное extended freeze state; отсутствие данных не выдаётся за unfrozen. Last-unfrozen teammate разрешён только для normal nonzero teams, с известным состоянием всех активных teammates. Компоновка renderer не наблюдалась.

## Что ещё отсутствует, даже если нет отдельного gated config

- Полный Grenade saver, KRX 1.32 Fent Light tiles / jump-double-jump visuals / automatic grenade-shotgun-laser priority и точная smooth-camera parity.
- Полная совместимость всех KRX 1.32 replay schemas, включая conditions/dummy roots и восстановление original initial world. Реализованные sample-schema importer и raw vault sync не означают эту совместимость.
- `!help` / `!skin` command-system parity.
- Часть movement/prediction/advanced Avoid/Fent и dummy-fly функций.
- Malinka Discord RPC profiles/custom app ID wiring; официальные IDs не выдуманы.
- Внешняя авторизация нормального BestClient badge.
- Реальный Windows11 renderer/gameplay, panic/navigation/search, online dummy/TAS regressions, full updater window/process restart и second-client remote badge observation. Windows/MSVC full build уже прошёл; локальные snapshots/installer checks не заменяют игровые проверки.

После KRX 1.32 появились другие функции. Pilot/Song HUD (1.36), advanced dummy modes (1.35), perfect silent/remove-useless/showall automation (1.34), F3/F4/player indicators (1.33) не считаются обязательной 1.32 parity только из-за наличия в текущей документации.

## Импорт KRX и raw replay vault: что именно работает в коде

- `krx_replay_io.h`: LE32 expanded length + один zlib stream, строгие JSON keys/types/UTF-8/limits, transactional output и scoped ownership всех C-parser allocations. Никакого угадывания map hash, initial world, ticks, dummy или physics version.
- Только `(TargetX,TargetY)=(0,0)` преобразуется в `(0,-1)`: это правило **реального DDNet 18.9.1** и текущей Best prediction. Количество кадров сообщается пользователю. Другие недопустимые inputs не clamp-ятся. Оригинал не меняется.
- Импорт сначала **PREVIEW ONLY**. `Validate Replay` симулирует **весь** track из текущего мира, проверяя единую pre-input либо post-input гипотезу позиций с tolerance 0.5; нельзя выбирать фазу отдельно каждый tick. Только полный успех переводит буфер в Malinka pre-input positions с hash текущей map/Best physics. Это не реконструкция original KRX world и не server guarantee.
- `Load Selected Replay` загружает и единственный/index-zero/повторно выбранный файл. Неуспешная загрузка не заменяет прежний буфер. Pending import нельзя воспроизводить или сохранить как trusted replay. Конверсия сохраняется только **под новым именем вне `vault/`**; исходный KRX file и все vault snapshots защищены от Save, включая case aliases.
- `replay_vault.{h,cpp}`: реальный asynchronous GitHub HEAD → pinned archive → engine cache job. `Sync Now`/cancel/status, startup/off→on auto-sync (default **off**), 60 s throttle и 120 s HTTP timeout. HTTPS/TLS принудительны независимо от insecure testing config. Не читается HTTP status/result у ERROR/ABORTED.
- `replay_vault_io.h`: CRC/path/type/resource checks, native bounded traversal, hash verification уже существующего cache, no-replace publication в `malinka/replays/vault/<sha>`. Work только в принадлежащих job `malinka/vault-cache/staging-*`; cleanup не удаляет user files/published snapshots и может оставить staging при устойчивой ошибке. ZIP ≤32 MiB, member ≤4 MiB, expanded ≤64 MiB, ≤4096 entries/1024 replays/depth6. Общие updater defaults **не уменьшены**.
- Vault сохраняет **raw community files**; sync ничего автоматически не конвертирует/загружает/воспроизводит. Текущий community `main` не выдан за набор именно Ultimate 1.32.

## Последние измеренные builds/checks

**Ни один пункт ниже не является Windows gameplay/renderer или внешней badge-проверкой.**

| Проверка | Наблюдаемый результат / границы |
|---|---|
| Свежий GitHub CI [42885117777](https://github.com/twin15388-beep/best-v2/actions/runs/42885117777), `2d00f1f40fb150656c8e916a758b544071619dfd` | Windows2022/MSVC x64 и Ubuntu24.04 **SUCCESS**, full client/updater/package **включая importer/vault/zero-aim normalization**. Experimental artifacts, не published releases. Предыдущий e2fdaef run42875704110 уже не последний результат. |
| Повторный ordinary workflow [42887818609](https://github.com/twin15388-beep/best-v2/actions/runs/42887818609), `0a0eeed0b463ade41801f2e76c0aaca5e1f338e9` | **Windows/MSVC + Linux SUCCESS / exit0**, полный client/updater/package после удаления одноразового regression fixture. Production sources/CMake/Cargo.lock/workflow идентичны2d00f1f; bytewise binary/ZIP reproducibility не заявляется. |
| Native build18 до восстановления sandbox | `game-client bestclient-updater package_zip` **exit0**, включал importer/vault и явную Load/read-only vault Save правку, **до zero-aim normalization**. Старые logs/toolchains/builds после восстановления среды отсутствуют. |
| Текущая ASan+UBSan+LSan decoder проверка | **2 366 910 checks / 0 failures / exit0**: 94 fixtures + все179 pinned vault files, independent Python hashes всех named inputs/positions; **151 accepted / 28 rejected**,1502 normalized zero-aim frames,35 C/3 C++ allocation failures. Parsing-only. |
| Предыдущая actual-world/TAS lifecycle проверка | **10 165 / 0 / exit0**,2 real maps,362 allocation failures. Actual load/validate/playback-input methods, pre/post imports, successful converted Save/readback, original/vault Save refusal. До новой normalization; не server/renderer. |
| Предыдущая actual HTTP/jobs проверка | **26 / 0 / exit0**, real GitHub API/archive, cold/repeat179 cache, local replay preservation, pending069 playback block, failed HTTP, duplicate/cooldown/cancel/shutdown. Auto-sync toggle и cache-job-stage cancellation отдельно не исполнены. |
| Свежая vault I/O fault проверка | **6567 / 0 / exit0**,29 invalid ZIPs +3 valid sources,179 побайтовых сравнений pinned raw files,8 отмен на I/O checkpoints/12completed,716 allocation positions/691 injected throws. Cold/repeat/tamper, native no-replace, user sentinel/link protection, cleanup. Это не component auto-sync/job-cancel runtime test. |
| Свежая input/panic/replay helper проверка | **107206 / 0 / exit0**,8192 input-stream scenarios/324 finite-aim cases, pure process latch, safe path/backups/.new refusal/CRC transactional readback. Не физическая клавиша/меню/игра. |
| Предыдущая vault I/O fault проверка | **1707 / 0 / exit0**,21 ZIP fixtures,179 byte-compared files,876 allocations/346 probes/303 injected throws. Native traversal вместо terminating libstdc++ iterator; bounded cleanup retry2. |
| Предыдущие decoder/logic/generator проверки | Decoder **18 836 / 0** (93 fixtures +2samples); pre-normalization all179 **19 146 / 0**,49 accepted/130 rejected; package/input logic **9459 / 0**; Python generator **50 / 0**, все exit0. Это исторические results, не новые reruns. |
| **Actual новые пакеты из `2d00f1f`** | [One-shot CI42887324042](https://github.com/twin15388-beep/best-v2/actions/runs/42887324042) **SUCCESS / ASan+UBSan+LSan exit0**: Linux1026files **2785/0**; Windows1038files **2821/0**. Actual ZIP/default limits/manifest/source SHA → C++ installer → forced rollback/readback/config+replay+vault preservation/backup/tamper refusal. **Обе проверки на Linux file operations**; это не Win32 updater UI/process/restart/deployed channel. |

Текущая среда содержит g++/Python/gh, но не старые CMake/Ninja/Rust/SDL/GL SDK/build outputs. Новая полная local native сборка не выполнена; **full MSVC/Linux исполнены в CI**. Основные Windows/Linux build commands и isolated decoder команда — в BUILD/HANDOFF. Cargo.lock SHA-256 остаётся `760288176362ad39458b4f01673df5ad8729809cd1e0178da0573ef728c6a904`.

## Явно недоступные controls (71)

Это **промежуточный gating**, а не согласованное сокращение исходного запроса. Реестр: `src/game/client/components/malinka/capabilities.h`; UI показывает причину и не включает такой control. `malinka_status` печатает ограничения в F1. Не все отсутствующие возможности имеют config, поэтому 71 — не число незавершённых игровых функций и не процент готовности.

| Config field | Причина |
|---|---|
| `MalinkaAimbotGrenadeMovePrediction` | This prediction control has no runtime consumer yet. |
| `MalinkaAutoEdgeDetectDeath` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoEdgeDetectFreeze` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoEdgeDetectTele` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoEdgeDetectionRange` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoEdgeExplorationDepth` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoEdgePopulationSize` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoEdgeSequenceLength` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoEdgeTopK` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAutoJumpSaveMode` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantAfkProtection` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantAfkTime` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantAimbotMode` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantAimbotSegments` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantCheckTicks` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantDeath` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantDirection` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantKickInTicks` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantNsif` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantPlayerPrediction` | This prediction control has no runtime consumer yet. |
| `MalinkaAvoidBlatantSafeAimTracking` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantTeles` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantTrackPoint` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantUnfreeze` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantUnfreezeTicks` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantVisualAimbot` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidBlatantVisualTrackPoint` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidFentPlayerPrediction` | This prediction control has no runtime consumer yet. |
| `MalinkaAvoidFentTicks` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidFentUnfreeze` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidFentUnfreezeRadius` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidLegitHookPriority` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidLegitLifePriority` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidLegitPlayerPrediction` | This prediction control has no runtime consumer yet. |
| `MalinkaAvoidTileAutoDrag` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaAvoidTileTrackPoints` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaChangeNameOnFinish` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaChangeNameOnFinishName` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaChangeNameOnFinishRandom` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaCursorInSpec` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaDiscordRpc` | Malinka RPC profiles are not connected; application IDs are unverified. |
| `MalinkaDiscordRpcIdCustom` | Malinka RPC profiles are not connected; application IDs are unverified. |
| `MalinkaDiscordRpcMode` | Malinka RPC profiles are not connected; application IDs are unverified. |
| `MalinkaDummyFly` | Only dummy hammer was present in the donor; fly logic is incomplete. |
| `MalinkaFancyChatFont` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaFastInput` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaFastInputOthers` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaGhostMove` | Packet-level ghost move is not implemented. |
| `MalinkaGhostMoveDirection` | Packet-level ghost move is not implemented. |
| `MalinkaGhostMoveHook` | Packet-level ghost move is not implemented. |
| `MalinkaGhostMoveHookClosest` | Packet-level ghost move is not implemented. |
| `MalinkaGhostMoveJump` | Packet-level ghost move is not implemented. |
| `MalinkaHideBubble` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaHookRide` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaIgnoreReplayWarnings` | Checksum, identity and physics failures cannot be ignored. |
| `MalinkaJetRide` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaModDetector` | No verified moderator/warning lists supplied; name guesses are disabled. |
| `MalinkaModDetectorLeave` | No verified moderator/warning lists supplied; name guesses are disabled. |
| `MalinkaModDetectorNames` | No verified moderator/warning lists supplied; name guesses are disabled. |
| `MalinkaModDetectorWarn` | No verified moderator/warning lists supplied; name guesses are disabled. |
| `MalinkaModDetectorWarnLeave` | No verified moderator/warning lists supplied; name guesses are disabled. |
| `MalinkaPredictionDeathTile` | This prediction control has no runtime consumer yet. |
| `MalinkaPredictionHeart` | This prediction control has no runtime consumer yet. |
| `MalinkaPredictionMoveRestriction` | This prediction control has no runtime consumer yet. |
| `MalinkaPredictionPlayers` | This prediction control has no runtime consumer yet. |
| `MalinkaSendAds` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaSpoofVersion` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaSpoofVersionNr` | Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md. |
| `MalinkaTasDrawPredictionPathColorOthers` | This prediction control has no runtime consumer yet. |
| `MalinkaTasFakeAim` | Replay aim transformation and equivalence validation are not implemented. |
| `MalinkaTasFakeAimMode` | Replay aim transformation and equivalence validation are not implemented. |

## Реальное исполнение

Actual Linux DDNet запускался ранее с isolated HOME/XDG и SDL dummy: process exit0, **graphics/OpenGL initialization FAILED**. Это не renderer smoke success. TSan, Windows11 runtime, online gameplay/dummy, panic/navigation/search и remote badge **не проверены**. Build marker и SHA-256 не являются signatures. Старые `.artifacts` harnesses/logs не пережили восстановление среды; новые decoder/vault/input logs и actual-package CI annotations перечислены в HANDOFF. Скачать Actions artifacts в sandbox не удалось (redirect на недоступный blob-host); поэтому actual-package regression реально выполнена внутри GitHub runner, а не объявлена по недоступному локальному ZIP. Одноразовый workflow удалён после success, постоянный test suite не добавлен.

## Кратчайшие следующие проверяемые шаги

1. Сохранены production source2d00f1f и восстановленный workflow0a0eeed; **оба full MSVC/Linux CI SUCCESS**, actual-package regression2d00f1f выше. Дальше — незавершённые1.32consumers и реальный Windows runtime; package integration checks не выдавать за gameplay или полный перенос.
2. На Windows 11 x64 в отдельной install/profile: запустить actual renderer, открыть `malinka` и каждый subsection, отключить/включить helpers, проверить key release и dummy slot switching.
3. Нажать `Insert` на открытой странице, проверить settings search/старую страницу, reconnect (остаётся скрыто), полный process restart (возвращается). Убедиться, что panic не отключает функции незаявленно.
4. На контролируемом сервере сравнить TAS main+dummy recording/rewind/save/load/playback; `/showall` включить вручную до capture. Проверить переключатели/двери/теле/tune zones и смерть participants. Для импорта: Sync Now → Reload list → Load Selected Replay → preview-only → matching map/start → Validate Replay → Save с новым именем вне vault/ → readback; отрицательные случаи должны сохранять исходный buffer/source/snapshot. Нельзя считать validation позиций доказательством идентичности полной серверной физики.
5. Подготовить два Malinka versions в разных disposable installs. Запустить checked updater, проверить сохранение config/replay/assets, bad hash/platform/path rejection, forced move failure/rollback и запуск обновлённого клиента. Не применять к единственной пользовательской установке.
6. Для exact KRX parity: в Windows VM проверить SHA-256 исходного EXE, зафиксировать фактическую module base/SizeOfImage/path/version; записать UI/config types/defaults и input/output traces на тех же local maps; экспортировать replay каждой опции. Никакие file RVA не превращать в runtime VA без измеренной module base. Не угадывать offsets/signatures/prototypes.
7. Для badge нужна отдельная поддержка fork внешним сервисом BestClient и наблюдение из немодифицированного клиента. Локальная подмена и строка версии не решают это требование.
