# Передача задачи: внешний помощник / исторический BestClient + malinka

Обновлено **2026-11-10 UTC**. Repo `/home/user/best-v2`, фиксированная ветка **`arena/733fc6a9-best-v2`**. **Активная задача теперь — отдельный Avoid над неизменённым официальным клиентом. Он ещё НЕ реализован.** Добавлен и реально проверен отдельный **0.1.0-diagnostic** помощник; не выдавать его за работающий Avoid. Исторический изменённый клиент **facfc8563106d5421de5c73f861e2e5df342f6ad /0.1.1** ранее доставлен; его production src/CMake/data/lock в этой работе не менялись. **Полный первоначальный запрос также не выполнен.**

## Текущий запрос / выбор пользователя

Пользователь: **«а вариант по верх настоящего клиета сделать чтобы не вмешиваться в файлы или программу которая делает avoid freeze чтобы был офиц клиент»**. На выбор read-only process + ordinary input против screen-only ответил **«как знаешь»**. Выбрана read-only архитектура без изменения файлов/памяти клиента и DLL injection. Обнаружен штатный Mumble Link, но он даёт только render telemetry; нет подтверждённой physics/input integration. Пользователю явно сказано, что диагностический этап не заменяет реальный Avoid. Исходники повторно не просить, offsets/signatures/совместимость не выдумывать, подмены badge нет. Ответы — `[Aero]` первой строкой, **бро**, русский.

### Что реально сделано сейчас

- `tools/malinka-helper/`: независимый C++20/MSVC/x64 Win32 GUI, topmost/resize/DPI, async/cancellable inspection, explicit unmodified EXE launch, RO process/module/PE reads, standard MumbleLink consumer, new/no-replace JSON outside client directory. Ни SendInput, ни process write/injection/hooks/debug privilege нет. Это не изменённый клиент и не DLL.
- `core.h`: единственный exact-image public profile, offsets персонажа/collision **nullopt**, в JSON `null`, automatic Avoid **BLOCKED всегда**. PE parser + safe RVA/raw translation, finite telemetry/tick wrap/offline/stale. Shared ABI не имеет writer PID/seqlock; double collect не доказывает цельный physics snapshot.
- `win32.cpp`: `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE)`, `ReadExact` rejects partial, module/header identity, ASLR-adjusted vtable, private-memory candidate search ≤512MiB/65536queries/1500ms/32candidates. Candidates **не validated CGameClient/world**. Session/process data — разовое измерение, не live snapshot. No-replace report/junction/client-directory refusal.
- `main_win32.cpp`: отдельное окно/CLI; focus/key injection отсутствуют, поэтому нет скрытого automatic input. Нормальные собственные config saves игры не отключаются. Выход помощника не закрывает клиент.
- `docs/MALINKA_EXTERNAL.md`, helper README/provenance: exact provenance, build/test commands, limits и acquisition recipe. Постоянный build-only workflow `malinka-companion-build.yml` отделён от game build; `malinka-build.yml` игнорирует helper-only paths. Временные audit/check workflows удалены после успеха; рецепт полностью остаётся в Git e8e1547.

### Измеренные build identity / источники

Public official Best v3.1.1 tag→**cf192e3fb8590dbe2e4f66a191aa6616beff6975**. ZIP asset**730984669**/**65165594bytes**/SHA256**51688c041453d989522658cf46ae21b08ef46b28786da204b82e46c5166d7059**. Внутри `bestclient-3.1.1-win64/DDNet.exe` **26206720bytes**/SHA256**50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f**, AMD64/SizeOfImage50638848. Primary vtable **fileoffset19898296/RVA19904440**, COL RVA24123696/type descriptor RVA25441720; первые8 function RVAs только fingerprint, не предполагаемые методы. RuntimeVA только от measured base. Нет PDB в этом asset. Acquisition **CI43002224833/check130460886361 SUCCESS**, metadata helper/provenance JSON.

**Совпадение с пользовательским Dropbox BestZIP120621904bytes НЕ установлено; другие EXE даже с именем v3.1.1 блокируются.** Read-only/header identity не доказывают genuine remote badge или отсутствие чужих модификаций процесса.

### Реально завершённые проверки / доставка

Pinned helper source **e8e1547f104c662e2b350a47f852609d3eaf1f61**. **CI43005315923/job130471217878 SUCCESS1m0s /watch exit0**, Windows2022/MSVC19.44.35229.0/SDK10.0.26100.0. Standalone `cmake -S tools/malinka-helper -B build-helper-release -A x64` и `cmake --build build-helper-release --config Release --parallel 2`: **exit0 оба**. Actual core+Win32 isolated fixture **compile0/run0**, **1080071 core checks/0failures**, **43 Win32 checks/0failures**. Дополнительно exact delivered standalone EXE: CLI/selfprocess/unknown-build/report-no-replace, real public official EXE **только как файл**/RTTI, no forbidden imports, GUI create/resize/self-PID inspect/topmost/close. Это synthetic messages, не physical keys; official game не запускался.

Linux g++12.2/CMake3.31.6/Ninja1.11.1.3 portable core configure/build **0**; ASan/UBSan/LSan core **compile0/run0/1080071/0**. Logs `.artifacts/external-helper/core-{configure-2,build-3,check-compile-3,check-run-3}.{log,exit}`; exact commands EXTERNAL. Первое configure1 из-за Ninja вне PATH исправлено. Native attempts43004322507 (fixture CMake backslash),43004678906 (GetWindowTextW не читает чужой EDIT),43004999743 (all runtime passed, strict source CRLF assertion) исправлены: normalized paths, WM_GETTEXT+timeout, helper-local `.gitattributes eol=lf`. Последний успех выше; не повторять старые ошибки/пакеты. Полные fixture sources/recipe: `git show e8e1547:.github/workflows/malinka-companion-check.yml`.

