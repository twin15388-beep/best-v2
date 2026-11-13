# AvoidFreeze Proxy (version.dll) for BestClient 3.1.1

Стелс-перехватчик Avoid Freeze на основе техники DLL Proxying (`version.dll`) для официального исполняемого файла `DDNet.exe` (BestClient 3.1.1, x64).

## Архитектура и скрытность

1. **Неизменность клиента (`DDNet.exe`)**:
   - Сам `DDNet.exe` не изменяется ни на один байт. Контрольная сумма SHA-256 (`50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f`) и цифровая подпись остаются оригинальными.
   - Сетевое рукопожатие (`NETMSGTYPE_CL_ISDDNETLEGACY`, `CLIENT_VERSION`) отправляется штатным кодом клиента, поэтому сервер видит чистый официальный `BestClient`.

2. **Принцип работы DLL Proxy**:
   - `DDNet.exe` динамически импортирует системную `version.dll` (`GetFileVersionInfoSizeW`).
   - По правилам поиска DLL в Windows, при наличии `version.dll` в каталоге приложения загрузчик загружает её первой.
   - Наша `version.dll` перенаправляет все 17 функций Win32 Version API в настоящую `C:\Windows\System32\version.dll`.

3. **Синхронный VMT-перехват ввода (Tick 50 Гц)**:
   - Модуль находит vtable `CGameClient` по измеренному смещению RVA `19904440`.
   - Перехватывает слот 21 vtable — виртуальный метод `PrepareInputForSend(int *pData, int Size, bool Dummy)`.
   - Метод вызывается движком на каждом тике непосредственно перед упаковкой ввода в сетевой пакет `NETMSG_INPUT`.
   - `pData[0]` (`m_Direction`) проверяется на попадание в фриз (`TILE_FREEZE = 9`, `TILE_DFREEZE = 12`, `TILE_LFREEZE = 144`, `TILE_DEATH = 2`).
   - Если текущее направление ведёт в опасную зону, направление автоматически корректируется или сбрасывается.
   - Нулевая задержка (0 ms lag) и отсутствие внешних чтений памяти (`ReadProcessMemory` / `Win32 299`).

4. **Управление**:
   - **Горячая клавиша F8**: мгновенное включение / выключение Avoid Freeze прямо в игре.
   - Звуковой сигнал `MessageBeep` при переключении (высокий тон — включен, глухой тон — выключен).

## Сборка (Windows MSVC x64)

Требования: Visual Studio 2022 (x64 C++20), CMake 3.20+.

```powershell
cmake -S tools/avoid-freeze-proxy -B build/proxy -A x64
cmake --build build/proxy --config Release
```

На выходе: `build/proxy/Release/version.dll`.

## Установка

1. Скопируй собранный `version.dll` в папку с клиентом рядом с `DDNet.exe`.
2. Запусти `DDNet.exe` как обычно.
3. Помощник активируется автоматически при старте игры.
4. Для удаления достаточно просто удалить или переименовать `version.dll`.
