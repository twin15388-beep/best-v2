#ifndef GAME_CLIENT_COMPONENTS_MALINKA_FILE_SAFETY_H
#define GAME_CLIENT_COMPONENTS_MALINKA_FILE_SAFETY_H

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Malinka::Files
{
namespace fs = std::filesystem;

inline std::string Utf8(const fs::path &Path)
{
	const auto Text = Path.generic_u8string();
	return std::string(Text.begin(), Text.end());
}

// One unambiguous relative name on BOTH supported desktop platforms. In
// particular, do not let Windows collapse CON, trailing dots/spaces, or colons.
inline bool SafeRelativePath(const std::string &Text)
{
	if(Text.empty() || Text.size() > 240 || Text.front() == '/' || Text.back() == '/' || Text.find("//") != std::string::npos ||
		Text.find_first_of("\\:*?\"<>|") != std::string::npos ||
		std::any_of(Text.begin(), Text.end(), [](unsigned char C) { return C < 32 || C == 127; }))
		return false;
	// Check UTF-8 before asking the Windows filesystem to convert it.
	for(size_t Index = 0; Index < Text.size();)
	{
		const unsigned char First = Text[Index++];
		if(First < 128)
			continue;
		int Extra;
		uint32_t Code;
		uint32_t Minimum;
		if(First >= 0xc2 && First <= 0xdf)
			Extra = 1, Code = First & 0x1f, Minimum = 0x80;
		else if(First >= 0xe0 && First <= 0xef)
			Extra = 2, Code = First & 0x0f, Minimum = 0x800;
		else if(First >= 0xf0 && First <= 0xf4)
			Extra = 3, Code = First & 7, Minimum = 0x10000;
		else
			return false;
		if(Index + Extra > Text.size())
			return false;
		while(Extra--)
		{
			const unsigned char Next = Text[Index++];
			if((Next & 0xc0) != 0x80)
				return false;
			Code = (Code << 6) | (Next & 0x3f);
		}
		if(Code < Minimum || Code > 0x10ffff || (Code >= 0xd800 && Code <= 0xdfff))
			return false;
	}
	for(const auto &Part : fs::u8path(Text))
	{
		const std::string Name = Utf8(Part);
		if(Name.empty() || Name == "." || Name == ".." || Name.back() == '.' || Name.back() == ' ')
			return false;
		std::string Base = Name.substr(0, Name.find('.'));
		std::transform(Base.begin(), Base.end(), Base.begin(), [](unsigned char C) { return C >= 'a' && C <= 'z' ? C - 'a' + 'A' : C; });
		if(Base == "CON" || Base == "NUL" || Base == "PRN" || Base == "AUX" ||
			(Base.size() == 4 && (Base.substr(0, 3) == "COM" || Base.substr(0, 3) == "LPT") && Base[3] >= '0' && Base[3] <= '9'))
			return false;
	}
	return true;
}

inline bool NoLinks(const fs::path &Path)
{
	std::error_code Ec;
	const auto Absolute = fs::absolute(Path, Ec);
	if(Ec)
		return false;
	fs::path Current;
	for(const auto &Part : Absolute)
	{
		Current /= Part;
		const auto Status = fs::symlink_status(Current, Ec);
		if(Ec && Ec != std::errc::no_such_file_or_directory)
			return false;
		Ec.clear();
		if(fs::is_symlink(Status))
			return false;
#if defined(_WIN32)
		const DWORD Attributes = GetFileAttributesW(Current.c_str());
		if(Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_REPARSE_POINT))
			return false;
#endif
	}
	return true;
}

inline bool ReadBounded(const fs::path &Path, size_t Limit, std::string &Out)
{
	std::error_code Ec;
	if(!fs::is_regular_file(Path, Ec) || Ec)
		return false;
	const auto Size = fs::file_size(Path, Ec);
	if(Ec || Size > Limit)
		return false;
	std::ifstream In(Path, std::ios::binary);
	if(!In)
		return false;
	std::string Data(static_cast<size_t>(Size), '\0');
	In.read(Data.data(), static_cast<std::streamsize>(Data.size()));
	if(In.gcount() != static_cast<std::streamsize>(Size) || In.peek() != std::char_traits<char>::eof() || In.bad())
		return false;
	Out = std::move(Data);
	return true;
}

// CREATE_NEW / O_EXCL prevents a stale temporary file, symlink, or hardlink
// from being opened for truncation. Flush the complete payload before rename.
inline bool WriteExclusive(const fs::path &Path, const std::string &Data)
{
	if(!NoLinks(Path))
		return false;
#if defined(_WIN32)
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
#else
	const int File = open(Path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
	if(File < 0)
		return false;
	bool Ok = true;
	size_t Offset = 0;
	while(Offset < Data.size())
	{
		const ssize_t Written = write(File, Data.data() + Offset, Data.size() - Offset);
		if(Written < 0 && errno == EINTR)
			continue;
		if(Written <= 0)
		{
			Ok = false;
			break;
		}
		Offset += static_cast<size_t>(Written);
	}
	Ok = fsync(File) == 0 && Ok;
	Ok = close(File) == 0 && Ok;
	if(!Ok)
		unlink(Path.c_str());
	return Ok;
#endif
}

inline bool AtomicReplace(const fs::path &From, const fs::path &To)
{
#if defined(_WIN32)
	return MoveFileExW(From.c_str(), To.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	std::error_code Ec;
	fs::rename(From, To, Ec);
	return !Ec;
#endif
}
}

#endif