**Получен оригинальный diagnostic ZIP**, не relabeled game EXE: `build/delivery/malinka-helper-e8e1547/MalinkaHelper-0.1.0-diagnostic-windows-x64.zip`, **204963bytes**, SHA256**66c9685adf3a81a311c1fd6b984be62ef18376dd23c07eeb2e8cf547bd63c16d**. Внутри `MalinkaHelper.exe` **355840bytes**, SHA256**b0c0ad64259778b57e9b337e42a8de21169f7b4962f82a8c7359554cc5cd70a3**, AMD64/PE32+/GUI; все **11 source/build/provenance files byte-identical Git**, README/LICENSE/CHECKS/source SHA list. ZIP CRC/blob/whole/PE/source verified locally exit0, `.artifacts/external-helper/verified-delivery.json`. Archive unchanged после CI. Unreferenced bounded blob32b23ee1d12f1f3162756dcbd345c4b86d0e14ba — transport only, без binary commit/ref/release.

Dropbox **`/MalinkaHelper-diagnostic-e8e1547`**, folderid`id:dED5V7bj3cAAAAAAAAAAEw`, uploaded ZIP(`id:dED5V7bj3cAAAAAAAAAAFQ`,rev65ffddb3f65674b309220), README(`id:dED5V7bj3cAAAAAAAAAAFg`,9707bytes), SHA256SUMS(`id:dED5V7bj3cAAAAAAAAAAFA`,113bytes). Add/strict-conflict, readback list matched3 files; public viewer/download ZIP link создан (access params в Git не копировать). ZIP contenthash955def309e249cf9f4393125fbc9e0b080947fc18a206ebf07209215b3193008. Историческая 0.1.1 папка НЕ заменялась.

### Ближайший шаг / недостающие входные данные

**Не выполнены внешние Avoid/physics/map/tuning/input adapter и наблюдение badge со второго unmodified клиента. not verified in-game.** Нужен runtime report от реальной официальной сборки на карте: helper вне client folder → реальный PID → повторная проверка → новый JSON вне игры. Он даёт identity/root candidates, не готовую физику. Затем matching source/compiler layout/symbols и независимое подтверждение local ID/dummy/pos/vel/tick/freeze; bounded consistent map/tuning/collision snapshot с invalidation по смене карты/сессии, независимая симуляция, ordinary input с focus/physical-key conflict/watchdog/release, same-map freeze outcomes. Нельзя включать input по render-координатам или одному candidate pointer. Public source/independent work выполнять без повторного запроса исходников; diagnostic gating не считать согласованной заменой запроса.

### Важное исправление Git-state

На входе был local244d7d4/README modified/imported source untracked, push нового audit47ae2e1 отклонён non-fast-forward. Fetch показал, что **эта же ветка** уже remote35a6576. **Все2374remote paths measured byte-identical** рабочим файлам, missing/different0. Сделан двухродительский merge0f13c6a через merge-tree/index-only read-tree, **ни один рабочий файл не заменялся**,2375SHA256 unchanged; no reset/clean/checkout/force push/new branch. История35a6576 теперь реально включена, рабочее дерево стало tracked/clean до последующих task edits. Reports `.artifacts/external-helper/{remote-worktree-comparison,history-synchronization}.json`. НЕ повторять recovery/import из старой memory. Текущий HEAD смотреть git log/status. Game production diff против35a6576 (src/CMake/data/lock) пустой. Не связанные с helper unchanged game builds43002224696 и43004322607 также SUCCESSboth, но не feature/runtime proof и не новая доставка fork.

## Исторический запрос к изменённому клиенту (по-прежнему не весь выполнен)

Пользователь ранее: «доделывай… настройки avoid не такие… fake… insert… убери krx… замаскируй tas». Уточнение: **«Скрытие и вкладки и команд в консоли»**. Далее — сохранённые результаты исторической версии0.1.1, НЕ новый companion.

- Insert — **UI-only**: вкладки Malinka + локальные F1 suggestions/setting help скрываются, execution/binds/config/history/backlog и F2 не удаляются. Latch живёт до полного перезапуска процесса, reconnect его не сбрасывает.
- Маскировка файлов и подделка авторизации **не выполнялись**. Genuine externally visible Best badge остаётся нерешённым.
- Не просить исходники повторно. Самостоятельная адаптация — **не original/official/full KRX 1.32**. 60 remaining gated controls не являются согласованным сокращением запроса.
- Точный полный запрос, standing instructions и исторические данные сохранены ниже и в docs. Ответы: первая строка `[Aero]`, обращаться **бро**, русский.

## Измеренные результаты текущей 0.1.1

### 1. Полная native сборка

**CI42908808859 / source facfc85: SUCCESS, watch exit0.**

| Target | Job | Длительность | Результат |
|---|---|---|---|
| Windows2022 / MSVC x64 | `130153717950` |10m25s|client/updater/package SUCCESS|
| Ubuntu24.04 / Linux x64 | `130153718518` |8m43s|client/updater/package SUCCESS|

Выполненные commands, каждый exit0 на обеих платформах (через `python scripts/malinka_ci_build.py --log ... -- ...`):

```sh
cmake --build build-release --config Release --target game-client bestclient-updater --parallel 2
cmake --build build-release --config Release --target package_zip --parallel 2
```

Outer CI artifacts: Windows **13807725269 /117850062 bytes**, Linux **13805474638 /116204955 bytes**. Не путать outer bundle с inner ZIP ниже.

### 2. Actual component и helper checks

- **CI42908966581**, workflow commit **`1d1a032dd96075a621c38a4a6e63f9d92cffdd47`**, job **130154252439**, checkout строго facfc85: **SUCCESS10m16s /watch exit0 /2102 checks /0failures**.
- Actual CClient/CGameClient/CMap/world/console/config, карты **coverage50×50 и Tutorial2210×110**, оба input slots, Legit/Blatant и Hook/Direction on/off. **41 assisted outputs**, **128 allocation refusals** с неизменённым исходным input. Проверены local character core/freeze time и live tick до/после, selected input FOV1/45/180/360, AFK после реально измеренного idle1100ms, config принимает Quality1000/Life298.
- Synthetic Insert через настоящий `CGameClient::OnInput`: redirect из вкладки, немедленный F1 command/setting/help filter; обычный cl_predict completion, explicit/toggle execution, history/input и remote completion cache сохранены; component reset не сбрасывает latch.
- ASan/UBSan инструментируют **4 production units**, не весь world; component LSan off только из-за renderer-owned console logger в no-renderer fixture. **Max call29.2549ms на Linux с instrumentation**, не Windows FPS proof. Physical keyboard/renderer/server не запускались.
- **Pure policy/panic local compile0/run0 +same CI success**: **29656558 checks /0 failures /595204 rollouts**, ASan+UBSan+LSan. В89/100 same-seed synthetic cases смена life weight100→298 изменила решение; это не гарантия глобального optimum при каждом UCT seed.
- Helper binary `build/local/check-avoid-policy-5`; logs `.artifacts/avoid-review/policy-{compile,run}-5.{log,exit}`. Component metadata `component-facfc85-{result,annotations}.json`. Exact g++ command в BUILD.

