// SPDX-License-Identifier: MIT
#ifndef MALINKA_HELPER_WIN32_H
#define MALINKA_HELPER_WIN32_H

#include "core.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stop_token>
#include <utility>

namespace malinka::helper
{
class Handle
{
	HANDLE m_value{nullptr};
public:
	Handle() = default;
	explicit Handle(HANDLE value) : m_value(value) {}
	~Handle() { Reset(); }
	Handle(const Handle &) = delete;
	Handle &operator=(const Handle &) = delete;
	Handle(Handle &&other) noexcept : m_value(std::exchange(other.m_value, nullptr)) {}
	Handle &operator=(Handle &&other) noexcept { if(this != &other) { Reset(); m_value = std::exchange(other.m_value, nullptr); } return *this; }
	void Reset(HANDLE value = nullptr) { if(m_value && m_value != INVALID_HANDLE_VALUE) CloseHandle(m_value); m_value = value; }
	HANDLE Get() const { return m_value; }
	explicit operator bool() const { return m_value && m_value != INVALID_HANDLE_VALUE; }
};
std::string Utf8(std::wstring_view value);
std::wstring Wide(std::string_view value);
std::string WinError(std::string_view operation, DWORD error = GetLastError());
std::string UtcNow();
std::uint64_t MonotonicMs();

FileReport InspectFile(const std::wstring &path);
Report InspectProcess(DWORD pid, std::stop_token stop = {});
// This function ONLY reads. Partial reads and non-user-mode ranges fail.
bool ReadExact(HANDLE process, std::uint64_t address, std::span<std::byte> output);
ScanReport ScanPrivatePointers(HANDLE process, std::uint64_t pointer, std::stop_token stop = {});

// Returns a PID, not an all-access process handle. No parameters/config files
// are added, no process is suspended/patched, and helper exit never kills it.
std::optional<DWORD> LaunchClient(const std::wstring &path, std::string &error);

class MumbleReader
{
	Handle m_mapping;
	const void *m_view{nullptr};
	bool m_created{false};
	TelemetryTracker m_tracker;
public:
	~MumbleReader();
	MumbleReader() = default;
	MumbleReader(const MumbleReader &) = delete;
	MumbleReader &operator=(const MumbleReader &) = delete;
	bool Open(std::string &error);
	Telemetry Sample();
	bool Created() const { return m_created; }
};

// New explicit report ONLY; never overwrites. Rejects the selected client's
// directory (including resolved junction/symlink destinations).
bool WriteNewReport(const std::wstring &path, const Report &report, std::string &error);
} // namespace malinka::helper
#endif
