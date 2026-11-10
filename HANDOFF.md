# Передача задачи: BestClient + malinka

Обновлено **2026-11-10 UTC**. Repo `/home/user/best-v2`, ветка **`arena/733fc6a9-best-v2`** (не переключать/переименовывать), последний pushed source commit перед этим продолжением **`e2fdaef4cd0952c041401d9e324dd3a914bd633c`**. Текущие importer/vault/normalization изменения требуют commit/push и свежего CI. **Полный запрос не выполнен.**

## Точный запрос / постоянные условия

Пользователь просит вкладку **`malinka` со всеми функциями `KRX-Client-18.9.1-win64-ultimate-v1.32`** в BestClient, обновления без потери функций, нормальный BestClient badge вместо `fake` в Tab у других игроков и **Insert panic**, скрывающий cheat tabs до «перезайти». Входные Dropbox links с access parameters есть в разговоре, не копировать в Git. Не выдавать Pasta adaptation за exact/official/full KRX. Пользователь отказался от investigation-only/source question; исследовать и делать самостоятельно, **не просить sources опять**.

Предварительная panic semantics: **UI-only до полного process restart**, reconnect не возвращает UI; функции не выключаются. Remote badge требует внешней авторизации fork/наблюдения из второго немодифицированного BestClient, локальная картинка≠решение. Собственный channel/checked installer написан, published/end-to-end updates ещё нет.

Ответы: точная первая строка **`[Aero]`**, обращаться **бро**, русский, код/пути на исходном языке. Перед долгой работой короткая цель; после deliverable отдельно changed files / exact build+exit / checks+result / observed runtime либо **not verified in-game**. Корректность важнее скорости; не выдумывать offsets/signatures/layouts/ABI/IDs/совместимость; unknown-dependent controls visibly gate, независимые части доделывать. Context length не причина остановиться. Temporary isolated checks допустимы, не добавлять постоянный test suite без conventions.

## Текущая среда: важное восстановление

Sandbox восстановился с source tree, **без** старых `.artifacts`, `build/native`, `.venv`, toolchains/dependencies/harnesses/logs. Старые process IDs не использовать. Git был на initial244d7d4 с почти всем деревом untracked. Уже безопасно исправлено: fetch точной ветки; ancestor check; SHA256-сверка2369 remote files(0missing/16different); **`git reset --mixed FETCH_HEAD`** восстановил history/index доe2fdaef **без изменения working files/branch**. Не повторять reset/recovery и не терять pending work.

Теперь есть g++12.2/gcc,Python3.11/pip,gh; нет cmake/ninja/cargo/rustc/SDL/GL dev headers. Bundled zlib headers и runtime `/lib/x86_64-linux-gnu/libz.so.1` доступны. Новые `.artifacts` содержат только заново созданные decoder harness/scripts/logs. `build/local/replay-vault` содержит повторно скачанный pinned vault и DDNet18.9.1 character.cpp; `build/local/generated/src/generated` — настоящие generated protocol/client headers.

## Что реализовано в этом pending patch

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

## Свежая локальная проверка после восстановления

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

## Ранее реально исполнено (до потери ignored build/log files)

Результаты ниже исторические, **не новые reruns**. Native17 и18 **подтверждены exit0**, нет висящего старого build. Native18 включал Load button/read-only Save,но до zero-aim normalization.