### 3. Оригинальные доставленные файлы

Runner transfer **CI42914015262 /c1738ff144d83c2736779da5ea8ecf4a423220c4 /job130171633080 SUCCESS47s /watch exit0** проверил native run/source/artifact identity, SHA/CRC/PE/marker и source manifest SHA. Передача через **6 bounded unreferenced Git blobs**, без binary payload commits/trees/refs/branches/tags/releases. Metadata `.artifacts/avoid-review/transfer-final{,-annotations}.json`.

| Файл | Bytes | SHA-256 |
|---|---:|---|
| Windows ZIP |60425920|`b7d0a2349b6c79c5a87299909d14b678ae5f4150f91a1cb0e99e0bb01225b231`|
| Companion source tar.gz |57789009|`d605a975ecbcf0e8c4ffd3b1ab4abbe6a8d7e523796d2a38c3f8d97d501e3c7a`|
| DDNet.exe внутри ZIP |8950272|`0721b337ccf0ee0543fb14afe1110a5dd4ee13d25102ff2baf8e0cc0bbaf6752`|

- Workspace **`build/local/delivery-0.1.1-facfc85`**: оригинальные ZIP/source, sidecars, `SHA256SUMS.txt`, `TEST-NOTES-0.1.1.txt`, `verified-delivery.json`. Main ZIP открыт `present_file`. EXE AMD64/PE32+, marker0.1.1,1039 ZIP entries (1038 manifest files+manifest), CRC passed. Отдельный EXE без DLL/data не выдавать.
- Companion compressed SHA точно совпадает с `source_sha256` manifest. Все **2375 source paths** сверены с facfc85: **822 byte-identical blobs,1553 text files ONLY LF→CRLF** Windows export. Никаких иных content changes; оригинальные archives не перепакованы. Canonical LF — git checkout facfc85. Tracked Cargo.lock SHA относится к LF blob, не CRLF bytes внутри tar.
- `retrieve_final.py` скачал и сверил все6 chunks, но завершился **exit1** на strict LF-byte assertion `.cargo/config.toml`. **Не скачивать заново/не повторять его as-is.** Последующий `verify_received.py` допустил только точное bareLF→CRLF, проверил каждый path/size/GitOID, **exit0**; logs `verify-received.{log,exit}`. Это не побайтовая идентичность всего source tar GitLF tree и не binary reproducibility proof.

### 4. Actual package installation regression

**Compile0/run0 /2821 checks /0failures**, ASan+UBSan+LSan, current production C++ ExtractZip/Verify/Install над этим ZIP и companion source:

- **1038 files /906 installed /132 preserved package paths**; source manifest SHA, обычные updater limits.
- Wrong platform и modified license tamper отвергнуты до мутации установленного клиента.
- Callback exception после первого изменения → rollback и прежние file fingerprints.
- Успешная установка/readback; config/native replay/raw vault/custom arrows/unrelated sentinels сохранены; backups сверены.
- **Linux file operations**, не Win32 process/UAC/updater window/restart/gameplay.

Binary `build/local/check-final-package`, fixture `.artifacts/avoid-review/check_final_package.cpp`, logs `final-package-{compile,run}.{log,exit}`. Fresh owned root **`build/local/ci-package-facfc85-check-1`** уже существует: не повторять на нём и не удалять user paths. Exact commands в BUILD; fixture recipe из historical53821ae, compiled с нынешними headers/0.1.1.

### 5. Dropbox delivery

Новая папка **`/Malinka-test-0.1.1-facfc85`**, id **`id:dED5V7bj3cAAAAAAAAAADg`**:

| Файл | ID | Revision |
|---|---|---|
| Windows ZIP |`id:dED5V7bj3cAAAAAAAAAAEg`|`65ff399bc4aca4b309220`|
| Source tar.gz |`id:dED5V7bj3cAAAAAAAAAAEQ`|`65ff399aae0674b309220`|
| SHA256SUMS.txt |`id:dED5V7bj3cAAAAAAAAAAEA`|`65ff398f1f58d4b309220`|
| TEST-NOTES-0.1.1.txt |`id:dED5V7bj3cAAAAAAAAAADw`|`65ff398ea6b594b309220`|

Все4 files uploaded add/strict-conflict; metadata readback sizes и независимо рассчитанные4MiB-block Dropbox content hashes совпали. ZIP hash **`59cf4e6868ebb045466f47e89fa95b39f1cd19ab08500e717a9302599e317824`**, source hash **`cfce9805004cb6376b39eca0a11dee61644ea9383439233165732a96d4aac9db`**. Public viewer/download links созданы для ZIP и папки; **access parameters не копировать в Git**. Metadata `.artifacts/avoid-review/dropbox-delivery.json`. Существующие пользовательские файлы не заменялись.

## Files / entrypoints / ограничения

