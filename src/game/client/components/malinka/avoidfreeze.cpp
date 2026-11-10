// Derived movement probes from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f.
// Avoid policy rewritten for BestClient; see docs/MALINKA_SOURCES.md and license.txt.
#include "avoidfreeze.h"
#include "input_helpers.h"
#include "simulation_step.h"

#include <base/time.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

void CMalinkaAvoidFreeze::OnReset()
{
	std::fill(std::begin(m_BlatantFreezeResetFrozenLast), std::end(m_BlatantFreezeResetFrozenLast), false);
	std::fill(std::begin(m_BlatantFreezeResetSince), std::end(m_BlatantFreezeResetSince), 0);
	std::fill(std::begin(m_AvoidLastMousePos), std::end(m_AvoidLastMousePos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aLastSafeValid), std::end(m_aLastSafeValid), false);
	std::fill(std::begin(m_aLastSafeClientId), std::end(m_aLastSafeClientId), -1);
	std::fill(std::begin(m_aLastMode), std::end(m_aLastMode), -1);
}

bool CMalinkaAvoidFreeze::IsFreezeTile(int Tile) const
{
	return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE;
}

bool CMalinkaAvoidFreeze::HasFreezeTile(vec2 Pos) const
{
	const int MapIndex = GameClient()->Collision()->GetPureMapIndex(Pos);
	if(MapIndex < 0)
		return false;

	return IsFreezeTile(GameClient()->Collision()->GetTileIndex(MapIndex)) || IsFreezeTile(GameClient()->Collision()->GetFrontTileIndex(MapIndex));
}

namespace
{
struct SAvoidTiles
{
	bool m_Death;
	bool m_Teles;
	bool m_Unfreeze;
	int m_UnfreezeTicks;
};

bool IsAvoidTile(int Tile, bool Unfreeze)
{
	return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE ||
		(Unfreeze && (Tile == TILE_UNFREEZE || Tile == TILE_DUNFREEZE || Tile == TILE_LUNFREEZE));
}

bool AvoidDangerAt(CCollision &Collision, vec2 Pos, const SAvoidTiles &Tiles, int Tick)
{
	const int Index = Collision.GetPureMapIndex(Pos);
	if(Index < 0)
		return false;
	const bool Unfreeze = Tiles.m_Unfreeze && Tick <= Tiles.m_UnfreezeTicks;
	if(IsAvoidTile(Collision.GetTileIndex(Index), Unfreeze) || IsAvoidTile(Collision.GetFrontTileIndex(Index), Unfreeze))
		return true;
	// Only player teleport entrances are hazards here: a hook/weapon teleport
	// or a teleport checkpoint alone does not teleport the character.
	if(Tiles.m_Teles && (Collision.IsTeleport(Index) || Collision.IsEvilTeleport(Index) ||
		Collision.IsCheckTeleport(Index) || Collision.IsCheckEvilTeleport(Index)))
		return true;
	if(Tiles.m_Death)
	{
		const float Offset = CCharacterCore::PhysicalSize() / 3.0f;
		for(float X : {-Offset, Offset})
			for(float Y : {-Offset, Offset})
			{
				const int Corner = Collision.GetPureMapIndex(Pos + vec2(X, Y));
				if(Corner >= 0 && (Collision.GetTileIndex(Corner) == TILE_DEATH || Collision.GetFrontTileIndex(Corner) == TILE_DEATH))
					return true;
			}
	}
	return false;
}
}