- Native18: `PATH="$PWD/.venv/bin:$PWD/.artifacts/toolchain/rust/bin:$PATH" CARGO_HOME="$PWD/.artifacts/cargo" CARGO_NET_OFFLINE=true cmake --build build/native --target game-client bestclient-updater package_zip --parallel 2 -- -k 0` — **exit0**. Paths сейчас отсутствуют.
- **GitHub42875704110/e2fdaef SUCCESS Windows2022/MSVC+Ubuntu24.04**, rechecked current sandbox. <https://github.com/twin15388-beep/best-v2/actions/runs/42875704110>. **Не включает importer/vault.** Artifacts Windows13791573420/117783968bytes,Linux13791952700/116103698bytes,не downloaded/not releases. Earlier42873249161/bcef908 также pass.
- World/TAS lifecycle run5 ASan+UBSan+LSan **10165/0/exit0**,2real maps(coverage50×50,Tutorial2210×110),362handled allocfail(109+253),pre/post imports,full validation/neutral before validation,successful converted Save/readback и source/vault overwrite refusal. Uses actual CGameClient/CMap/world/TAS methods,не mocks; no renderer/server.
- HTTP/jobs run3 **26/0/exit0**,libcurl7.88.1,real GitHub HEAD/archive,cold179/repeat179verified,user replay preserved,pending069/playback blocked,failedHTTP,duplicate/cooldown/cancel/shutdown. Auto-sync toggle/cache-job-stage cancellation не отдельно проверены.
- VaultIO run4 **1707/0/exit0**,21hostile ZIP fixtures,179byte-identical,876allocs/346probes/303throws. libstdc++ directory_iterator OOM terminate реально найден→native traversal;cleanup6failures→bounded retry2→0failures.
- Decoder93fixtures+2samples **18836/0/exit0**,32Callocfail,leak2489bytes/51allocs найден→flat arena fixed. All179 pre-normalization **19146/0/exit0**,49accepted/130rejected. Новый normalized batch выше supersedes acceptance.
- Package/input logic7 **9459/0/exit0**; generator2 **50/0/exit0**. Actual ed5a7791026file Python→C++ Extract/Verify/Install **exit0**:exception rollback,actual binary readback,settings/replay preservation,backup,tamper. Новый package regression ещё нужен. Generator source_sha не доказывает binary reproducibility/signature.
- Independent physics8 **8357/0/exit0**; canonical Best ordering comparisons,serial/parallel/detached contexts/PRNG/COWdoors. Character states,не full world bytes/server equivalence. Win updater Zig actual x64 GUI cross-link **exit0**,не Windows runtime; real MSVC CI отдельно выше.
- Actual Linux client launch processexit0,**Failed to initialize the graphics/OpenGL**. Не считать renderer smoke. No X/Wine/MSVC locally; benchmark command/TSan не запускались.

Если заново строить world harness: использовать реальные native link objects; engine/client/client.cpp.o сохранять с `main→MalinkaClientMain` через objcopy,не выкидывать; CLayers.Init(Map,false,true); пересобирать все CGameClient/TAS layout consumers после header changes. Старые non-idempotent injection scripts не запускать/не применять повторно к уже исправленному коду. Engine console harness cleanup был test-only,production не менять ради LSan.

## Что ещё не сделано / ближайшие проверяемые шаги

1. Commit/push pending source+docs **только** `arena/733fc6a9-best-v2`; fresh Windows/MSVC и Linux Actions run. Не приписывать e2fdaef success новой работе. Нормальные exact commands/dependency pins в `docs/MALINKA_BUILD.md`/workflow. Исправлять реальные compile errors, читать exit/checks.
2. Повторить actual checked-package C++ installation/rollback/readback/preservation/tamper на **новом** artifact/portable stage в disposable installs. Если download с GitHub artifact redirect недоступен, зафиксировать это,не заявлять pass; не публиковать release/tag преждевременно.
3. Текущий vault parser honest rejects conditions/dummy schemas. Исследовать их поля/семантику,не игнорировать conditions и не выдумывать world. Авто-sync toggle/cache-job-stage cancel отдельная execution regression ещё нужна.
4. **71 remaining gated configs**,full Grenade saver/advanced movement/Avoid/Fent Light+jump visuals/chat`!help/!skin`/RPC/wiring и exact1.32parity неполны. Malinka RPCIDs неизвестны,BestID1444576875774083133 не KRX. Laser negativeRes=-1 не reproduced; не закрывать по1000/0logic alone.
5. Windows11 x64 runtime на отдельной install/profile:renderer+allmenu sections,focus/keyrelease,dummy/TAS/door/tele/tune/events,Insert на открытой странице/search/reconnect/restart. Vault Sync→Reload→Load→Validate matching map/start→new Save outside vault→readback;negativecases preserve buffer/raw/snapshot. Не проверено в игре.
6. Exact KRX acquisition recipe в STATUS:Windows VM,actual EXE hash/module identity/SizeOfImage/path/version;record UI/config defaults/input-output traces/replay per option on same maps. RVA≠VA without measured base. Полный original source user повторно не спрашивать.
7. Genuine remote badge:authorization внешнего Best service +second unmodified client observation. Public pipeline не имеет BESTCLIENT_OFFICIAL_CODE/private secret;local suppress fake≠requirement. Update channel own GitHub releases endpoint есть,операционных releases/deployment/restartproof пока нет.

Документы: `docs/MALINKA_STATUS.md` (current71controls/limitations/manualrecipe),`SOURCES`(pins/formatprovenance),`BUILD`(MSVC/Linux/packagecommands),`MALINKA_DEPENDENCIES.json`(SDKversions),`README.md`(honestincompletefork). Все generated fixtures/downloads/builds ignored,не добавлять тяжёлые artifacts/секреты в Git.