- `avoid_policy.h`: независимый bounded UCT. Quality≤1000,≤1001nodes,60ticks; Life298 не clamp-ится к100. Defaults16/13 и прежний Life scale/100 не менялись. Screenshot evidence: Legit blob6199bec3090b470110bef5a2b97d9c78360e005d; original1.32 maxima/defaults не установлены.
- `avoidfreeze.{h,cpp}`: private world, AFK seconds, Check/Kick-in, action/tile/FOV constraints, revalidated cached input; private forbidden teleport entrances остаются observable, quantized aim проверяется, finite/abs<1e9 position guard.
- `malinka/controls.cpp`: Basic-only side probe, raw activity, active-slot IDs, selected aim больше не затирается common output. **Позднейшие другие helper features ещё могут менять input**, это не runtime proof всей orchestration.
- `malinka/menus.cpp`, `config_variables_malinka.h`: группы и ranges Quality1..1000/Life0..1000; `capabilities.h` **60 remaining gates**, exact registry/docs comparison passed.
- `panic.h`, `gameclient.cpp`, `console.{h,cpp}`: F1 presentation-only filter, commands/history не удаляются; build_identity0.1.1/CMake/misc status.
- Незавершённые PlayerPrediction/TrackPoint/AutoDrag/visuals остаются gated. NSIF хранит одно constant input, не полную safest sequence. Оригинальный KRX algorithm/defaults не восстановлены.
- **fake не исправлен**: HasOfficialCode пустой, remote PACKET_PEER_VERSION_STATE/browser cache authoritative. Local flag suppression/authorization forgery не делались. Нужны legitimate Best service fork authorization и наблюдение со второго unmodified клиента.
- KRX/TAS concealment rename не выполнялся. Published/end-to-end update channel отсутствует.
- **not verified in-game**: Windows11 renderer/gameplay/focus/key release/physical Insert/reconnect/restart; FreezeReset friend timer/helper conflicts; swept hazards/performance; external badge.

## CI cleanup и следующий проверяемый шаг

- Disposable `.github/workflows/malinka-avoid-once.yml` удалён после завершения checks/transfer. Normal `.github/workflows/malinka-build.yml` восстановлен **byte-for-byte изc4c450b**, без temporary skip/paths-ignore/contents-write. Cleanup commit — docs/workflows-only `[skip ci]`; **повторная native сборка неизменённого production кода не заявляется**. Historical1d1a032/c1738ff сохраняют recipes. Нет permanent tests/heavy artifacts в Git.
- Все watch-процессы latest native/component/transfer завершились exit0, pending CI для delivery нет. Manual workflow_dispatch403 integration permission; push-trigger работал. Не повторять dispatch и не просить credentials.
- Следующий input — Windows11 user test этой свежей сборки в отдельной папке/профиле по TEST-NOTES: `storage.cfg` CURRENTDIR/DATADIR (папка сама по себе не изолирует `$USERDIR`), F1 до/после Insert, бинды/held keys/reconnect/full restart, Legit/Blatant constraints/AFK/FOV/tele/death/unfreeze/slots. Записать фактические ошибки/поведение; не выдавать compilation за gameplay.
- Продолжать независимую реализацию отсутствующих Avoid consumers и остальных1.32 функций на известных Best APIs с real-world checks, не пустые toggles/guessed defaults. Для exact original parity нужны измеренные UI/default/input-output traces закрытого бинарника на Windows; recipe в STATUS/SOURCES, исходники снова не спрашивать.
- **Не объявлять исходный полный запрос выполненным из-за успешной текущей промежуточной доставки.** External badge authorization и опубликованные updates — отдельные unresolved dependencies.

## Исторические неудачи и obsolete packages

- CI42904487820 fixture compile failed на ExecuteLine без ClientId; исправлен fixture, не production.
- CI42906400444 actual component остановился на398 checks: LastActivity=`time_get()-2freq` оказался отрицательным (process-relative timer), production правильно его отверг. Fixture перешёл на measured1100ms idle; latest run полностью passed.
- Helper fixture3/4 требовал global optimum для каждого seed от bounded UCT; исправлена assertion на доказуемую consumption/difference property. Production ради прохождения этого assertion не менялся.
- Старые c4c450b/3c7d586 native тоже passed, но их packages не latest. c4 transfer — metadata only, download/delivery не было. Пустая Dropbox `/Malinka-test-0.1.1-c4c450b` оставлена без изменений. Старую0.1.0 доставку ниже не подменять за новую.

## Историческая доставка 0.1.0: EXE пользователю

Пользователь попросил скачать EXE и проверить сам; после ссылки Actions прислал screenshot **ERR_SSL_PROTOCOL_ERROR на `productionresultssa7.blob.core.windows.net`**. Это сбой скачивания, не наблюдение игрового запуска. Не предлагать обход TLS/certificate verification и не повторять ту же Azure-ссылку.

