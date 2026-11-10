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
- `tas.cpp`: отдельный мир recording, main/dummy input tracks, 50 TPS, sliced rewind/validation, playback input domain separation, map hash/physics checks, path/markers/events/sound, resource limits и safe saves. Это собственный `MALINKATAS2`, **не официальный KRX Ultimate format**. Отсутствуют importer/vault и replay fake-aim transformation.
- `simulation_step.h`: общий checked direct-input → increment tick → predicted-input → world tick. Direct inputs сортируются по client ID; указатели entities переопределяются после мутаций. Отказ должен остановить/отбросить соответствующую private simulation.
- `prediction/gameworld.*`, `prediction/entity.*`, `collision.*`: независимые config/tunings/mapbugs/PRNG/layers/door data, COW collision, detached entity clones, exception-safe partial clone cleanup, mixed ordinary/private-copy lifetime handling. Нельзя отправлять worker потоки в live map/world.
- `misc.cpp`: настоящий parallel prediction benchmark, до 16 workers / memory bound, 15 s deadline, join/cancellation и собственные snapshots. **Сам benchmark control в игре не запускался.**
- `players.cpp`, `tclient/trails.cpp`, `visuals.cpp`: local tee/hook/weapon/frozen/trail colors, bounded tile outlines, aim/trajectory/self-hit ESP, features HUD/watermark/notifications. Не выдаются за точную отрисовку KRX.
- `items.cpp` → `RenderBulletLine`: **реальный projectile renderer consumer**, не только HUD label. 64 сегмента за последние 2 s, tune-zone curvature/speed и интерполяционное время фактического projectile. Self-only требует достоверного owner; неизвестный owner не считается своим. Неидентичная KRX trail length/style — явное ограничение.
- `RenderFrozenHud`: bounded rows/size, имена/ID/freeze time, skins и team filter. Используется только известное extended freeze state; отсутствие данных не выдаётся за unfrozen. Last-unfrozen teammate разрешён только для normal nonzero teams, с известным состоянием всех активных teammates. Компоновка renderer не наблюдалась.

## Что ещё отсутствует, даже если нет отдельного gated config

- Полный Grenade saver, KRX 1.32 Fent Light tiles / jump-double-jump visuals / automatic grenade-shotgun-laser priority и точная smooth-camera parity.
- Официальный replay importer, vault fetch/sync и полное воспроизведение KRX 1.32 binary replay semantics/initial world.
- `!help` / `!skin` command-system parity.
- Часть movement/prediction/advanced Avoid/Fent и dummy-fly функций.
- Malinka Discord RPC profiles/custom app ID wiring; официальные IDs не выдуманы.
- Внешняя авторизация нормального BestClient badge.
- Windows/MSVC full-client build и реальный renderer/gameplay, panic/navigation/search/input-release/dummy/TAS regressions, two-install update/rollback и second-client remote badge observation.

После KRX 1.32 появились другие функции. Pilot/Song HUD (1.36), advanced dummy modes (1.35), perfect silent/remove-useless/showall automation (1.34), F3/F4/player indicators (1.33) не считаются обязательной 1.32 parity только из-за наличия в текущей документации.

## Явно недоступные controls (72)

Это **промежуточный gating**, а не согласованное сокращение исходного запроса. Реестр: `src/game/client/components/malinka/capabilities.h`; UI показывает причину и не включает такой control. `malinka_status` печатает ограничения в F1. Не все отсутствующие возможности имеют config, поэтому 72 — не число незавершённых игровых функций и не процент готовности.

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
| `MalinkaTasReplayVaultAutoSync` | Replay vault synchronization is not implemented. |

## Измеренные проверки

- Native GCC 12.2 Release: `game-client` и `bestclient-updater` собраны и слинкованы, full build7 **179/179, exit 0**; после последних исходников/metadata/notice fixes incremental build8 + `package_zip` **6/6, exit 0**. Окончательные команды/результаты — в HANDOFF.
- Syntax проверка simulation-1: **30 PASS / 0 FAIL, exit 0**. Реальная сборка дополнительно компилирует связанные Best/DDNet модули.
- ASan + UBSan pure I/O/input/package logic, повторный run3: **9448 checks / 0 failures, exit 0**. Replay format/checksum/path aliases/backups, hostile ZIPs, input counter parity/domain transitions, process-only panic latch. Это не keyboard/UI тест.
- ASan + UBSan actual physics harness run7: **8357 checks / 0 failures, exit 0**. Настоящие `coverage.map` 50×50 и `Tutorial.map` 2210×110; 362 обработанных allocation failures; serial/parallel copies и ticks после unload оригинала. 384 ticks/map сравнивают canonical helper с отдельным Best reference ordering, включая reverse primary/dummy order. Проверены duplicate/missing IDs, absent dummy и tick-overflow refusal. Сравниваются character states, **не полные world bytes**; остальные linked client modules не полностью инструментированы.
- Windows-target updater: Zig/Clang 19 `x86_64-windows-gnu`, updater + public-domain SHA-256 + bundled zlib слинкованы в PE32+ x64 GUI EXE, **exit 0**. Это не MSVC build, не полный Windows client и не Windows runtime test. Zig `-fsyntax-only` давал tool-level `FileNotFound`; реальная object compilation и link прошли.
- SDK bootstrap: реальный tarball hash проверен, все SDK files извлечены в отдельную новую директорию, **exit 0**. Dependency directory не включён в Git.
- Actual DDNet launch с SDL dummy завершился process exit 0, **но OpenGL/graphics initialization FAILED**. Это **не успешный renderer/game smoke**.
- TSan, gameplay, network replay equivalence, UI screenshots и remote badge **не проверены**. Build marker/SHA-256 не являются signature и не доказывают сохранность всех функций.

## Кратчайшие следующие проверяемые шаги

1. Завершить native incremental build после последних исходников. Получить Windows/MSVC artifact из build pipeline; не приравнивать cross-compiled updater к нему.
2. На Windows 11 x64 в отдельной install/profile: запустить actual renderer, открыть `malinka` и каждый subsection, отключить/включить helpers, проверить key release и dummy slot switching.
3. Нажать `Insert` на открытой странице, проверить settings search/старую страницу, reconnect (остаётся скрыто), полный process restart (возвращается). Убедиться, что panic не отключает функции незаявленно.
4. На контролируемом сервере сравнить TAS main+dummy recording/rewind/save/load/playback; `/showall` включить вручную до capture. Проверить переключатели/двери/теле/tune zones и смерть participants. Нельзя считать validation позиций доказательством идентичности полной серверной физики.
5. Подготовить два Malinka versions в разных disposable installs. Запустить checked updater, проверить сохранение config/replay/assets, bad hash/platform/path rejection, forced move failure/rollback и запуск обновлённого клиента. Не применять к единственной пользовательской установке.
6. Для exact KRX parity: в Windows VM проверить SHA-256 исходного EXE, зафиксировать фактическую module base/SizeOfImage/path/version; записать UI/config types/defaults и input/output traces на тех же local maps; экспортировать replay каждой опции. Никакие file RVA не превращать в runtime VA без измеренной module base. Не угадывать offsets/signatures/prototypes.
7. Для badge нужна отдельная поддержка fork внешним сервисом BestClient и наблюдение из немодифицированного клиента. Локальная подмена и строка версии не решают это требование.
