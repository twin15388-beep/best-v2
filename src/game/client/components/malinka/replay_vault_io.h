// Community replay cache, not a game-state or official-client authorization.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_VAULT_IO_H
#define GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_VAULT_IO_H

#include "file_safety.h"
#include "replay_paths.h"

#include <tools/malinka_package_zip.h>

#include <functional>
#include <set>
#include <string_view>

namespace Malinka::ReplayVault
{
namespace fs = std::filesystem;
// Public source documented by KRX. No tokens, arbitrary URL setting or
// executable download. The archive URL is constructed only from a checked SHA.
constexpr const char *HEAD_URL = "https://api.github.com/repos/krxclient/krx-replays/commits/main";
constexpr const char *ARCHIVE_URL = "https://codeload.github.com/krxclient/krx-replays/zip/";
constexpr uint64_t MAX_ARCHIVE_BYTES = 32ull * 1024 * 1024;
constexpr uint64_t MAX_MEMBER_BYTES = 4ull * 1024 * 1024;
constexpr uint64_t MAX_EXPANDED_BYTES = 64ull * 1024 * 1024;
constexpr size_t MAX_ENTRIES = 4096;
constexpr size_t MAX_REPLAYS = 1024;

inline bool ValidRevision(std::string_view Revision)
{
	return Revision.size() == 40 && std::all_of(Revision.begin(), Revision.end(), [](char C) {
		return (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f');
	});
}

// GitHub's application/vnd.github.sha media type, measured on the public
// commits/main endpoint. An error JSON or a changed schema is never a URL.
inline bool ReadRevision(std::string_view Response, std::string &Out)
{
	if(Response.ends_with("\r\n"))
		Response.remove_suffix(2);
	else if(Response.ends_with('\n'))
		Response.remove_suffix(1);
	if(!ValidRevision(Response))
		return false;
	Out.assign(Response);
	return true;
}

struct SCacheResult
{
	int m_Files = 0;
	bool m_AlreadyPresent = false;
};

class CWorkDirectory
{
	fs::path m_Path;

public:
	CWorkDirectory() = default;
	void Take(fs::path &&Path) noexcept { m_Path = std::move(Path); }
	const fs::path &Path() const { return m_Path; }
	CWorkDirectory(const CWorkDirectory &) = delete;
	CWorkDirectory &operator=(const CWorkDirectory &) = delete;
	~CWorkDirectory() noexcept
	{
		// A bounded retry handles a transient allocation failure during optional
		// cleanup, including one after publication. Persistent failures leave only
		// our staging tree, never delete an old snapshot or follow a link.
		for(int Attempt = 0; Attempt < 2; ++Attempt)
		{
			try
			{
				if(m_Path.empty() || (Files::NoLinks(m_Path) && Files::RemoveTreeBounded(m_Path, 8192, 128)))
					return;
			}
			catch(...)
			{
				// Never throw while unwinding a failed parse/cache operation.
			}
		}
	}
};

inline bool CacheArchive(const std::string &ArchiveData, std::string_view Revision,
	const fs::path &ReplayRoot, const fs::path &CacheRoot, SCacheResult &ResultOut,
	std::string &Error, const std::function<bool()> &Cancelled = {})
{
	const auto IsCancelled = [&] { return Cancelled && Cancelled(); };
	if(!ValidRevision(Revision) || ArchiveData.size() < 22 || ArchiveData.size() > MAX_ARCHIVE_BYTES ||
		!Files::NoLinks(ReplayRoot) || !Files::NoLinks(CacheRoot))
		return Error = "Invalid vault revision, archive size, or linked cache root.", false;
	if(IsCancelled())
		return Error = "Vault sync cancelled.", false;
	std::error_code Ec;
	fs::create_directories(CacheRoot, Ec);
	if(Ec || !Files::NoLinks(CacheRoot))
		return Error = "Cannot create an unlinked vault work root.", false;
	// Do not reuse stale work or remove a pre-existing directory. Up to 16
	// concurrent/stale names can be tolerated; exhaustion fails visibly.
	CWorkDirectory Cleanup;
	for(int Attempt = 0; Attempt < 16; ++Attempt)
	{
		auto Candidate = CacheRoot / ("staging-" + std::string(Revision) + "-" + std::to_string(Attempt));
		if(fs::create_directory(Candidate, Ec))
		{
			Cleanup.Take(std::move(Candidate));
			break;
		}
		if(Ec && Ec != std::errc::file_exists)
			return Error = "Cannot reserve a vault work directory.", false;
		Ec.clear();
	}
	const auto &Work = Cleanup.Path();
	if(Work.empty())
		return Error = "Vault work names are occupied. Inspect vault-cache/staging-* before retrying.", false;
	const auto Archive = Work / "vault.zip";
	const auto Extract = Work / "extract";
	if(!Files::WriteExclusive(Archive, ArchiveData))
		return Error = "Cannot write the private vault archive.", false;
	if(IsCancelled())
		return Error = "Vault sync cancelled.", false;
	if(!Package::ExtractZip(Archive, Extract, Error, {MAX_ARCHIVE_BYTES, MAX_MEMBER_BYTES, MAX_EXPANDED_BYTES, MAX_ENTRIES}))
		return false;
	const auto Prefix = "krx-replays-" + std::string(Revision);
	const auto Source = Extract / Prefix / "replays";
	// Require the pinned repository prefix, not an archive-selected root.
	std::vector<Files::SDirectoryEntry> vRoots;
	if(!Files::ListDirectory(Extract, 2, vRoots) || vRoots.size() != 1 ||
		vRoots.front().m_Kind != Files::EDirectoryEntryKind::Directory || vRoots.front().m_Path.filename() != Prefix ||
		!fs::is_directory(Source, Ec) || Ec || !Files::NoLinks(Source))
		return Error = "Unexpected vault archive root or missing replay directory.", false;
	const auto ReadNames = [&](const fs::path &Root, std::set<std::string> &Names) {
		struct SNode { fs::path m_Path; int m_Depth; };
		std::vector<SNode> vPending = {{Root, 0}};
		size_t Visited = 0;
		while(!vPending.empty())
		{
			auto Node = std::move(vPending.back());
			vPending.pop_back();
			if(IsCancelled() || Node.m_Depth > 6)
				return false;
			std::vector<Files::SDirectoryEntry> vEntries;
			if(!Files::ListDirectory(Node.m_Path, MAX_ENTRIES - Visited, vEntries))
				return false;
			Visited += vEntries.size();
			for(auto &Entry : vEntries)
			{
				if(IsCancelled())
					return false;
				if(Entry.m_Kind == Files::EDirectoryEntryKind::Directory)
				{
					vPending.push_back({std::move(Entry.m_Path), Node.m_Depth + 1});
					continue;
				}
				const auto Relative = Replay::Utf8(Entry.m_Path.lexically_relative(Root));
				if(Entry.m_Kind != Files::EDirectoryEntryKind::RegularFile || Entry.m_Path.extension() != ".tas" ||
					!Files::SafeRelativePath("vault/" + std::string(Revision) + "/" + Relative) ||
					!Names.insert(Relative).second || Names.size() > MAX_REPLAYS)
					return false;
			}
		}
		return true;
	};
	std::set<std::string> Names;
	if(!ReadNames(Source, Names))
		return Error = "Vault replay tree is unsafe/unsupported, exceeds limits or was cancelled.", false;
	if(Names.empty())
		return Error = "Vault archive contains no replay files.", false;
	// Raw .tas files are deliberately NOT marked trusted, converted, loaded or
	// played here. The sample-schema loader and full physics validation still
	// apply when a user explicitly selects a file, including unknown formats.
	const auto VaultRoot = ReplayRoot / "vault";
	const auto Destination = VaultRoot / std::string(Revision);
	if(!Files::NoLinks(Destination) || !Replay::WithinRoot(ReplayRoot, Destination))
		return Error = "Vault destination escapes its root or is linked.", false;
	SCacheResult Result;
	Result.m_Files = static_cast<int>(Names.size());
	if(fs::exists(Destination, Ec))
	{
		if(Ec || !fs::is_directory(Destination))
			return Error = "Existing vault revision is not a directory; it was not changed.", false;
		std::set<std::string> CachedNames;
		if(!ReadNames(Destination, CachedNames) || CachedNames != Names)
			return Error = "Existing vault snapshot differs/is incomplete, or sync was cancelled. Nothing was overwritten.", false;
		for(const auto &Relative : Names)
		{
			if(IsCancelled())
				return Error = "Vault sync cancelled.", false;
			const auto CachedPath = Destination / fs::u8path(Relative);
			const auto SourcePath = Source / fs::u8path(Relative);
			const auto Size = fs::file_size(CachedPath);
			if(Size > MAX_MEMBER_BYTES || Size != fs::file_size(SourcePath))
				return Error = "Existing vault replay size differs; it was not overwritten.", false;
			std::string SourceHash, CacheHash;
			uint64_t SourceSize = 0, CacheSize = 0;
			if(!Package::HashFile(SourcePath, SourceHash, SourceSize) || !Package::HashFile(CachedPath, CacheHash, CacheSize) ||
				SourceSize != CacheSize || SourceHash != CacheHash)
				return Error = "Existing vault replay differs; it was not overwritten.", false;
		}
		Result.m_AlreadyPresent = true;
	}
	else
	{
		if(Ec)
			return Error = "Cannot inspect vault destination.", false;
		fs::create_directories(VaultRoot, Ec);
		if(Ec || IsCancelled() || !Files::NoLinks(Source) || !Files::NoLinks(Destination) || !Files::MoveNoReplace(Source, Destination))
			return Error = "Vault publication cancelled/failed, or destination appeared. No old snapshot was replaced.", false;
	}
	ResultOut = Result;
	Error.clear();
	return true;
}
}

#endif