- В этом восстановлении sandbox Git снова имел initial244d7d4/неиндексированные sources; **измерена** идентичность2373files remote60c4275(0missing/0different), затем безопасно восстановлен history/index `git reset --mixed FETCH_HEAD` без source mutation/смены ветки. Никаких pending source edits не потеряно. Не запускать recovery повторно без нового измеренного расхождения.
- Старые ignored harnesses/logs/build outputs из разделов ниже **снова отсутствовали**; текущие `.artifacts/delivery`/`build/local/delivery` созданы заново. Не объявлять старые процессы живыми и не повторять старые scripts.
- **Тот же** Windows package из CI42887818609/0a0eeed,artifact13796858394 перенесён без пересборки/изменения EXE. One-shot delivery CI **42897858639/805eedbe5b4048e21a0f7a57a5cce2de13b5ffe7 SUCCESS/exit0**. Runner проверил artifact/source identity,sidecar SHA,CRC,PE AMD64/PE32+ и Malinka build marker. Из-за недоступного Azure в sandbox использованы3bounded **unreferenced Git blob objects**,без binary commits/tree refs/новых branches/tags/releases; это только обязательный transport для файла,не новый update channel.
- Оригинальный **inner** `bestclient-malinka-windows-x86_64.zip`: **60418532bytes**, **SHA256 `653c7ed5853e6ae4bce948b0525442c1ac3c6305b62099e4c17a8c0d4862fb81`**,1039ZIPentries(1038manifestfiles+manifest). `DDNet.exe` **8948224bytes**,SHA256 **`b4a4cf17c450b57a68e43947176215f0321d61bf8c5bc2a4e4d6ac7db4a0e0d0`**. Не путать с outer Actions bundle117825375bytes и старым2d00f1fpackagehash.
- Реально получен локально через разрешённый GitHub raw-blob API,проверены per-chunkSHA256/GitOID +wholeSHA256+ZIPCRC+exeSHA,**retrieve exit0**. Workspace `build/local/delivery/bestclient-malinka-windows-x86_64.zip`,открыт `present_file`;отдельный extracted `DDNet.exe` тоже есть,но **без DLL/data не раздавать как standalone**. `.zip.sha256` рядом.
- Загружен в подключённый Dropbox **`/Malinka-test-0a0eeed/bestclient-malinka-windows-x86_64.zip`**,file **`id:dED5V7bj3cAAAAAAAAAACw`**,rev65ff227d546974b309220,size60418532. Returned **Dropbox content hash `1e24921a55cb5d31e56a5d7a4a1f94afca2892fd5ecbd1f7146c1ba52595c3a4`** совпал с независимым local4MiB-block algorithm. Share-link/access parameters не копировать в Git;ссылка выдаётся пользователю в чате. Upload modeadd/strict conflict,ничего чужого не заменено.
- Fresh **actual production C++ ExtractZip+Package::Verify** над этим0a0eeedZIP локально,ASan+UBSan+LSan: **1038files проверены/exit0**,версия0.1.0/Windowsx64/marker/все manifestSHA+sizes. `.artifacts/delivery/verify-{compile,run}.{log,exit}`. Это **Windows ZIP integrity на Linux**,не Win32 execution/installer UI/gameplay. Companion source archive0a0eeed в этой доставке не получен/не сравнивался;известный source hash из manifest не выдавать за отдельный readback.
- Точная isolated compile: `g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc -Isrc/engine/external/zlib .artifacts/delivery/verify_package.cpp src/game/client/components/malinka/file_safety.cpp src/base/hash_libtomcrypt.cpp /lib/x86_64-linux-gnu/libz.so.1 -o build/local/delivery/verify-package`,**exit0**. Run: `ASAN_OPTIONS=detect_leaks=1 build/local/delivery/verify-package build/local/delivery/bestclient-malinka-windows-x86_64.zip build/local/delivery/verified-extraction`,**exit0**. Для rerun fresh extraction root,не трогать user paths.
- Production sources/CMake/lock не менялись;native rebuild в этом delivery turn **не запускался**,последний fullMSVC+Linux success0a0eeed указан выше. Temporary delivery workflow и contents-write permission удалены после success;normal workflow восстановлен byte-for-byte,cleanup/docs commit `[skip ci]` пропускает только лишнюю пересборку неизменённого кода.
- Пользователь **ещё не сообщил об успешном старте/меню/feature tests**. Полный запрос по-прежнему incomplete. Следующий verifiable input — фактический запуск пользователем;получить ошибку/скрин,если не откроется. Для теста отдельная распакованная папка;перед первым запуском **в тестовой копии** storage.cfg=`add_path $CURRENTDIR` затем `add_path $DATADIR`,запускать из этой папки,чтобы не трогать обычный$USERDIR профиль. Исходный ZIP не модифицирован для portable mode.

## Точный запрос / постоянные условия

Пользователь просит вкладку **`malinka` со всеми функциями `KRX-Client-18.9.1-win64-ultimate-v1.32`** в BestClient, обновления без потери функций, нормальный BestClient badge вместо `fake` в Tab у других игроков и **Insert panic**, скрывающий cheat tabs до «перезайти». Входные Dropbox links с access parameters есть в разговоре, не копировать в Git. Не выдавать Pasta adaptation за exact/official/full KRX. Пользователь отказался от investigation-only/source question; исследовать и делать самостоятельно, **не просить sources опять**.

Предварительная panic semantics: **UI-only до полного process restart**, reconnect не возвращает UI; функции не выключаются. Remote badge требует внешней авторизации fork/наблюдения из второго немодифицированного BestClient, локальная картинка≠решение. Собственный channel/checked installer написан, published/end-to-end updates ещё нет.

Ответы: точная первая строка **`[Aero]`**, обращаться **бро**, русский, код/пути на исходном языке. Перед долгой работой короткая цель; после deliverable отдельно changed files / exact build+exit / checks+result / observed runtime либо **not verified in-game**. Корректность важнее скорости; не выдумывать offsets/signatures/layouts/ABI/IDs/совместимость; unknown-dependent controls visibly gate, независимые части доделывать. Context length не причина остановиться. Temporary isolated checks допустимы, не добавлять постоянный test suite без conventions.

## Текущая среда: важное восстановление

Sandbox восстановился с source tree, **без** старых `.artifacts`, `build/native`, `.venv`, toolchains/dependencies/harnesses/logs. Старые process IDs не использовать. Git был на initial244d7d4 с почти всем деревом untracked. Уже безопасно исправлено: fetch точной ветки; ancestor check; SHA256-сверка2369 remote files(0missing/16different); **`git reset --mixed FETCH_HEAD`** восстановил history/index доe2fdaef **без изменения working files/branch**. Не повторять reset/recovery и не терять pending work.

Теперь есть g++12.2/gcc,Python3.11/pip,gh; нет cmake/ninja/cargo/rustc/SDL/GL dev headers. Bundled zlib headers и runtime `/lib/x86_64-linux-gnu/libz.so.1` доступны. Новые `.artifacts` содержат заново созданные decoder/vault/input/package harness/scripts/logs и CI metadata, не прежние historical builds. `build/local/replay-vault` содержит повторно скачанный pinned vault и DDNet18.9.1 character.cpp; `build/local/generated/src/generated` — настоящие generated protocol/client headers.

## Что реализовано и сохранено (2d00f1f)

