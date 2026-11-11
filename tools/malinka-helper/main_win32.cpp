// SPDX-License-Identifier: MIT
#include "win32.h"

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <thread>

using namespace malinka::helper;
namespace
{
constexpr UINT WORK_DONE = WM_APP + 1;
constexpr UINT_PTR TIMER = 1;
enum : int { CHOOSE = 101, LAUNCH, ATTACH, SAVE, PIN, PID_EDIT, PATH_TEXT, STATE_TEXT, TELEMETRY_TEXT };

std::optional<DWORD> ParsePid(std::wstring_view value)
{
	if(value.empty() || value.size() > 10) return std::nullopt;
	std::uint64_t number = 0;
	for(wchar_t c : value)
	{
		if(c < L'0' || c > L'9') return std::nullopt;
		number = number * 10 + static_cast<unsigned>(c - L'0');
		if(number > MAXDWORD) return std::nullopt;
	}
	if(number == 0) return std::nullopt;
	return static_cast<DWORD>(number);
}

struct App
{
	HWND window{};
	HFONT font{};
	std::array<HWND, 9> controls{};
	std::wstring client_path;
	Report report;
	MumbleReader mumble;
	std::mutex mutex;
	std::optional<Report> pending;
	std::jthread worker;
	bool busy{false};

	~App() { worker.request_stop(); if(worker.joinable()) worker.join(); if(font) DeleteObject(font); }
	HWND Control(int id) const { return controls[static_cast<std::size_t>(id - CHOOSE)]; }
	void Message(std::string_view text) const
	{
		MessageBoxW(window, Wide(text).c_str(), L"MalinkaHelper — диагностика", MB_OK | MB_ICONINFORMATION);
	}
	void Font()
	{
		const auto dpi = GetDpiForWindow(window);
		HFONT next = CreateFontW(-MulDiv(10, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		if(!next) return;
		for(HWND control : controls) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(next), TRUE);
		if(font) DeleteObject(font); font = next;
	}
	void Layout()
	{
		RECT rect{}; GetClientRect(window, &rect);
		const auto scale = [this](int n) { return MulDiv(n, static_cast<int>(GetDpiForWindow(window)), 96); };
		const int gap = scale(10), h = scale(29), width = rect.right;
		MoveWindow(Control(CHOOSE), gap, gap, scale(182), h, TRUE);
		MoveWindow(Control(LAUNCH), gap + scale(192), gap, scale(165), h, TRUE);
		MoveWindow(Control(PIN), gap + scale(367), gap, scale(160), h, TRUE);
		MoveWindow(Control(PATH_TEXT), gap, gap * 2 + h, std::max(1, width - 2 * gap), h * 2, TRUE);
		const int second = gap * 3 + h * 3;
		MoveWindow(Control(PID_EDIT), gap, second, scale(100), h, TRUE);
		MoveWindow(Control(ATTACH), gap + scale(110), second, scale(180), h, TRUE);
		MoveWindow(Control(SAVE), gap + scale(300), second, scale(180), h, TRUE);
		MoveWindow(Control(TELEMETRY_TEXT), gap, second + h + gap, std::max(1, width - 2 * gap), scale(64), TRUE);
		const int top = second + h + gap * 2 + scale(64);
		MoveWindow(Control(STATE_TEXT), gap, top, std::max(1, width - 2 * gap), std::max(1, static_cast<int>(rect.bottom) - top - gap), TRUE);
	}
	void Display()
	{
		std::wstring text = L"АВТОМАТИЧЕСКИЙ AVOID FREEZE: НЕ ГОТОВ / ЗАБЛОКИРОВАН\r\n"
			L"Это отдельный диагностический помощник, не модифицированный клиент.\r\n"
			L"Нет записи в процесс, DLL-инъекций, отправки клавиш или подмены badge.\r\n\r\n";
		if(busy) text += L"Идёт ограниченная проверка выбранного процесса…\r\n\r\n";
		if(!report.file.path.empty())
		{
			text += L"EXE: " + Wide(report.file.path) + L"\r\nSHA-256: " + Wide(report.file.sha256) + L"\r\n";
			text += report.file.recognized_public_release ? L"Образ совпадает с проверенным публичным official v3.1.1.\r\n" : L"Точный образ не распознан: игровые адреса недоступны. Имя/версия не заменяют проверку SHA.\r\n";
			if(!report.file.error.empty()) text += L"Ошибка образа: " + Wide(report.file.error) + L"\r\n";
		}
		if(report.process)
		{
			const auto &p = *report.process;
			text += L"\r\nPID: " + std::to_wstring(p.pid) + L"; main module: " + (UserRange(p.module_base, p.module_size) ? Wide(Hex(p.module_base)) : L"unavailable") + L"\r\n";
			text += L"Проверка заголовков загруженного образа: " + std::wstring(p.loaded_headers_match ? L"совпали" : L"не подтверждены") + L"\r\n";
			text += L"Поиск CGameClient: " + Wide(p.scan.status) + L"; кандидатов: " + std::to_wstring(p.scan.candidates.size()) + L"\r\n";
			text += Wide(p.scan.reason) + L"\r\n";
			if(!p.error.empty()) text += L"Ошибка процесса: " + Wide(p.error) + L"\r\n";
		}
		text += L"\r\nКандидат по vtable НЕ означает проверенное состояние персонажа.\r\n"
			L"Нет подтверждённых полей физики/карты/настроек ввода — управление не включается.\r\n"
			L"Наличие официального значка у других игроков отдельно не проверено.\r\n"
			L"Отчёт сохраняется только по вашему выбору, под новым именем, вне папки клиента.\r\n"
			L"Закрытие помощника не завершает клиент.\r\n";
		SetWindowTextW(Control(STATE_TEXT), text.c_str());
		EnableWindow(Control(CHOOSE), !busy); EnableWindow(Control(LAUNCH), !busy && !client_path.empty());
		EnableWindow(Control(ATTACH), !busy); EnableWindow(Control(SAVE), !busy && !report.file.path.empty());
	}
	void Sample()
	{
		report.telemetry = mumble.Sample(); report.mapping_created_by_helper = mumble.Created();
		const auto &t = report.telemetry;
		std::wstring text = L"Mumble Link: " + Wide(t.status);
		if(t.x && t.y) text += L"; render X=" + std::to_wstring(*t.x) + L", Y=" + std::to_wstring(*t.y);
		text += L"\r\nТолько render-телеметрия, не физика; автор записи не привязан к PID. Если пусто: F1 → mumble_reconnect.";
		SetWindowTextW(Control(TELEMETRY_TEXT), text.c_str());
	}
	void Choose()
	{
		std::array<wchar_t, 32768> path{};
		OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
		dialog.lpstrFilter = L"Windows executable (*.exe)\0*.exe\0\0"; dialog.lpstrFile = path.data(); dialog.nMaxFile = static_cast<DWORD>(path.size());
		dialog.lpstrTitle = L"Выберите настоящий официальный клиент (его файлы не изменяются)";
		dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
		if(!GetOpenFileNameW(&dialog)) return;
		client_path = path.data(); SetWindowTextW(Control(PATH_TEXT), client_path.c_str());
		Start(0, client_path);
	}
	void Start(DWORD pid, std::wstring path = {})
	{
		if(busy) return;
		if(worker.joinable()) worker.join();
		busy = true; Display();
		worker = std::jthread([this, pid, path = std::move(path)](std::stop_token stop) {
			Report next;
			try
			{
				if(pid) next = InspectProcess(pid, stop);
				else { next.measured_at_utc = UtcNow(); next.file = InspectFile(path); }
			}
			catch(const std::exception &e) { next.file.error = e.what(); }
			catch(...) { next.file.error = "Unexpected inspection failure"; }
			{ std::lock_guard lock(mutex); pending = std::move(next); }
			PostMessageW(window, WORK_DONE, 0, 0);
		});
	}
	void Complete()
	{
		{ std::lock_guard lock(mutex); if(pending) { report = std::move(*pending); pending.reset(); } }
		busy = false; Sample(); Display();
	}
	void Attach()
	{
		std::array<wchar_t, 32> text{}; GetWindowTextW(Control(PID_EDIT), text.data(), static_cast<int>(text.size()));
		const auto pid = ParsePid(text.data());
		if(!pid) { Message("Enter a nonzero Windows process ID (PID), not an address"); return; }
		Start(*pid);
	}
	void Launch()
	{
		if(client_path.empty() || busy) return;
		// Explicit user action; use the selected unmodified EXE, no extra flags.
		std::string error;
		const auto pid = LaunchClient(client_path, error);
		if(!pid) { Message(error); return; }
		SetWindowTextW(Control(PID_EDIT), std::to_wstring(*pid).c_str());
		Start(*pid);
	}
	void Save()
	{
		std::array<wchar_t, 32768> path{};
		constexpr wchar_t DEFAULT_NAME[] = L"MalinkaHelper-report.json";
		std::copy(std::begin(DEFAULT_NAME), std::end(DEFAULT_NAME), path.begin());
		OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
		dialog.lpstrFilter = L"JSON report (*.json)\0*.json\0\0"; dialog.lpstrFile = path.data(); dialog.nMaxFile = static_cast<DWORD>(path.size());
		dialog.lpstrDefExt = L"json"; dialog.lpstrTitle = L"НОВЫЙ отчёт вне папки клиента (существующие файлы не заменяются)";
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
		if(!GetSaveFileNameW(&dialog)) return;
		Sample();
		std::string error;
		if(!WriteNewReport(path.data(), report, error)) Message(error);
		else Message("Diagnostic report saved. Automatic Avoid Freeze remains BLOCKED.");
	}
};

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	App *app = reinterpret_cast<App *>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if(message == WM_NCCREATE)
	{
		app = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
		app->window = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
	}
	if(!app) return DefWindowProcW(window, message, wparam, lparam);
	try
	{
		switch(message)
		{
		case WM_CREATE:
		{
			const HINSTANCE instance = GetModuleHandleW(nullptr);
			struct Spec { int id; const wchar_t *type, *text; DWORD style; };
			const std::array<Spec, 9> specs{{
				{CHOOSE, L"BUTTON", L"Выбрать официальный EXE", BS_PUSHBUTTON},
				{LAUNCH, L"BUTTON", L"Запустить клиент", BS_PUSHBUTTON},
				{ATTACH, L"BUTTON", L"Проверить PID ещё раз", BS_PUSHBUTTON},
				{SAVE, L"BUTTON", L"Сохранить новый отчёт", BS_PUSHBUTTON},
				{PIN, L"BUTTON", L"Поверх окон", BS_AUTOCHECKBOX},
				{PID_EDIT, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL},
				{PATH_TEXT, L"STATIC", L"Выберите EXE или введите PID уже запущенного клиента.", SS_LEFT},
				{STATE_TEXT, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL},
				{TELEMETRY_TEXT, L"STATIC", L"", SS_LEFT}}};
			for(const auto &s : specs)
			{
				app->controls[static_cast<std::size_t>(s.id - CHOOSE)] = CreateWindowExW(0, s.type, s.text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | s.style,
					0, 0, 1, 1, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(s.id)), instance, nullptr);
				if(!app->Control(s.id)) return -1;
			}
			SendMessageW(app->Control(PID_EDIT), EM_SETLIMITTEXT, 10, 0);
			std::string error; if(!app->mumble.Open(error)) app->report.telemetry.reason = error;
			app->Font(); app->Layout(); app->Display(); app->Sample();
			if(!SetTimer(window, TIMER, 100, nullptr)) return -1;
			return 0;
		}
		case WM_COMMAND:
			if(HIWORD(wparam) != BN_CLICKED) break;
			switch(LOWORD(wparam))
			{
			case CHOOSE: app->Choose(); break;
			case LAUNCH: app->Launch(); break;
			case ATTACH: app->Attach(); break;
			case SAVE: app->Save(); break;
			case PIN: SetWindowPos(window, SendMessageW(app->Control(PIN), BM_GETCHECK, 0, 0) == BST_CHECKED ? HWND_TOPMOST : HWND_NOTOPMOST,
				0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); break;
			}
			return 0;
		case WORK_DONE: app->Complete(); return 0;
		case WM_TIMER: if(wparam == TIMER) app->Sample(); return 0;
		case WM_SIZE: app->Layout(); return 0;
		case WM_DPICHANGED:
		{
			const auto *rect = reinterpret_cast<const RECT *>(lparam);
			SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
			app->Font(); app->Layout(); return 0;
		}
		case WM_GETMINMAXINFO:
		{
			auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
			const int dpi = static_cast<int>(GetDpiForWindow(window));
			info->ptMinTrackSize = {MulDiv(610, dpi, 96), MulDiv(440, dpi, 96)};
			return 0;
		}
		case WM_CLOSE:
			KillTimer(window, TIMER); app->worker.request_stop(); if(app->worker.joinable()) app->worker.join();
			DestroyWindow(window); return 0;
		case WM_DESTROY: PostQuitMessage(0); return 0;
		case WM_NCDESTROY: SetWindowLongPtrW(window, GWLP_USERDATA, 0); break;
		}
	}
	catch(const std::exception &e) { app->Message(e.what()); }
	catch(...) { MessageBoxW(window, L"Unexpected helper error. No automatic input is enabled.", L"MalinkaHelper", MB_OK | MB_ICONERROR); }
	return DefWindowProcW(window, message, wparam, lparam);
}

