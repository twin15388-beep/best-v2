// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_AUTOALED_H
#define GAME_CLIENT_COMPONENTS_MALINKA_AUTOALED_H

#include <base/vmath.h>
#include <engine/console.h>
#include <generated/protocol.h>
#include <game/client/component.h>
#include <engine/client/enums.h>
#include <game/client/prediction/entities/character.h>

class CMalinkaAutoAled : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	bool HasSingleFreezeGapBetween(vec2 From, vec2 To);
	bool FindAutoAledTarget(CCharacter *pLocalCharacter, vec2 RealAimPos);

	int64_t m_LastAutoAled[NUM_DUMMIES];
	bool m_AutoAledLatched[NUM_DUMMIES];
};

#endif