- **`krx_replay_io.h`**: строгий observed LE32/zlib/JSON reader, exact keys/types/UTF-8/bounds, transactional outputs, scoped flat ownership C-parser allocations (унаследованный parser оставлял disconnected allocations при second-pass OOM). `SInfo::m_NormalizedZeroAimFrames`: только нулевой aim→(0,-1), явный count. Provenance: DDNet18.9.1/Best OnDirectInput + OnPredictedInput, не предположение о KRX. INT_MIN/другие invalid inputs не clamp.
- **`tas.{h,cpp}`**: preview-only import, полный current-world pre/post phase validation (одна consistent hypothesis на весь track), conversion только после полного успеха с hash текущей map/Best physics; original initial world/hash/phase/ticks не выдуманы. Pending нельзя Save/Playback. Save guards: исходный canonical/equivalent KRX file не заменять; `vault/` snapshot read-only case-insensitively, до mkdir и на write boundary. Normalization warning.
- **`replay_vault.{h,cpp}`**: real HTTPS GitHub HEAD→pinned archive→engine job, sync/cancel/status, default-off startup/rising auto-sync,60s throttle/120s timeout. Worker владеет paths/data, не CGameClient pointers; results читать только DONE, не ABORTED. Null/non-DONE request проверяются **до StatusCode/Result**, иначе debug trap.
- **`replay_vault_io.h`**: raw immutable `malinka/replays/vault/<sha>`, work `malinka/vault-cache/staging-*`; ZIP32MiB/member4MiB/expanded64MiB/4096entries/1024replays/depth6; prefix/revision/path/type/CRC/hash/repeat verification; no-replace publish. Никакого auto load/convert/play. Cleanup только owned work,8192entries/depth128/retry2, persistent failure может оставить staging.
- **`file_safety.{h,cpp}`**: native transactional directory enumeration вместо libstdc++ iterator, OOM которого реально terminate-ился; bounded no-link cleanup; Linux renameat2/Windows MoveFileEx no-replace. Win32 headers только cpp (IGNORE macro не лечить чужим game enum).
- **`malinka_package_zip.h`**: optional bounded limits с неизменёнными updater defaults512MiB/member+archive,2GiBtotal,10000entries; EOF/growth checks.
- **`engine/http.h`, `shared/http_{curl,emscripten}.cpp`, `bestclient/updater.cpp`**: RequireHttps запрещает plaintext/redirect/insecure TLS для vault и обоих updater requests независимо от глобальной testing config; Emscripten initial scheme check only.
- **menus.cpp / menus.h / capabilities.h**: явная **Load Selected Replay** (чинит single/index-zero/unchanged dropdown), import pending/vault status+sync/cancel; vault auto-sync default0/ungated; реестр содержит **71**, не72, remaining unavailable configs. CMake/gameclient component wiring.
- **docs/MALINKA_{STATUS,SOURCES,BUILD}.md**, этот HANDOFF обновлены; README уже честно помечает incomplete fork.

## Источники / измерения

- Best3.1.1 `cf192e3fb8590dbe2e4f66a191aa6616beff6975`,DDNet20.1.1/TClient10.9.0. Supplied Best ZIP120621904bytes превысил Dropbox connector104857600limit; binary identity **не установлена**, не повторять тот же download.
- Pasta donor `dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f`, экспериментален, не official KRX. KRX docs `8c193ca2a27cc7ba5d4e2d6ad7686c6e99910c43`:1.32=2025-02-07/DDNet18.9.1. Поздние1.33–1.36 features не добавлять автоматически в обязательную1.32parity.
- Supplied KRX ZIP80079820bytes SHA256 `77aec68613b9f0f01dd43ec27f955e21a6546076ef539863025ea6140ace67e9`,973CRC pass,нет source/PDB. EXE14364176bytes SHA256 `f9ec1d9f050e4ecd9f426d0ae71abaf5d91972a082636822883d419856858c85`,protected PE64; protector/runtime layout не установлены. File offsets/preferred ImageBase в SOURCES, не runtime game addresses. В Windows не запускался.
- SDK `ddnet/ddnet-libs` `c0e6703fbcdbe03df2f26875427ec3951ce4ec21`; normal pipeline Rust1.92.0 stable/исходный locked registry. Cargo.lock SHA256 **`760288176362ad39458b4f01673df5ad8729809cd1e0178da0573ef728c6a904`** неизменён. Historical sandbox nightly/npm wrapper не применять к release/CI.
- **Vault pin `ed7bfe99968c3107d05e57c0c06c3ef3e90f3493`**,treec8fe016b2b0f03056c36c3b86e79e5f90500bcc8/333entries; ZIP11528399bytes SHA256 **`cc5c7f41380decf5cbcacc9aab3c0dc50a80e0565e6e4dff778f99d21de0ca02`**,179tas/11492809expanded/max476828,CRCclean. JSON max22260071bytes/45958frames. Roots152plain/25conditions_local/1conditions+bothtracks/1dummyonly. `main`≠proven1.32snapshot.
- **DDNet18.9.1 `3b4b69c13365f4efd76b619c3762b3a8306569b4`**,actual `prediction/entities/character.cpp` lines514–515/537–538 canonicalizes zero aim to(0,-1),same as Best551–552/574–575. Fetched via gh contents API,local `build/local/replay-vault/ddnet-18.9.1-character.cpp`.
- Before normalization actual C++ batch49accepted/130rejected:102zero-aim-only,1INT_MIN aim,27root schemas. **Now actually measured151accepted/28rejected**,1502zero-aim frames. Это parsing, не original-world/gameplay proof.
-069 SHA256 `9ea981d09031d6bf9f9594b1e2ce455311a2a90cdf283a26fb5c95412f06b99b`,1895frames;003 `335f12a54417748e14220327dce938ce7acf2630c830ca01579b72441d16d205`,7248frames. No initial world/hash/phase.

## Свежие проверки после восстановления

Temporary `.artifacts/acquire_replay_checks.py`: pinned ZIP SHA/CRC,179raw files; independent Python normalized input/tick/position SHA256;94generated valid/hostile fixtures (удаляет только свои valid-/invalid- files в disposable dir). `.artifacts/check_krx_normalized.cpp`: actual decoder **и actual SFrame**,не guessed struct; all named fields hashed LE32/floatbits,transactional rejection,allocation failures,normalized native replay roundtrip,raw unchanged.

```sh
mkdir -p build/local/generated/src/generated build/local/check-outputs
python3 datasrc/compile.py network_header > build/local/generated/src/generated/protocol.h
python3 datasrc/compile.py client_content_header > build/local/generated/src/generated/client_data.h
python3 -m datasrc.seven.compile network_header > build/local/generated/src/generated/protocol7.h
python3 .artifacts/acquire_replay_checks.py
gcc -std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -c src/engine/external/json-parser/json.c -o build/local/check-outputs/krx-json.o
g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -Ibuild/local/generated/src -Isrc/engine/external/zlib \
  .artifacts/check_krx_normalized.cpp src/base/hash_libtomcrypt.cpp \
  build/local/check-outputs/krx-json.o /lib/x86_64-linux-gnu/libz.so.1 \
  -Wl,--wrap=malloc,--wrap=calloc -o build/local/check-outputs/check-krx-normalized
ASAN_OPTIONS=detect_leaks=1 build/local/check-outputs/check-krx-normalized
```