void CMalinkaAvoidFreeze::ApplyAvoid(CNetObj_PlayerInput &Input, int64_t LastActivity) try
{
	const int Slot = g_Config.m_ClDummy;
	if(Slot < 0 || Slot >= NUM_DUMMIES || !g_Config.m_MalinkaAvoidfreeze)
		return;
	const int Mode = g_Config.m_MalinkaAvoidMode;
	if(Mode != 1 && Mode != 2)
		return;
	if(!GameClient()->Predict() || GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive() ||
		GameClient()->m_GameConsole.IsActive() || GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;
	const bool Legit = Mode == 1;
	const bool AfkProtection = Legit ? g_Config.m_MalinkaAvoidLegitAfkProtection : g_Config.m_MalinkaAvoidBlatantAfkProtection;
	const int AfkTime = Legit ? g_Config.m_MalinkaAvoidLegitAfkTime : g_Config.m_MalinkaAvoidBlatantAfkTime;
	if(AfkProtection && Malinka::AvoidAfkExpired(time_get(), LastActivity, time_freq(), AfkTime))
	{
		g_Config.m_MalinkaAvoidfreeze = 0;
		m_aLastSafeValid[Slot] = false;
		return;
	}
	const int ClientId = GameClient()->m_aLocalIds[Slot];
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(m_aLastSafeClientId[Slot] != ClientId || m_aLastMode[Slot] != Mode)
		m_aLastSafeValid[Slot] = false;
	m_aLastSafeClientId[Slot] = ClientId;
	m_aLastMode[Slot] = Mode;
	const auto *pLiveCharacter = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
	if(!pLiveCharacter || pLiveCharacter->m_FreezeTime > 0 || pLiveCharacter->Core()->m_DeepFrozen || pLiveCharacter->Core()->m_LiveFrozen)
		return;

	const int CheckTicks = std::clamp(Legit ? g_Config.m_MalinkaAvoidLegitCheckTicks : g_Config.m_MalinkaAvoidBlatantCheckTicks, 1, Malinka::AVOID_MAX_TICKS);
	const bool AllowDirection = Legit ? g_Config.m_MalinkaAvoidLegitDirection : g_Config.m_MalinkaAvoidBlatantDirection && g_Config.m_MalinkaAvoidBlatantCanSetDir;
	const bool AllowHook = (Legit ? g_Config.m_MalinkaAvoidLegitHook : g_Config.m_MalinkaAvoidBlatantHook) &&
		(Legit || !g_Config.m_MalinkaAvoidBlatantOnlyHorizontally);
	const SAvoidTiles Tiles{
		(Legit ? g_Config.m_MalinkaAvoidLegitDeath : g_Config.m_MalinkaAvoidBlatantDeath) != 0,
		(Legit ? g_Config.m_MalinkaAvoidLegitTeles : g_Config.m_MalinkaAvoidBlatantTeles) != 0,
		(Legit ? g_Config.m_MalinkaAvoidLegitUnfreeze : g_Config.m_MalinkaAvoidBlatantUnfreeze) != 0,
		std::clamp(Legit ? g_Config.m_MalinkaAvoidLegitUnfreezeTicks : g_Config.m_MalinkaAvoidBlatantUnfreezeTicks, 1, 30)};
	const CNetObj_PlayerInput BaseInput = Input;
	const auto Actions = Malinka::AvoidActions(Input.m_Direction, Input.m_Hook, AllowDirection, AllowHook);
	CGameWorld Captured;
	if(!Captured.CopyWorldIndependent(&GameClient()->m_PredictedWorld))
		return;
	Captured.m_WorldConfig.m_PredictEvents = false;

	auto Evaluate = [&](const CNetObj_PlayerInput &Candidate, const Malinka::CAvoidSequence *pSequence, int Ticks) -> Malinka::SAvoidPrediction {
		CGameWorld World;
		if(!World.CopyWorldIndependent(&Captured))
			return {};
		for(int Step = 0; Step < Ticks; ++Step)
		{
			CNetObj_PlayerInput TickInput = Candidate;
			if(pSequence)
			{
				const auto &Action = Actions.m_aActions[(*pSequence)[Step]];
				TickInput.m_Direction = Action.m_Direction;
				TickInput.m_Hook = Action.m_Hook;
			}
			auto *pCharacter = World.GetCharacterById(ClientId);
			if(!pCharacter)
				return {}; // No valid starting character is not a safe path.
			const vec2 Before = pCharacter->GetPos();
			if(!std::isfinite(Before.x) || !std::isfinite(Before.y))
				return {};
			if(AvoidDangerAt(*World.Collision(), Before, Tiles, Step + 1))
				return {Malinka::EAvoidPrediction::DANGER, Step + 1};
			if(!Malinka::TickSimulation(World, {{ClientId, TickInput}}))
				return {};
			pCharacter = World.GetCharacterById(ClientId);
			if(!pCharacter)
				return {Malinka::EAvoidPrediction::DANGER, Step + 1};
			const vec2 After = pCharacter->GetPos();
			if(!std::isfinite(After.x) || !std::isfinite(After.y))
				return {};
			if(pCharacter->m_FreezeTime > 0 || pCharacter->Core()->m_DeepFrozen || pCharacter->Core()->m_LiveFrozen ||
				AvoidDangerAt(*World.Collision(), After, Tiles, Step + 1) ||
				(Tiles.m_Unfreeze && Step < Tiles.m_UnfreezeTicks &&
					(IsAvoidTile(pCharacter->m_TileIndex, true) || IsAvoidTile(pCharacter->m_TileFIndex, true))))
				return {Malinka::EAvoidPrediction::DANGER, Step + 1};
		}
		return {Malinka::EAvoidPrediction::SAFE, 0};
	};

	const auto Baseline = Evaluate(BaseInput, nullptr, CheckTicks);
	if(Baseline.m_State == Malinka::EAvoidPrediction::UNAVAILABLE)
		return;
	if(Baseline.m_State == Malinka::EAvoidPrediction::SAFE)
	{
		m_aLastSafeInput[Slot] = BaseInput;
		m_aLastSafeValid[Slot] = true;
		return;
	}
	if(!Legit && !Malinka::AvoidShouldIntervene(Baseline, g_Config.m_MalinkaAvoidBlatantKickInTicks))
		return;

	if(Legit)
	{
		const uint32_t Seed = static_cast<uint32_t>(Captured.GameTick()) * 0x9e3779b9u + static_cast<uint32_t>(ClientId);
		const auto Result = Malinka::SearchLegitAvoid(Actions, CheckTicks, g_Config.m_MalinkaAvoidLegitQuality,
			g_Config.m_MalinkaAvoidLegitRandomness, g_Config.m_MalinkaAvoidLegitDirectionPriority,
			g_Config.m_MalinkaAvoidLegitHookPriority, g_Config.m_MalinkaAvoidLegitLifePriority, Seed, Baseline,
			[&](const Malinka::CAvoidSequence &Sequence, int Ticks) { return Evaluate(BaseInput, &Sequence, Ticks); });
		if(Result.m_Available && Result.m_Found)
		{
			Input.m_Direction = Actions.m_aActions[Result.m_FirstAction].m_Direction;
			Input.m_Hook = Actions.m_aActions[Result.m_FirstAction].m_Hook;
		}
		return;
	}

	// Blatant: evaluate constant candidates for the complete Check Ticks horizon.
	// Unlike the donor, Hook/Direction/FOV are constraints, not scoring bonuses.
	const bool ScanAim = AllowHook && g_Config.m_MalinkaAvoidBlatantAimbot;
	const int Segments = ScanAim ? std::clamp(g_Config.m_MalinkaAvoidBlatantAimbotSegments, 1, 180) : 0;
	const vec2 BaseAim = normalize(Malinka::FiniteAim(vec2(BaseInput.m_TargetX, BaseInput.m_TargetY)));
	const float BaseAngle = std::atan2(BaseAim.y, BaseAim.x);
	CNetObj_PlayerInput BestInput = BaseInput;
	bool FoundSafe = false;
	float BestAngle = std::numeric_limits<float>::infinity();
	int BestChanges = std::numeric_limits<int>::max();
	auto Consider = [&](const CNetObj_PlayerInput &Candidate, float AimOffset) {
		const auto Prediction = Evaluate(Candidate, nullptr, CheckTicks);
		if(Prediction.m_State == Malinka::EAvoidPrediction::UNAVAILABLE)
			return false;
		if(Prediction.m_State != Malinka::EAvoidPrediction::SAFE)
			return true;
		const int Changes = (Candidate.m_Direction != BaseInput.m_Direction) + (Candidate.m_Hook != BaseInput.m_Hook);
		if(!FoundSafe || (g_Config.m_MalinkaAvoidBlatantAimbotMode == 1 ?
			(AimOffset < BestAngle || (AimOffset == BestAngle && Changes < BestChanges)) :
			(Changes < BestChanges || (Changes == BestChanges && AimOffset < BestAngle))))
		{
			BestInput = Candidate;
			BestChanges = Changes;
			BestAngle = AimOffset;
			FoundSafe = true;
		}
		return true;
	};
	for(int ActionIndex = 0; ActionIndex < Actions.m_Count; ++ActionIndex)
	{
		CNetObj_PlayerInput Candidate = BaseInput;
		Candidate.m_Direction = Actions.m_aActions[ActionIndex].m_Direction;
		Candidate.m_Hook = Actions.m_aActions[ActionIndex].m_Hook;
		if(!Consider(Candidate, 0.0f))
			return;
		// Aim is relevant only to a hooking candidate. Preserve exact user aim
		// on releases; changing it there could fire a weapon in another direction.
		if(!Candidate.m_Hook)
			continue;
		for(int Segment = 0; Segment < Segments; ++Segment)
		{
			const float Offset = Malinka::AvoidAimOffset(Segment, Segments, g_Config.m_MalinkaAvoidBlatantAimbotFov);
			const float Angle = BaseAngle + Offset * pi / 180.0f;
			Candidate.m_TargetX = static_cast<int>(std::cos(Angle) * 1000.0f);
			Candidate.m_TargetY = static_cast<int>(std::sin(Angle) * 1000.0f);
			if(!Consider(Candidate, std::abs(Offset)))
				return;
		}
	}
	if(FoundSafe)
	{
		Input = BestInput;
		m_aLastSafeInput[Slot] = BestInput;
		m_aLastSafeValid[Slot] = true;
	}
	else if(g_Config.m_MalinkaAvoidBlatantNsif && m_aLastSafeValid[Slot])
	{
		// Revalidate the cached first input against the *current* world, identity
		// and assistance limits. Never blindly replay a stale hook/aim/direction.
		CNetObj_PlayerInput Cached = BaseInput;
		if(AllowDirection)
			Cached.m_Direction = m_aLastSafeInput[Slot].m_Direction;
		if(AllowHook)
		{
			Cached.m_Hook = m_aLastSafeInput[Slot].m_Hook;
			if(ScanAim)
			{
				const vec2 Aim = normalize(Malinka::FiniteAim(vec2(m_aLastSafeInput[Slot].m_TargetX, m_aLastSafeInput[Slot].m_TargetY)));
				if(dot(BaseAim, Aim) >= std::cos(g_Config.m_MalinkaAvoidBlatantAimbotFov * pi / 360.0f))
				{
					Cached.m_TargetX = m_aLastSafeInput[Slot].m_TargetX;
					Cached.m_TargetY = m_aLastSafeInput[Slot].m_TargetY;
				}
			}
		}
		const auto Prediction = Evaluate(Cached, nullptr, CheckTicks);
		if(Prediction.m_State == Malinka::EAvoidPrediction::SAFE ||
			(Prediction.m_State == Malinka::EAvoidPrediction::DANGER && Prediction.m_DangerTick > Baseline.m_DangerTick))
			Input = Cached;
	}
}
catch(const std::exception &)
{
	// No network input is modified until an entire selection succeeds.
}

bool CMalinkaAvoidFreeze::HasFreezeAbove(vec2 Pos, int Tiles)
{
	for(int Tile = 1; Tile <= Tiles; ++Tile)
	{
		if(HasFreezeTile(Pos + vec2(0.0f, -Tile * 32.0f)))
			return true;
	}
	return false;
}

int CMalinkaAvoidFreeze::DetectFreezeSide(vec2 Pos)
{
	const float SideOffset = CCharacterCore::PhysicalSizeVec2().x * 0.52f + 1.5f;
	constexpr float aSamplesY[] = {-8.0f, 0.0f, 8.0f};
	bool FreezeRight = false;
	bool FreezeLeft = false;
	for(const float SampleY : aSamplesY)
	{
		FreezeRight |= HasFreezeTile(Pos + vec2(SideOffset, SampleY));
		FreezeLeft |= HasFreezeTile(Pos + vec2(-SideOffset, SampleY));
	}

	if(FreezeRight == FreezeLeft)
		return 0;
	return FreezeRight ? 1 : -1;
}

bool CMalinkaAvoidFreeze::ShouldAutoJumpSave(CCharacter *pCharacter)
{
	if(pCharacter == nullptr || pCharacter->IsGrounded() || pCharacter->Core()->m_Vel.y <= 4.4f)
		return false;
	if(pCharacter->GetJumped() > 1)
		return false;

	const vec2 Pos = pCharacter->GetPos();
	if(HasFreezeAbove(Pos, 4))
		return false;

	const float FootOffset = CCharacterCore::PhysicalSizeVec2().y * 0.5f + 2.0f;
	for(int Step = 0; Step < 2; ++Step)
	{
		const float Lookahead = FootOffset + (Step + 1) * 4.0f + pCharacter->Core()->m_Vel.y * (0.18f + 0.06f * Step);
		if(HasFreezeTile(Pos + vec2(0.0f, Lookahead)))
			return true;
	}

	return false;
}

bool CMalinkaAvoidFreeze::HasPlayerNearAim(const CCharacter *pLocalCharacter, vec2 AimDir, float MaxDistance, float MaxAngleRadians)
{
	if(pLocalCharacter == nullptr || length(AimDir) < 0.001f)
		return false;

	const vec2 Origin = pLocalCharacter->GetPos();
	const vec2 NormalizedAim = normalize(AimDir);
	const float CosThreshold = std::cos(MaxAngleRadians);
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(ClientId == GameClient()->m_Snap.m_LocalClientId || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;

		const CCharacter *pTarget = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
		const vec2 TargetPos = pTarget != nullptr ? pTarget->GetPos() : GameClient()->m_aClients[ClientId].m_RenderPos;
		const vec2 ToTarget = TargetPos - Origin;
		const float Dist = length(ToTarget);
		if(Dist < 1.0f || Dist > MaxDistance)
			continue;
		if(dot(normalize(ToTarget), NormalizedAim) >= CosThreshold)
			return true;
	}
	return false;
}

bool CMalinkaAvoidFreeze::HasImmediateSideFreeze(const CCharacter *pCharacter, int Direction)
{
	if(pCharacter == nullptr || Direction == 0)
		return false;

	const vec2 Pos = pCharacter->GetPos();
	const float SideOffset = CCharacterCore::PhysicalSizeVec2().x * 0.5f + 1.5f;
	constexpr float aSampleY[] = {-8.0f, 0.0f, 8.0f};
	for(int Step = 0; Step < 3; ++Step)
	{
		const float SampleX = Pos.x + Direction * (SideOffset + Step * 4.0f);
		for(const float OffsetY : aSampleY)
		{
			if(HasFreezeTile(vec2(SampleX, Pos.y + OffsetY)))
				return true;
		}
	}
	return false;
}

bool CMalinkaAvoidFreeze::HasImpendingFreeze(const CCharacter *pCharacter, int Ticks)
{
	if(pCharacter == nullptr)
		return false;

	const vec2 Pos = pCharacter->GetPos();
	const vec2 Vel = pCharacter->Core()->m_Vel;
	const float FootOffset = CCharacterCore::PhysicalSizeVec2().y * 0.5f + 4.0f;
	for(int Tick = 1; Tick <= std::max(1, Ticks); ++Tick)
	{
		const float Scale = Tick * 0.95f;
		const vec2 FuturePos = Pos + Vel * Scale;
		if(HasFreezeTile(FuturePos + vec2(0.0f, FootOffset)) ||
			HasFreezeTile(FuturePos + vec2(0.0f, 0.0f)))
			return true;
	}
	return false;
}

bool CMalinkaAvoidFreeze::IsBlatantFrozenAtPos(vec2 Pos)
{
	const int Index = GameClient()->Collision()->GetPureMapIndex(Pos);
	if(Index < 0)
		return false;
	const int aTiles[] = {
		GameClient()->Collision()->GetTileIndex(Index),
		GameClient()->Collision()->GetFrontTileIndex(Index),
		GameClient()->Collision()->GetSwitchType(Index)};
	for(const int Tile : aTiles)
	{
		if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE)
			return true;
	}
	return false;
}
