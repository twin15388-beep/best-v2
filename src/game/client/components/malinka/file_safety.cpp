// Platform file operations for BestClient-Malinka. Keep Win32 macros local.
#include "file_safety.h"

#if defined(_WIN32)
#include <windows.h>

namespace Malinka::Files
{
bool NoReparsePoint(const fs::path &Path)
{
	const DWORD Attributes = GetFileAttributesW(Path.c_str());
	if(Attributes == INVALID_FILE_ATTRIBUTES)
	{
		const DWORD Error = GetLastError();
		return Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND;
	}
	return (Attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

bool WriteExclusive(const fs::path &Path, const std::string &Data)
{
	if(!NoLinks(Path))
		return false;
	HANDLE File = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(File == INVALID_HANDLE_VALUE)
		return false;
	bool Ok = true;
	size_t Offset = 0;
	while(Offset < Data.size())
	{
		const DWORD Wanted = static_cast<DWORD>(std::min<size_t>(Data.size() - Offset, 1048576));
		DWORD Written = 0;
		if(!WriteFile(File, Data.data() + Offset, Wanted, &Written, nullptr) || Written == 0)
		{
			Ok = false;
			break;
		}
		Offset += Written;
	}
	Ok = FlushFileBuffers(File) && Ok;
	Ok = CloseHandle(File) && Ok;
	if(!Ok)
		DeleteFileW(Path.c_str());
	return Ok;
}

bool AtomicReplace(const fs::path &From, const fs::path &To)
{
	return MoveFileExW(From.c_str(), To.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
}
#endif