**Compile3 exit0 / run1 exit0: `2366910 checks,0 failures,94 fixtures,151accepted/28rejected,1502normalizedframes,35 C+3 C++ allocation failures`**, no sanitizer/leak errors. Logs `.artifacts/normalized-{compile-3,run-1}.{log,exit}`. Initial compile1 nonexistent `hash_ctxt.c`,compile2 unneeded hash.cpp unresolved deps; fixed by actual SHA context implementation only. `datasrc/seven/compile.py` direct call failed relative import; correct `python3 -m datasrc.seven.compile` above. Real header probe exit0. Это fresh parser/I/O check, **не full client/physics/Windows build**.

### Full Windows/MSVC+Linux build и actual artifacts: теперь тоже проверены

- **Native CI42885117777/2d00f1f SUCCESS Windows2022/MSVC x64(10m33s)+Ubuntu24.04(8m8s)**,включая importer/vault/normalization; полный client+updater+package_zip+flat generator+exact source archive. <https://github.com/twin15388-beep/best-v2/actions/runs/42885117777>. Actual commands: `python scripts/malinka_ci_build.py --log build-release/compile.log -- cmake --build build-release --config Release --target game-client bestclient-updater --parallel 2`; затем аналогично portable.log/`--target package_zip`,оба exit0 на обеих платформах. Configure/dependency pins в BUILD/workflow.
- Artifacts **Windows13795797827/117824429bytes;Linux13796002799/116176507bytes**,not releases. `gh run download42885117777 --name malinka-linux-x86_64 --dir build/local/ci-42885117777/linux` реально **exit1/EOF**,redirect на blob-host вне sandbox allowlist; не скачаны локально. Не повторять неизменённый download и не заявлять локальный package pass.
- Вместо остановки проведён **одноразовый CI42887324042/53821ae SUCCESS**,оба actual artifacts42885117777 скачаны GitHub runner,gitdiff production src/CMake/lock против2d00f1f и source commit build-environment проверены,actual C++ installer harness под ASan+UBSan+LSan. **Оба jobs на Ubuntu24.04 Linux file operations**,не Win32 UI/runtime. <https://github.com/twin15388-beep/best-v2/actions/runs/42887324042>.
- Linux **2785checks/0failures/exit0**,1026files/894installed/132preserved. ZIP SHA256 `3255c75080142bc7bcbb9a86510ce9c5b3b9b0394f533f9599168258221bc644`,source `e56cb5036a96ddb149db1319aacea04ff96cc8e95a62f3629af9598a578859ce`.
- Windows ZIP **2821/0/exit0**,1038files/906installed/132preserved. ZIP SHA256 `e1b559c0ad4c59c319dab1777abf04f9271a8f1ddb251d8435103fc4c1b81082`,source `594524b556352694ccc3e4df261fefc644c06637a788b1ae3635a37df7e0f3f7`. Source archives differ; each matches own manifest,byte-identical cross-platform archive claim не делался.
- Real ZIP/defaultlimits/manifest/source SHA/install/readback;forced callback exception after first mutation→rollback;oldclient/license backup;settings_custom+ddnet/replays/rawvault/arrow/unrelatedfiles preserved;wrongplatform/tamper rejected. No game launch/process wait/Win updater window/deployed release.
- Exact temporary code/commands: `git show 53821ae52e70063bdca855f71a2828861534a13a:.github/workflows/malinka-package-regression-once.yml` (heredoc builds `build/check/package.cpp`);local `.artifacts/check_ci_package.cpp`,compile2exit0. Recipe также BUILD. Checked run outputs retrieved via check-runs130078985658/130078985882 annotations API,stored `.artifacts/ci-{linux,windows}-package-result.json`;other metadata `.artifacts/ci-{native-build,package-regression}-result.json`,ci-build-artifacts.json.
- Не оставлен постоянный test suite: one-shot workflow удалён,обычныйworkflow восстановлен commit0a0eeed. Temporary skip expression на53821ae имел YAML colon quoting error→run42887321410 failure **без compile jobs**,исправлен9910aa9→normalbuild42887385962skipped;потомусловиеубрано. **Не compiler regression.** Новый normalworkflow **42887818609/0a0eeed SUCCESS обоих jobs/exit0**, ordinary workflow восстановлен и исполнен. <https://github.com/twin15388-beep/best-v2/actions/runs/42887818609>. Последние artifactsWindows13796858394/117825375bytes,Linux13797033179/116182078bytes;не скачаны вsandbox/не releases. **Actual installation regression выше относится к ZIP2d00f1f, не новым ZIP0a0eeed**. Source/workflow идентичны, binary reproducibility автоматически не заявляется. Metadata `.artifacts/ci-restored-build-{result,artifacts}.json`;watchprocess `final-restored-build-workflow-ve-a2f7fd2a` закончилexit0,старых pending процессов нет.

### Ещё свежие local isolated checks

- Vault `.artifacts/acquire_vault_checks.py`→32ZIPfixtures(29invalid/3valid),`check_vault_restored.cpp`+actual `file_safety.cpp`/SHA/zlib,ASan+UBSan+LSan: **6567/0/exit0**,179byte compares,cold/repeat/tamper,no-replace/links/user sentinel,8I/Ocancelcheckpoints+12completed,716allocationpositions/691throws,OOM567refusals/159successes. Not component auto-sync/cache-job-stage cancellation test.
- Input/replay `.artifacts/check_input_replay_restored.cpp`,actualheaders/SFrame: **107206/0/exit0**,8192 inputstreamscenarios/324finiteaimcases,pure latch,paths,CRCtransactionalreadback,backup/.new/.bak.newrefusal,links/NaN/ticks. **Не physical Insert/navigation/runtime**.
- Exact compile/run commands в BUILD;new logs `.artifacts/{vault-restored,input-replay-restored}-{compile-1,run-1}.{log,exit}`. Disposable owned roots `build/local/{vault-restored-run-1,input-replay-restored-run-1}` уже существуют;rerun только с новыми roots,не удалять user dirs. Сборка temporaryharness не fullclient.

## Ранее реально исполнено (до потери ignored build/log files)

Результаты ниже исторические, **не новые reruns**. Native17 и18 **подтверждены exit0**, нет висящего старого build. Native18 включал Load button/read-only Save,но до zero-aim normalization.

