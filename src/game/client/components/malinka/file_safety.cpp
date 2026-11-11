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

bool ListDirectory(const fs::path &Path, size_t MaxEntries, std::vector<SDirectoryEntry> &vOut)
{
	if(!NoLinks(Path))
		return false;
	WIN32_FIND_DATAW Data;
	const HANDLE Handle = FindFirstFileW((Path / L"*").c_str(), &Data);
	if(Handle == INVALID_HANDLE_VALUE)
	{
		if(GetLastError() != ERROR_FILE_NOT_FOUND || !fs::is_directory(Path))
			return false;
		vOut.clear();
		return true;
	}
	struct CFindHandle
	{
		HANDLE m_Handle;
		~CFindHandle() { FindClose(m_Handle); }
	} Guard{Handle};
	std::vector<SDirectoryEntry> vEntries;
	for(;;)
	{
		const auto *pName = Data.cFileName;
		if(!(pName[0] == L'.' && (pName[1] == L'\0' || (pName[1] == L'.' && pName[2] == L'\0'))))
		{
			if(vEntries.size() >= MaxEntries)
				return false;
			const auto Kind = (Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? EDirectoryEntryKind::Link :
				(Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? EDirectoryEntryKind::Directory :
				(Data.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) ? EDirectoryEntryKind::Other : EDirectoryEntryKind::RegularFile;
			vEntries.push_back({Path / pName, Kind});
		}
		if(!FindNextFileW(Handle, &Data))
		{
			if(GetLastError() != ERROR_NO_MORE_FILES)
				return false;
			break;
		}
	}
	vOut = std::move(vEntries);
	return true;
}

bool MoveNoReplace(const fs::path &From, const fs::path &To)
{
	return MoveFileExW(From.c_str(), To.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
}

bool AtomicReplace(const fs::path &From, const fs::path &To)
{
	return MoveFileExW(From.c_str(), To.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
}
#endif

#if defined(__linux__)
#include <sys/syscall.h>
#include <linux/fs.h>

namespace Malinka::Files
{
bool MoveNoReplace(const fs::path &From, const fs::path &To)
{
	// RENAME_NOREPLACE is supported by Linux >= 3.15. Fail closed on a
	// filesystem/kernel without it instead of falling back to replacing rename.
	return syscall(SYS_renameat2, AT_FDCWD, From.c_str(), AT_FDCWD, To.c_str(), RENAME_NOREPLACE) == 0;
}
}
#elif !defined(_WIN32)
namespace Malinka::Files
{
bool MoveNoReplace(const fs::path &, const fs::path &)
{
	return false; // Cache publication is currently supported on Windows/Linux.
}
}
#endif

#if !defined(_WIN32)
#include <dirent.h>
#include <sys/stat.h>

namespace Malinka::Files
{
bool ListDirectory(const fs::path &Path, size_t MaxEntries, std::vector<SDirectoryEntry> &vOut)
{
	if(!NoLinks(Path))
		return false;
	DIR *pDir = opendir(Path.c_str());
	if(!pDir)
		return false;
	struct CDirectoryHandle
	{
		DIR *m_pDir;
		~CDirectoryHandle() { closedir(m_pDir); }
	} Guard{pDir};
	std::vector<SDirectoryEntry> vEntries;
	for(;;)
	{
		errno = 0;
		const dirent *pEntry = readdir(pDir);
		if(!pEntry)
		{
			if(errno)
				return false;
			break;
		}
		const char *pName = pEntry->d_name;
		if(pName[0] == '.' && (pName[1] == '\0' || (pName[1] == '.' && pName[2] == '\0')))
			continue;
		if(vEntries.size() >= MaxEntries)
			return false;
		struct stat Status;
		if(fstatat(dirfd(pDir), pName, &Status, AT_SYMLINK_NOFOLLOW) != 0)
			return false;
		const auto Kind = S_ISLNK(Status.st_mode) ? EDirectoryEntryKind::Link :
			S_ISDIR(Status.st_mode) ? EDirectoryEntryKind::Directory :
			S_ISREG(Status.st_mode) ? EDirectoryEntryKind::RegularFile : EDirectoryEntryKind::Other;
		vEntries.push_back({Path / pName, Kind});
	}
	vOut = std::move(vEntries);
	return true;
}
}
#endif

namespace Malinka::Files
{
bool RemoveTreeBounded(const fs::path &Path, size_t MaxEntries, int MaxDepth)
{
	if(!NoLinks(Path) || MaxEntries == 0 || MaxDepth < 0)
		return false;
	struct SNode { fs::path m_Path; int m_Depth; bool m_Expanded; };
	std::vector<SNode> vPending = {{Path, 0, false}};
	size_t Visited = 1;
	while(!vPending.empty())
	{
		auto Node = std::move(vPending.back());
		vPending.pop_back();
		std::error_code Ec;
		if(Node.m_Expanded)
		{
			fs::remove(Node.m_Path, Ec);
			if(Ec)
				return false;
			continue;
		}
		std::vector<SDirectoryEntry> vEntries;
		if(Node.m_Depth > MaxDepth || !ListDirectory(Node.m_Path, MaxEntries - Visited, vEntries))
			return false;
		Visited += vEntries.size();
		vPending.push_back({std::move(Node.m_Path), Node.m_Depth, true});
		for(auto &Entry : vEntries)
		{
			if(Entry.m_Kind == EDirectoryEntryKind::Directory)
				vPending.push_back({std::move(Entry.m_Path), Node.m_Depth + 1, false});
			else
			{
				fs::remove(Entry.m_Path, Ec);
				if(Ec)
					return false;
			}
		}
	}
	return true;
}
}
