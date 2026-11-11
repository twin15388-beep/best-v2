#ifndef TOOLS_MALINKA_PACKAGE_ZIP_H
#define TOOLS_MALINKA_PACKAGE_ZIP_H

#include "malinka_package.h"

#include <zlib.h>

#include <limits>

namespace Malinka::Package
{
inline uint16_t ReadLe16(const unsigned char *p)
{
	return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
inline uint32_t ReadLe32(const unsigned char *p)
{
	return p[0] | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

struct SZipLimits
{
	uint64_t m_ArchiveBytes = MAX_FILE_SIZE;
	uint64_t m_MemberBytes = MAX_FILE_SIZE;
	uint64_t m_TotalBytes = MAX_TOTAL_SIZE;
	size_t m_Entries = MAX_FILES;
};

inline bool ExtractZip(const fs::path &Archive, const fs::path &Destination, std::string &Error, SZipLimits Limits = {})
{
	std::error_code Ec;
	const auto Size = fs::file_size(Archive, Ec);
	if(Ec || Size < 22 || Size > Limits.m_ArchiveBytes || !NoLinks(Archive) || !NoLinks(Destination))
		return Error = "Update archive is missing, oversized, or linked.", false;
	if(fs::exists(Destination, Ec) || Ec)
		return Error = "Refusing to reuse an existing extraction directory.", false;
	std::vector<unsigned char> vZip(static_cast<size_t>(Size));
	std::ifstream In(Archive, std::ios::binary);
	if(!In.read(reinterpret_cast<char *>(vZip.data()), static_cast<std::streamsize>(Size)) || In.peek() != std::char_traits<char>::eof() || In.bad())
		return Error = "Truncated update archive read.", false;
	const auto Span = [&](uint64_t Offset, uint64_t Count) { return Offset <= Size && Count <= Size - Offset; };
	uint64_t End = Size - 22;
	const uint64_t Minimum = Size > 65557 ? Size - 65557 : 0;
	bool Found = false;
	for(;;)
	{
		if(ReadLe32(vZip.data() + End) == 0x06054b50 && Span(End, 22) && End + 22 + ReadLe16(vZip.data() + End + 20) == Size)
		{
			Found = true;
			break;
		}
		if(End == Minimum)
			break;
		--End;
	}
	if(!Found)
		return Error = "Missing ZIP central-directory terminator.", false;
	const unsigned char *pEnd = vZip.data() + End;
	const uint16_t Count = ReadLe16(pEnd + 10);
	const uint32_t DirectorySize = ReadLe32(pEnd + 12);
	const uint32_t DirectoryOffset = ReadLe32(pEnd + 16);
	if(ReadLe16(pEnd + 4) || ReadLe16(pEnd + 6) || ReadLe16(pEnd + 8) != Count || Count == 0 || Count > Limits.m_Entries ||
		!Span(DirectoryOffset, DirectorySize) || static_cast<uint64_t>(DirectoryOffset) + DirectorySize != End)
		return Error = "Unsupported ZIP64/multidisk archive or invalid entry count.", false;
	struct SEntry
	{
		std::string m_Name;
		uint32_t m_Offset;
		uint32_t m_Packed;
		uint32_t m_Unpacked;
		uint32_t m_Crc;
		uint16_t m_Method;
		bool m_Directory;
		uint32_t m_Permissions;
	};
	std::vector<SEntry> vEntries;
	std::set<std::string> Names;
	uint64_t Total = 0, Cursor = DirectoryOffset;
	for(size_t Index = 0; Index < Count; ++Index)
	{
		if(!Span(Cursor, 46) || ReadLe32(vZip.data() + Cursor) != 0x02014b50)
			return Error = "Invalid ZIP central-directory entry.", false;
		const unsigned char *p = vZip.data() + Cursor;
		const uint16_t NameLength = ReadLe16(p + 28), ExtraLength = ReadLe16(p + 30), CommentLength = ReadLe16(p + 32);
		if(NameLength == 0 || NameLength > 241 || !Span(Cursor + 46, static_cast<uint64_t>(NameLength) + ExtraLength + CommentLength))
			return Error = "Invalid ZIP name/extra-field length.", false;
		std::string Name(reinterpret_cast<const char *>(p + 46), NameLength);
		const bool Directory = Name.back() == '/';
		if(Directory)
			Name.pop_back();
		const unsigned Mode = (ReadLe32(p + 38) >> 16) & 0170000;
		if(!SafeRelativePath(Name) || (ReadLe16(p + 8) & (1 | 64)) || ReadLe16(p + 34) ||
			(Mode && Mode != (Directory ? 0040000u : 0100000u)))
			return Error = "Unsafe ZIP path, encryption, link, or special file.", false;
		std::string Canonical = Name;
		std::transform(Canonical.begin(), Canonical.end(), Canonical.begin(), [](unsigned char C) { return C >= 'A' && C <= 'Z' ? C - 'A' + 'a' : C; });
		if(!Names.insert(Canonical).second)
			return Error = "Duplicate/aliased ZIP entry.", false;
		SEntry Entry{Name, ReadLe32(p + 42), ReadLe32(p + 20), ReadLe32(p + 24), ReadLe32(p + 16), ReadLe16(p + 10), Directory, (ReadLe32(p + 38) >> 16) & 0777};
		if((Entry.m_Method != 0 && Entry.m_Method != 8) || Entry.m_Unpacked > Limits.m_MemberBytes ||
			(Directory && (Entry.m_Packed || Entry.m_Unpacked)) || Total > Limits.m_TotalBytes || Entry.m_Unpacked > Limits.m_TotalBytes - Total)
			return Error = "Unsupported ZIP compression or decompression limit exceeded.", false;
		Total += Entry.m_Unpacked;
		vEntries.push_back(std::move(Entry));
		Cursor += 46ull + NameLength + ExtraLength + CommentLength;
	}
	if(Cursor != static_cast<uint64_t>(DirectoryOffset) + DirectorySize)
		return Error = "ZIP directory size mismatch.", false;
	fs::create_directories(Destination, Ec);
	if(Ec)
		return Error = "Cannot create isolated extraction directory.", false;
	for(const auto &Entry : vEntries)
	{
		const auto Path = Destination / fs::u8path(Entry.m_Name);
		if(!NoLinks(Path))
			return Error = "Extraction path was linked/replaced.", false;
		if(Entry.m_Directory)
		{
			fs::create_directories(Path, Ec);
			if(Ec)
				return Error = "Cannot create extracted directory.", false;
			continue;
		}
		if(!Span(Entry.m_Offset, 30) || ReadLe32(vZip.data() + Entry.m_Offset) != 0x04034b50)
			return Error = "Invalid ZIP local-file header.", false;
		const unsigned char *pLocal = vZip.data() + Entry.m_Offset;
		const uint16_t NameLength = ReadLe16(pLocal + 26), ExtraLength = ReadLe16(pLocal + 28);
		const uint64_t DataOffset = Entry.m_Offset + 30ull + NameLength + ExtraLength;
		if(!Span(Entry.m_Offset + 30, NameLength) || !Span(DataOffset, Entry.m_Packed) ||
			DataOffset + Entry.m_Packed > DirectoryOffset || (ReadLe16(pLocal + 6) & (1 | 64)) || ReadLe16(pLocal + 8) != Entry.m_Method ||
			std::string(reinterpret_cast<const char *>(pLocal + 30), NameLength) != Entry.m_Name)
			return Error = "ZIP local/central entry mismatch.", false;
		std::vector<unsigned char> vContent(std::max<uint32_t>(1, Entry.m_Unpacked));
		if(Entry.m_Method == 0)
		{
			if(Entry.m_Packed != Entry.m_Unpacked)
				return Error = "Invalid stored ZIP entry size.", false;
			std::copy_n(vZip.data() + DataOffset, Entry.m_Packed, vContent.begin());
		}
		else
		{
			z_stream Stream{};
			Stream.next_in = const_cast<Bytef *>(vZip.data() + DataOffset);
			Stream.avail_in = Entry.m_Packed;
			Stream.next_out = vContent.data();
			Stream.avail_out = static_cast<uInt>(vContent.size());
			if(inflateInit2(&Stream, -MAX_WBITS) != Z_OK)
				return Error = "Cannot initialize ZIP decompression.", false;
			const int Result = inflate(&Stream, Z_FINISH);
			const bool Valid = Result == Z_STREAM_END && Stream.total_in == Entry.m_Packed && Stream.total_out == Entry.m_Unpacked;
			inflateEnd(&Stream);
			if(!Valid)
				return Error = "Corrupt/truncated ZIP deflate stream.", false;
		}
		if(crc32(0, vContent.data(), Entry.m_Unpacked) != Entry.m_Crc)
			return Error = "ZIP entry CRC mismatch.", false;
		fs::create_directories(Path.parent_path(), Ec);
		if(Ec)
			return Error = "Cannot create extracted file parent.", false;
		std::ofstream Out(Path, std::ios::binary);
		Out.write(reinterpret_cast<const char *>(vContent.data()), Entry.m_Unpacked);
		Out.flush();
		Out.close();
		if(!Out)
			return Error = "Extracted file write failed.", false;
#if !defined(_WIN32)
		fs::permissions(Path, static_cast<fs::perms>(Entry.m_Permissions ? Entry.m_Permissions : 0644), fs::perm_options::replace, Ec);
		if(Ec)
			return Error = "Cannot set extracted file permissions.", false;
#endif
	}
	Error.clear();
	return true;
}
}

#endif