- Native18: `PATH="$PWD/.venv/bin:$PWD/.artifacts/toolchain/rust/bin:$PATH" CARGO_HOME="$PWD/.artifacts/cargo" CARGO_NET_OFFLINE=true cmake --build build/native --target game-client bestclient-updater package_zip --parallel 2 -- -k 0` — **exit0**. Paths сейчас отсутствуют.
- **GitHub42875704110/e2fdaef SUCCESS Windows2022/MSVC+Ubuntu24.04**, rechecked current sandbox. <https://github.com/twin15388-beep/best-v2/actions/runs/42875704110>. **Не включает importer/vault.** Artifacts Windows13791573420/117783968bytes,Linux13791952700/116103698bytes,не downloaded/not releases. Earlier42873249161/bcef908 также pass.
- World/TAS lifecycle run5 ASan+UBSan+LSan **10165/0/exit0**,2real maps(coverage50×50,Tutorial2210×110),362handled allocfail(109+253),pre/post imports,full validation/neutral before validation,successful converted Save/readback и source/vault overwrite refusal. Uses actual CGameClient/CMap/world/TAS methods,не mocks; no renderer/server.
- HTTP/jobs run3 **26/0/exit0**,libcurl7.88.1,real GitHub HEAD/archive,cold179/repeat179verified,user replay preserved,pending069/playback blocked,failedHTTP,duplicate/cooldown/cancel/shutdown. Auto-sync toggle/cache-job-stage cancellation не отдельно проверены.
- VaultIO run4 **1707/0/exit0**,21hostile ZIP fixtures,179byte-identical,876allocs/346probes/303throws. libstdc++ directory_iterator OOM terminate реально найден→native traversal;cleanup6failures→bounded retry2→0failures.
- Decoder93fixtures+2samples **18836/0/exit0**,32Callocfail,leak2489bytes/51allocs найден→flat arena fixed. All179 pre-normalization **19146/0/exit0**,49accepted/130rejected. Новый normalized batch выше supersedes acceptance.
- Package/input logic7 **9459/0/exit0**; generator2 **50/0/exit0**. Actual ed5a7791026file Python→C++ Extract/Verify/Install **exit0**:exception rollback,actual binary readback,settings/replay preservation,backup,tamper. Новый actual-package regression **теперь прошёл** выше; deployed/Win32 runtime нет. Generator source_sha не доказывает binary reproducibility/signature.
- Independent physics8 **8357/0/exit0**; canonical Best ordering comparisons,serial/parallel/detached contexts/PRNG/COWdoors. Character states,не full world bytes/server equivalence. Win updater Zig actual x64 GUI cross-link **exit0**,не Windows runtime; real MSVC CI отдельно выше.
- Actual Linux client launch processexit0,**Failed to initialize the graphics/OpenGL**. Не считать renderer smoke. No X/Wine/MSVC locally; benchmark command/TSan не запускались.

Если заново строить world harness: использовать реальные native link objects; engine/client/client.cpp.o сохранять с `main→MalinkaClientMain` через objcopy,не выкидывать; CLayers.Init(Map,false,true); пересобирать все CGameClient/TAS layout consumers после header changes. Старые non-idempotent injection scripts не запускать/не применять повторно к уже исправленному коду. Engine console harness cleanup был test-only,production не менять ради LSan.

## Что ещё не сделано / ближайшие проверяемые шаги

1. Source/workflow сохранены и два свежих fullMSVC/Linux runs **SUCCESS**; одноразовый fixture удалён. Документы фиксируют точные outputs. **Продолжать незавершённую реальную1.32функциональность**,не повторять unchanged CI/старые downloads и не считать60gates выполнением fullscope. Независимые consumers реализовывать по проверяемой семантике; отсутствующие game values/IDs оставлять явно unavailable.
2. Actual artifacts2d00f1f installation/rollback/preservation/tamper **пройдены** CI42887324042;к Windows11 UI/process wait/real install/restart переходить отдельно,без published release пока runtime/incomplete list не проверены. Для других source commits эти packagehash/results автоматически не переносить.
3. Текущий vault parser honest rejects conditions/dummy schemas. Свежий Python audit:26conditions_local и1conditions_dummy,все exact fields `hook_id,pos,weapons`;weaponkeys WEAPON_GRENADE/GUN/HAMMER/LASER/NINJA/SHOTGUN bool;hook_id наблюдался0/1. **Семантика hook_id/remapping и достаточность conditions не доказаны**,не считать0/1 настоящими liveclientIDs. Исследовать/переноситьбезигнорированияconditions/выдумыванияworld. Авто-sync toggle/cache-job-stage cancel отдельная execution regression ещё нужна.
4. **60 remaining gated configs**,full Grenade saver/advanced movement/Avoid/Fent Light+jump visuals/chat`!help/!skin`/RPC/wiring и exact1.32parity неполны. Malinka RPCIDs неизвестны,BestID1444576875774083133 не KRX. Laser negativeRes=-1 не reproduced; не закрывать по1000/0logic alone.
5. Windows11 x64 runtime на отдельной install/profile:renderer+allmenu sections,focus/keyrelease,dummy/TAS/door/tele/tune/events,Insert на открытой странице/search/reconnect/restart. Vault Sync→Reload→Load→Validate matching map/start→new Save outside vault→readback;negativecases preserve buffer/raw/snapshot. Не проверено в игре.
6. Exact KRX acquisition recipe в STATUS:Windows VM,actual EXE hash/module identity/SizeOfImage/path/version;record UI/config defaults/input-output traces/replay per option on same maps. RVA≠VA without measured base. Полный original source user повторно не спрашивать.
7. Genuine remote badge:authorization внешнего Best service +second unmodified client observation. Public pipeline не имеет BESTCLIENT_OFFICIAL_CODE/private secret;local suppress fake≠requirement. Update channel own GitHub releases endpoint есть,операционных releases/deployment/restartproof пока нет.

Документы: `docs/MALINKA_STATUS.md` (current60controls/limitations/manualrecipe),`SOURCES`(pins/formatprovenance),`BUILD`(MSVC/Linux/packagecommands),`MALINKA_DEPENDENCIES.json`(SDKversions),`README.md`(honestincompletefork). Все generated fixtures/downloads/builds ignored,не добавлять тяжёлые artifacts/секреты в Git.
