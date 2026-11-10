#ifndef GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_PATHS_H
#define GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_PATHS_H

#include <engine/storage.h>
#include "file_safety.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace Malinka::Replay
{
inline std::filesystem::path Root(IStorage *pStorage)
{
	char aPath[IO_MAX_PATH_LENGTH];
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, "malinka/replays", aPath, sizeof(aPath));
	return std::filesystem::u8path(aPath);
}

inline std::string Utf8(const std::filesystem::path &Path)
{
	const auto Text = Path.generic_u8string();
	return std::string(Text.begin(), Text.end());
}

inline bool SafeRelativeName(const std::string &Name)
{
	return Files::SafeRelativePath(Name);
}

inline bool WithinRoot(const std::filesystem::path &RootPath, const std::filesystem::path &Path)
{
	std::error_code Ec;
	const auto CanonicalRoot = std::filesystem::weakly_canonical(RootPath, Ec);
	if(Ec)
		return false;
	const auto Canonical = std::filesystem::weakly_canonical(Path, Ec);
	if(Ec)
		return false;
	auto Component = Canonical.begin();
	for(const auto &RootComponent : CanonicalRoot)
	{
		if(Component == Canonical.end() || *Component != RootComponent)
			return false;
		++Component;
	}
	return Component != Canonical.end();
}

inline std::vector<std::string> List(IStorage *pStorage)
{
	const auto RootPath = Root(pStorage);
	std::error_code Ec;
	std::filesystem::recursive_directory_iterator It(RootPath, std::filesystem::directory_options::skip_permission_denied, Ec), End;
	std::vector<std::string> vNames;
	size_t Visited = 0;
	while(!Ec && It != End && ++Visited <= 20000 && vNames.size() < 4096)
	{
		if(It.depth() >= 8)
			It.disable_recursion_pending();
		const auto Path = It->path();
		if(It->is_regular_file(Ec) && !Ec && Path.extension() == ".tas" && WithinRoot(RootPath, Path))
		{
			const auto Relative = Path.lexically_relative(RootPath);
			const std::string Name = Utf8(Relative);
			if(SafeRelativeName(Name))
				vNames.push_back(Name);
		}
		It.increment(Ec);
	}
	std::sort(vNames.begin(), vNames.end());
	return vNames;
}
}

#endif
