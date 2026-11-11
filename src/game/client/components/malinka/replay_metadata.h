#ifndef GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_METADATA_H
#define GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_METADATA_H

#include <string>

namespace Malinka::Replay
{
constexpr const char *PHYSICS = "Best-cf192e3-DDNet-20.1.1";

struct SMetadata
{
	std::string m_Map;
	std::string m_MapSha256;
	std::string m_Physics = PHYSICS;
	bool m_Legacy = false;
};
}

#endif
