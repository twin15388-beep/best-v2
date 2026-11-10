// Checked, fail-closed package installation for the BestClient-Malinka channel.
// No game offsets, injected code, or official-client authorization are involved.
#ifndef TOOLS_MALINKA_PACKAGE_H
#define TOOLS_MALINKA_PACKAGE_H

#include <base/hash_ctxt.h>
#include <game/client/components/malinka/build_identity.h>
#include <game/client/components/malinka/file_safety.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Malinka::Package
{
namespace fs = std::filesystem;
constexpr uint64_t MAX_FILE_SIZE = 512ull * 1024 * 1024;
constexpr uint64_t MAX_TOTAL_SIZE = 2ull * 1024 * 1024 * 1024;
constexpr size_t MAX_FILES = 10000;
inline const char *Platform()
{
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
	return "windows-x86_64";
#elif defined(__linux__) && defined(__x86_64__)
	return "linux-x86_64";
#elif defined(__linux__) && defined(__aarch64__)
	return "linux-aarch64";
#else
	return "unsupported";
#endif
}

constexpr const char *MANIFEST = "malinka-update.manifest";

struct SFile
{
	std::string m_Path;
	std::string m_Sha256;
	uint64_t m_Size = 0;
};

struct SManifest
{
	std::string m_Version;
	std::string m_Platform;
	std::string m_Executable;
	std::string m_SourceSha256;
	std::vector<SFile> m_vFiles;
};

using Files::Utf8;
using Files::SafeRelativePath;
using Files::NoLinks;

inline bool IsHex(const std::string &Text, size_t Size)
{
	return Text.size() == Size && std::all_of(Text.begin(), Text.end(), [](char C) { return (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'); });
}


inline bool PreservedPath(const std::string &Path)
{
	std::string Relative = Path;
	std::transform(Relative.begin(), Relative.end(), Relative.begin(), [](unsigned char C) { return C >= 'A' && C <= 'Z' ? C - 'A' + 'a' : C; });
	static constexpr const char *apPreserved[] = {
		"storage.cfg", "autoexec.cfg", "settings_ddnet.cfg", "settings_malinka.cfg",
		"malinka", "replays", "demos", "screenshots", "downloadedmaps",
		"data/assets/arrow", "data/assets/arrows", "data/assets/audio", "data/audio"};
	for(const char *pPrefix : apPreserved)
	{
		const std::string Prefix = pPrefix;
		if(Relative == Prefix || Relative.starts_with(Prefix + "/"))
			return true;
	}
	return Relative.starts_with("settings_") && Relative.ends_with(".cfg");
}

inline bool HashFile(const fs::path &Path, std::string &Hex, uint64_t &Size)
{
	std::ifstream In(Path, std::ios::binary);
	if(!In)
		return false;
	SHA256_CTX Context;
	sha256_init(&Context);
	Size = 0;
	std::array<char, 65536> aBuffer;
	while(In)
	{
		In.read(aBuffer.data(), aBuffer.size());
		const auto Count = In.gcount();
		if(Count < 0 || Size + static_cast<uint64_t>(Count) > MAX_FILE_SIZE)
			return false;
		Size += Count;
		sha256_update(&Context, aBuffer.data(), static_cast<size_t>(Count));
	}
	if(!In.eof())
		return false;
	const auto Digest = sha256_finish(&Context);
	constexpr char HEX[] = "0123456789abcdef";
	Hex.clear();
	for(unsigned char Byte : Digest.data)
	{
		Hex.push_back(HEX[Byte >> 4]);
		Hex.push_back(HEX[Byte & 15]);
	}
	return true;
}

inline bool HasBuildMarker(const fs::path &Path, const std::string &Version)
{
	std::ifstream In(Path, std::ios::binary);
	if(!In)
		return false;
	const std::string Needle = Version.empty() ? Build::MARKER_PREFIX : std::string(Build::MARKER_PREFIX) + Version + ";upstream=";
	std::string Window;
	std::array<char, 65536> aBuffer;
	uint64_t Total = 0;
	while(In && Total <= MAX_FILE_SIZE)
	{
		In.read(aBuffer.data(), aBuffer.size());
		const auto Count = In.gcount();
		Window.append(aBuffer.data(), static_cast<size_t>(Count));
		if(Window.find(Needle) != std::string::npos)
			return true;
		if(Window.size() > Needle.size())
			Window.erase(0, Window.size() - Needle.size());
		Total += Count;
	}
	return false;
}

inline bool ReadManifest(const fs::path &Root, SManifest &Out, std::string &Error)
{
	std::error_code Ec;
	if(!NoLinks(Root / MANIFEST) || fs::file_size(Root / MANIFEST, Ec) > 4 * 1024 * 1024 || Ec)
		return Error = "Missing/invalid Malinka manifest; an official BestClient package cannot replace this build.", false;
	std::ifstream In(Root / MANIFEST, std::ios::binary);
	std::string Line;
	if(!std::getline(In, Line) || Line != "MALINKA_PACKAGE 1")
		return Error = "Unsupported package manifest.", false;
	SManifest Manifest;
	std::string Key, ID;
	size_t Count = 0;
	if(!(In >> Key >> ID) || Key != "id" || ID != Build::ID ||
		!(In >> Key >> Manifest.m_Version) || Key != "version" || Manifest.m_Version.size() > 32 ||
		!(In >> Key >> Manifest.m_Platform) || Key != "platform" ||
		!(In >> Key >> std::quoted(Manifest.m_Executable)) || Key != "executable" || !SafeRelativePath(Manifest.m_Executable) ||
		!(In >> Key >> Manifest.m_SourceSha256) || Key != "source_sha256" || !IsHex(Manifest.m_SourceSha256, 64) ||
		!(In >> Key >> Count) || Key != "files" || Count == 0 || Count > MAX_FILES)
		return Error = "Malformed Malinka package identity.", false;
	std::getline(In, Line);
	std::set<std::string> Paths;
	uint64_t Total = 0;
	bool HasExecutable = false;
	for(size_t Index = 0; Index < Count; ++Index)
	{
		if(!std::getline(In, Line) || Line.size() > 512)
			return Error = "Truncated package manifest.", false;
		SFile File;
		std::istringstream Row(Line);
		if(!(Row >> File.m_Sha256 >> File.m_Size >> std::quoted(File.m_Path)) ||
			!IsHex(File.m_Sha256, 64) || !SafeRelativePath(File.m_Path) || File.m_Path == MANIFEST ||
			File.m_Path.starts_with("update/") || File.m_Size > MAX_FILE_SIZE || Total + File.m_Size > MAX_TOTAL_SIZE)
			return Error = "Invalid package file entry or resource limit exceeded.", false;
		Row >> std::ws;
		if(!Row.eof())
			return Error = "Unexpected package entry fields.", false;
		std::string CanonicalName = File.m_Path;
		std::transform(CanonicalName.begin(), CanonicalName.end(), CanonicalName.begin(), [](unsigned char C) { return C >= 'A' && C <= 'Z' ? C - 'A' + 'a' : C; });
		if(CanonicalName == MANIFEST || CanonicalName == "update" || CanonicalName.starts_with("update/") || !Paths.insert(CanonicalName).second)
			return Error = "Duplicate/aliased package path.", false;
		Total += File.m_Size;
		HasExecutable = HasExecutable || File.m_Path == Manifest.m_Executable;
		Manifest.m_vFiles.push_back(std::move(File));
	}
	In >> std::ws;
	if(!In.eof() || !HasExecutable || PreservedPath(Manifest.m_Executable))
		return Error = "Package has trailing data or no client executable.", false;
	Out = std::move(Manifest);
	return true;
}

inline bool Verify(const fs::path &Root, const std::string &Platform, const std::string &Executable, SManifest &Out, std::string &Error)
{
	SManifest Manifest;
	if(!ReadManifest(Root, Manifest, Error))
		return false;
	if(Manifest.m_Platform != Platform || Manifest.m_Executable != Executable)
		return Error = "Update platform/executable does not match this installation.", false;
	for(const auto &File : Manifest.m_vFiles)
	{
		const auto Path = Root / fs::u8path(File.m_Path);
		std::string Hash;
		uint64_t Size = 0;
		std::error_code Ec;
		if(!NoLinks(Path) || !fs::is_regular_file(Path, Ec) || Ec || !HashFile(Path, Hash, Size) || Hash != File.m_Sha256 || Size != File.m_Size)
			return Error = "Package checksum/size/type verification failed: " + File.m_Path, false;
	}
	if(!HasBuildMarker(Root / fs::u8path(Executable), Manifest.m_Version))
		return Error = "Client binary does not carry the matching Malinka build identity.", false;
	Out = std::move(Manifest);
	return true;
}

// Move, never overwrite a live file in place. A failed rename leaves the source
// intact. All paths are under one install volume, including stage and rollback.
inline bool Move(const fs::path &From, const fs::path &To)
{
	std::error_code Ec;
	fs::rename(From, To, Ec);
	return !Ec;
}

inline bool Install(const fs::path &Root, const fs::path &InstallDir, const fs::path &Executable,
	const std::string &Platform, std::string &Error, const std::function<void(int)> &Progress = {})
{
	SManifest Manifest;
	if(!NoLinks(InstallDir) || !NoLinks(Executable) || Executable.parent_path().lexically_normal() != InstallDir.lexically_normal() ||
		!HasBuildMarker(Executable, "") || !Verify(Root, Platform, Utf8(Executable.filename()), Manifest, Error))
		return Error = Error.empty() ? "Unsafe install path." : Error, false;
	const auto Token = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const auto Stage = InstallDir / "update" / ("malinka-stage-" + Token);
	const auto Backup = InstallDir / "update" / ("malinka-backup-" + Token);
	if(!NoLinks(Stage) || !NoLinks(Backup))
		return Error = "Unsafe staging/backup path.", false;
	std::error_code Ec;
	fs::create_directories(Stage, Ec);
	if(Ec)
		return Error = "Cannot create update staging directory.", false;
	fs::create_directories(Backup, Ec);
	if(Ec)
		return Error = "Cannot create update backup directory.", false;
	struct SInstallFile
	{
		fs::path m_Relative;
		bool m_HadOld = false;
		bool m_OldMoved = false;
		bool m_NewMoved = false;
	};
	std::vector<SInstallFile> vInstall;
	for(const auto &File : Manifest.m_vFiles)
	{
		if(PreservedPath(File.m_Path))
			continue;
		const auto Relative = fs::u8path(File.m_Path);
		const auto Destination = InstallDir / Relative;
		if(!NoLinks(Destination))
			return Error = "Install destination contains a symlink/reparse point: " + File.m_Path, false;
		const bool Exists = fs::exists(Destination, Ec);
		if(Ec || (Exists && !fs::is_regular_file(Destination, Ec)) || Ec)
			return Error = "Cannot replace non-regular or unreadable install destination.", false;
		fs::create_directories((Stage / Relative).parent_path(), Ec);
		if(Ec)
			return Error = "Cannot create staged file directory.", false;
		fs::copy_file(Root / Relative, Stage / Relative, fs::copy_options::none, Ec);
		if(Ec)
			return Error = "Cannot stage update file; existing client was not changed.", false;
		std::string Hash;
		uint64_t Size = 0;
		if(!HashFile(Stage / Relative, Hash, Size) || Hash != File.m_Sha256 || Size != File.m_Size)
			return Error = "Staged file verification failed; existing client was not changed.", false;
		vInstall.push_back({Relative, Exists});
	}
	// Create ALL rollback directories and a readable journal before the first mutation.
	for(const auto &File : vInstall)
	{
		fs::create_directories((Backup / File.m_Relative).parent_path(), Ec);
		if(Ec)
			return Error = "Cannot prepare rollback directory; update cancelled.", false;
		fs::create_directories((InstallDir / File.m_Relative).parent_path(), Ec);
		if(Ec)
			return Error = "Cannot prepare install directory; update cancelled.", false;
	}
	std::ofstream Journal(Backup / "rollback.txt", std::ios::binary);
	for(const auto &File : vInstall)
		Journal << (File.m_HadOld ? "restore " : "remove ") << std::quoted(Utf8(File.m_Relative)) << '\n';
	Journal.flush();
	Journal.close();
	if(!Journal)
		return Error = "Cannot write rollback journal; update cancelled.", false;
	bool Success = true;
	size_t Done = 0;
	for(auto &File : vInstall)
	{
		const auto Destination = InstallDir / File.m_Relative;
		if(!NoLinks(Destination))
		{
			Success = false;
			break;
		}
		if(File.m_HadOld)
		{
			File.m_OldMoved = Move(Destination, Backup / File.m_Relative);
			if(!File.m_OldMoved)
			{
				Success = false;
				break;
			}
		}
		File.m_NewMoved = Move(Stage / File.m_Relative, Destination);
		if(!File.m_NewMoved)
		{
			Success = false;
			break;
		}
		if(Progress)
			Progress(static_cast<int>(++Done * 100 / vInstall.size()));
	}
	if(!Success)
	{
		bool Restored = true;
		for(auto It = vInstall.rbegin(); It != vInstall.rend(); ++It)
		{
			const auto Destination = InstallDir / It->m_Relative;
			if(It->m_NewMoved)
			{
				fs::remove(Destination, Ec);
				if(Ec)
					Restored = false;
			}
			if(It->m_OldMoved && !Move(Backup / It->m_Relative, Destination))
				Restored = false;
		}
		Error = Restored ? "Installation failed; previous files were restored. Backup: " : "Installation and rollback failed. Do not restart; restore from backup: ";
		Error += Utf8(Backup);
		return false;
	}
	fs::remove_all(Stage, Ec);
	// Keep the previous executables/resources and journal, including a renamed
	// running updater on Windows. User configurations/replays were never replaced.
	Error.clear();
	return true;
}
}

#endif
