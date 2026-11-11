// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_AUTOEDGE_H
#define GAME_CLIENT_COMPONENTS_MALINKA_AUTOEDGE_H

#include <base/vmath.h>

#include <engine/console.h>
#include <generated/protocol.h>
#include <game/client/component.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <engine/client/enums.h>

class CMalinkaAutoEdge : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	bool HasSupportTile(int TileX, float FootY);
	bool HasHazardOrVoidBeyondPlatform(int TileX, int Direction, float FootY);
	void UpdateAutoEdgeState(int Dummy, CCharacter *pLocalCharacter);

	int m_FoundCount[NUM_DUMMIES];
	bool m_Locked[NUM_DUMMIES];
	vec2 m_LockedPos[NUM_DUMMIES];
	vec2 m_FoundPos[NUM_DUMMIES][16];
};

#endif