int Cli(int count, wchar_t **args)
{
	std::wstring inspect, destination;
	std::optional<DWORD> pid;
	for(int i = 1; i < count; ++i)
	{
		const std::wstring_view option(args[i]);
		if(i + 1 >= count) return 2;
		if(option == L"--inspect-file" && inspect.empty() && !pid) inspect = args[++i];
		else if(option == L"--pid" && inspect.empty() && !pid) { pid = ParsePid(args[++i]); if(!pid) return 2; }
		else if(option == L"--report" && destination.empty()) destination = args[++i];
		else return 2;
	}
	if((inspect.empty() && !pid) || destination.empty()) return 2;
	Report report;
	if(pid) report = InspectProcess(*pid);
	else { report.measured_at_utc = UtcNow(); report.file = InspectFile(inspect); }
	std::string error;
	if(!WriteNewReport(destination, report, error)) return 3;
	if(!report.file.error.empty() || (report.process && !report.process->error.empty())) return 4;
	// Diagnostic success does NOT mean an Avoid implementation is available.
	return 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
	try
	{
		int count{}; wchar_t **raw = CommandLineToArgvW(GetCommandLineW(), &count);
		if(!raw) return 2;
		struct ArgsGuard { wchar_t **p; ~ArgsGuard() { LocalFree(p); } } guard{raw};
		if(count > 1) return Cli(count, raw);
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		App app;
		WNDCLASSEXW type{}; type.cbSize = sizeof(type); type.hInstance = instance; type.lpfnWndProc = WindowProc;
		type.hCursor = LoadCursorW(nullptr, IDC_ARROW); type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); type.lpszClassName = L"MalinkaHelperDiagnostic";
		if(!RegisterClassExW(&type)) return 5;
		HWND window = CreateWindowExW(WS_EX_APPWINDOW, type.lpszClassName, L"MalinkaHelper — отдельная диагностика, НЕ готовый Avoid",
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 870, 650, nullptr, nullptr, instance, &app);
		if(!window) return 5;
		ShowWindow(window, show); UpdateWindow(window);
		MSG message{};
		int result{};
		while((result = GetMessageW(&message, nullptr, 0, 0)) > 0)
		{
			if(!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
		}
		return result == -1 ? 5 : static_cast<int>(message.wParam);
	}
	catch(...) { return 5; }
}
