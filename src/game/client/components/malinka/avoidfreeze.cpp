// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#include "simulation_step.h"
#include <base/time.h>
#include "avoidfreeze.h"

#include <engine/engine.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

constexpr int gs_MalinkaAvoidForceMs = 120;
int64_t gs_MalinkaLastAvoidTime = 0;

void CMalinkaAvoidFreeze::OnReset()
{
	std::fill(std::begin(m_BlatantTrackPointInit), std::end(m_BlatantTrackPointInit), false);
	std::fill(std::begin(m_BlatantTrackPoint), std::end(m_BlatantTrackPoint), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_BlatantHookTarget), std::end(m_BlatantHookTarget), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_BlatantHookHoldTicks), std::end(m_BlatantHookHoldTicks), 0);
	std::fill(std::begin(m_BlatantFreezeResetFrozenLast), std::end(m_BlatantFreezeResetFrozenLast), false);
	std::fill(std::begin(m_BlatantFreezeResetSince), std::end(m_BlatantFreezeResetSince), 0);
	std::fill(std::begin(m_AvoidLastMousePos), std::end(m_AvoidLastMousePos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_AvoidForcing), std::end(m_AvoidForcing), false);
	std::fill(std::begin(m_AvoidForcedDir), std::end(m_AvoidForcedDir), 0);
	std::fill(std::begin(m_AvoidForceUntil), std::end(m_AvoidForceUntil), 0);
	std::fill(std::begin(m_AvoidWasInDanger), std::end(m_AvoidWasInDanger), false);
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

bool CMalinkaAvoidFreeze::IsDangerousMapIndex(int MapIndex, bool DetectFreeze, bool DetectDeath, bool DetectTele)
{
	if(MapIndex < 0)
		return false;

	if(DetectFreeze)
	{
		const int Tile = GameClient()->Collision()->GetTileIndex(MapIndex);
		const int FrontTile = GameClient()->Collision()->GetFrontTileIndex(MapIndex);
		if(IsFreezeTile(Tile) || IsFreezeTile(FrontTile))
			return true;
	}

	if(DetectDeath)
	{
		if(GameClient()->Collision()->GetTileIndex(MapIndex) == TILE_DEATH || GameClient()->Collision()->GetFrontTileIndex(MapIndex) == TILE_DEATH)
			return true;
	}

	if(DetectTele)
	{
		if(GameClient()->Collision()->IsTeleport(MapIndex) || GameClient()->Collision()->IsEvilTeleport(MapIndex) || GameClient()->Collision()->IsCheckTeleport(MapIndex) ||
			GameClient()->Collision()->IsCheckEvilTeleport(MapIndex) || GameClient()->Collision()->IsTeleportWeapon(MapIndex) || GameClient()->Collision()->IsTeleportHook(MapIndex) ||
			GameClient()->Collision()->IsTeleCheckpoint(MapIndex))
			return true;
	}

	return false;
}

bool CMalinkaAvoidFreeze::MalinkaTryMove(const CNetObj_PlayerInput &BaseInput, int Direction, int CheckTicks)
{
	CNetObj_PlayerInput ModifiedInput = BaseInput;
	if(BaseInput.m_Direction != Direction)
	{
		const int Sensitivity = MalinkaDirectionSensitivityStep();
		if(Direction > BaseInput.m_Direction)
			ModifiedInput.m_Direction = std::min(BaseInput.m_Direction + Sensitivity, Direction);
		else
			ModifiedInput.m_Direction = std::max(BaseInput.m_Direction - Sensitivity, Direction);
	}

	if(MalinkaPredictFreeze(ModifiedInput, CheckTicks) == EFreezePrediction::SAFE)
	{
		const int Local = g_Config.m_ClDummy;
		GameClient()->m_MalinkaAvoidFreeze.m_AvoidForcing[Local] = true;
		GameClient()->m_MalinkaAvoidFreeze.m_AvoidForcedDir[Local] = ModifiedInput.m_Direction;
		GameClient()->m_MalinkaAvoidFreeze.m_AvoidForceUntil[Local] = time_get() + static_cast<int64_t>(gs_MalinkaAvoidForceMs) * time_freq() / 1000;
		return true;
	}
	return false;
}

bool CMalinkaAvoidFreeze::MalinkaTryAvoidFreeze(int LocalPlayerId)
{
	const int CheckTicks = MalinkaAvoidPredictTicks();
	const int MaxAttempts = MalinkaMaxAvoidAttempts();
	const int MaxAttemptsPerDirection = MalinkaMaxAvoidAttemptsPerDirection();
	const CNetObj_PlayerInput BaseInput = GameClient()->m_Controls.m_aInputData[LocalPlayerId];
	const int ClientId = GameClient()->m_aLocalIds[LocalPlayerId];

	const CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
	vec2 DangerDir(0.0f, 0.0f);
	if(pChar && pChar->Core())
	{
		int DangerTick = -1;
		float DangerDistance = 0.0f;
		const auto Prediction = MalinkaPredictFreeze(BaseInput, CheckTicks, &DangerTick, &DangerDistance);
		if(Prediction == EFreezePrediction::UNAVAILABLE)
			return false;
		if(Prediction == EFreezePrediction::DANGER)
		{
			static CGameWorld s_World;
			if(!s_World.CopyWorldIndependent(&GameClient()->m_PredictedWorld))
				return false;
			s_World.m_WorldConfig.m_PredictEvents = false;
			CCharacter *pTestChar = s_World.GetCharacterById(ClientId);
			if(pTestChar)
			{
				for(int i = 0; i < std::clamp(DangerTick, 0, CheckTicks); i++)
				{
					if(!Malinka::TickSimulation(s_World, {{ClientId, BaseInput}}))
						return false;
					pTestChar = s_World.GetCharacterById(ClientId);
					if(!pTestChar)
						return false;
				}
				const vec2 DangerPos = pTestChar->m_Pos;
				const vec2 CurrentPos = pChar->Core()->m_Pos;
				const vec2 DirVec = DangerPos - CurrentPos;
				const float DirLen = length(DirVec);
				if(DirLen > 0.1f)
					DangerDir = DirVec / DirLen;
			}
		}
	}

	std::array<int, 3> aDirections = {-1, 1, 0};
	if(std::fabs(DangerDir.x) > 0.1f)
	{
		const int AwayDir = DangerDir.x > 0.0f ? -1 : 1;
		const int TowardDir = DangerDir.x > 0.0f ? 1 : -1;
		aDirections = {AwayDir, TowardDir, 0};
	}

	std::array<int, 3> aDirectionAttempts = {0, 0, 0};
	int Attempts = 0;
	while(Attempts < MaxAttempts)
	{
		bool TriedAny = false;
		for(size_t i = 0; i < aDirections.size() && Attempts < MaxAttempts; i++)
		{
			if(aDirectionAttempts[i] >= MaxAttemptsPerDirection)
				continue;

			const int Direction = aDirections[i];
			if(Direction == BaseInput.m_Direction && aDirectionAttempts[i] == 0)
			{
				aDirectionAttempts[i]++;
				continue;
			}

			CNetObj_PlayerInput AttemptInput = BaseInput;
			if(aDirectionAttempts[i] > 0 && AttemptInput.m_Hook != 0)
				AttemptInput.m_Hook = 0;

			const int AttemptCheckTicks = std::max(1, CheckTicks - aDirectionAttempts[i]);
			aDirectionAttempts[i]++;
			Attempts++;
			TriedAny = true;

			if(MalinkaTryMove(AttemptInput, Direction, AttemptCheckTicks))
			{
				CNetObj_PlayerInput VerifyInput = AttemptInput;
				if(VerifyInput.m_Direction != Direction)
				{
					const int Sensitivity = MalinkaDirectionSensitivityStep();
					if(Direction > VerifyInput.m_Direction)
						VerifyInput.m_Direction = std::min(VerifyInput.m_Direction + Sensitivity, Direction);
					else
						VerifyInput.m_Direction = std::max(VerifyInput.m_Direction - Sensitivity, Direction);
				}

				if(MalinkaPredictFreeze(VerifyInput, std::min(AttemptCheckTicks + 2, 15)) == EFreezePrediction::SAFE)
					return true;

				GameClient()->m_MalinkaAvoidFreeze.m_AvoidForcing[LocalPlayerId] = false;
			}
		}

		if(!TriedAny)
			break;
	}

	return false;
}

int CMalinkaAvoidFreeze::MalinkaAvoidPredictTicks() const
{
	return std::clamp(g_Config.m_MalinkaAvoidLegitCheckTicks, 1, 100);
}

int CMalinkaAvoidFreeze::MalinkaHookAssistTicks() const
{
	return std::clamp(g_Config.m_MalinkaAvoidLegitUnfreeze ? g_Config.m_MalinkaAvoidLegitUnfreezeTicks : g_Config.m_MalinkaAvoidLegitCheckTicks, 1, 100);
}

int CMalinkaAvoidFreeze::MalinkaDirectionSensitivityStep() const
{
	return g_Config.m_MalinkaAvoidLegitDirectionPriority >= 667 ? 2 : 1;
}

float CMalinkaAvoidFreeze::MalinkaDirectionSensitivityFactor() const
{
	return std::clamp(g_Config.m_MalinkaAvoidLegitDirectionPriority / 1000.0f, 0.10f, 1.0f);
}

int CMalinkaAvoidFreeze::MalinkaMaxAvoidAttempts() const
{
	return std::clamp(g_Config.m_MalinkaAvoidLegitQuality, 1, 100);
}

int CMalinkaAvoidFreeze::MalinkaMaxAvoidAttemptsPerDirection() const
{
	return std::clamp(std::max(1, g_Config.m_MalinkaAvoidLegitRandomness), 1, 100);
}

bool CMalinkaAvoidFreeze::MalinkaGetFreeze(vec2 Pos, int FreezeTime) const
{
	if(FreezeTime > 0)
		return true;

	const float CheckOffset = CCharacterCore::PhysicalSize() / 3.0f;
	const vec2 aCheckPositions[] = {
		Pos,
		{Pos.x + CheckOffset, Pos.y - CheckOffset},
		{Pos.x + CheckOffset, Pos.y + CheckOffset},
		{Pos.x - CheckOffset, Pos.y - CheckOffset},
		{Pos.x - CheckOffset, Pos.y + CheckOffset},
	};

	for(const vec2 &CheckPos : aCheckPositions)
	{
		const int MapIndex = Collision()->GetPureMapIndex(CheckPos.x, CheckPos.y);
		if(MapIndex < 0)
			continue;

		if(IsFreezeTile(Collision()->GetTileIndex(MapIndex)) || IsFreezeTile(Collision()->GetFrontTileIndex(MapIndex)))
			return true;
		if(g_Config.m_MalinkaAvoidLegitDeath &&
			(Collision()->GetTileIndex(MapIndex) == TILE_DEATH || Collision()->GetFrontTileIndex(MapIndex) == TILE_DEATH))
			return true;
		if(g_Config.m_MalinkaAvoidLegitTeles &&
			(Collision()->IsTeleport(MapIndex) || Collision()->IsCheckEvilTeleport(MapIndex) || Collision()->IsCheckTeleport(MapIndex) ||
				Collision()->IsEvilTeleport(MapIndex) || Collision()->IsTeleportWeapon(MapIndex) || Collision()->IsTeleportHook(MapIndex) ||
				Collision()->IsTeleCheckpoint(MapIndex)))
			return true;
	}

	return false;
}

bool CMalinkaAvoidFreeze::MalinkaAutomationAllowed() const
{
	if(!GameClient() || !GameClient()->Predict())
		return false;
	if(GameClient()->m_Snap.m_SpecInfo.m_Active || GameClient()->m_Chat.IsActive() || GameClient()->m_Menus.IsActive())
		return false;
	return true;
}

void CMalinkaAvoidFreeze::MalinkaAvoidFreeze()
{
	if(!g_Config.m_MalinkaAvoidfreeze || g_Config.m_MalinkaAvoidMode != 1 || !g_Config.m_MalinkaAvoidLegitDirection)
		return;
	if(!MalinkaAutomationAllowed())
		return;

	const int64_t CurrentTime = time_get();
	if(!MalinkaIsAvoidCooldownElapsed(CurrentTime))
		return;

	const int LocalPlayerId = g_Config.m_ClDummy;
	if(GameClient()->m_MalinkaAvoidFreeze.m_AvoidForcing[LocalPlayerId] && CurrentTime <= GameClient()->m_MalinkaAvoidFreeze.m_AvoidForceUntil[LocalPlayerId])
		return;
	if(!GameClient()->m_Controls.IsPlayerActive(LocalPlayerId))
		return;

	int DangerTick = -1;
	float DangerDistance = 0.0f;
	const auto InDanger = MalinkaPredictFreeze(GameClient()->m_Controls.m_aInputData[LocalPlayerId], MalinkaAvoidPredictTicks(), &DangerTick, &DangerDistance);
	if(InDanger != EFreezePrediction::DANGER)
	{
		GameClient()->m_MalinkaAvoidFreeze.m_AvoidWasInDanger[LocalPlayerId] = false;
		return;
	}

	const int Level = std::clamp(MalinkaAvoidPredictTicks(), 1, 5);
	const float SensitivityFactor = MalinkaDirectionSensitivityFactor();
	const float BaseTickLimit = Level <= 2 ? (MalinkaAvoidPredictTicks() / 4.0f) :
						 (Level <= 4 ? (MalinkaAvoidPredictTicks() / 3.0f) : (MalinkaAvoidPredictTicks() / 2.0f));
	const float SensitivityAdjustment = 0.5f + (0.5f * SensitivityFactor);
	const int CloseTickLimit = std::max(1, (int)(BaseTickLimit * SensitivityAdjustment));

	float BaseDistanceThreshold = 48.0f;
	if(Level == 2)
		BaseDistanceThreshold = 64.0f;
	else if(Level == 3)
		BaseDistanceThreshold = 80.0f;
	else if(Level == 4)
		BaseDistanceThreshold = 96.0f;
	else if(Level >= 5)
		BaseDistanceThreshold = 112.0f;

	const float DistanceThreshold = BaseDistanceThreshold * (0.6f + (0.8f * SensitivityFactor));
	if(DangerTick > CloseTickLimit && DangerDistance > DistanceThreshold)
	{
		GameClient()->m_MalinkaAvoidFreeze.m_AvoidWasInDanger[LocalPlayerId] = false;
		return;
	}

	if(GameClient()->m_MalinkaAvoidFreeze.m_AvoidWasInDanger[LocalPlayerId])
		return;

	if(MalinkaTryAvoidFreeze(LocalPlayerId))
	{
		m_AvoidWasInDanger[LocalPlayerId] = true;
		MalinkaUpdateAvoidCooldown(CurrentTime);
	}
}

void CMalinkaAvoidFreeze::MalinkaHookAssist()
{
	if(!g_Config.m_MalinkaAvoidfreeze || g_Config.m_MalinkaAvoidMode != 1 || !g_Config.m_MalinkaAvoidLegitHook)
		return;
	if(!MalinkaAutomationAllowed())
		return;

	static int s_aLastHookTick[NUM_DUMMIES] = {-1, -1};
	const int Local = g_Config.m_ClDummy;
	const int CurrentPredTick = GameClient()->m_PredictedWorld.GameTick();
	if(CurrentPredTick == s_aLastHookTick[Local])
		return;
	s_aLastHookTick[Local] = CurrentPredTick;

	const int ClientId = GameClient()->m_aLocalIds[Local];
	const CCharacter *pChar = ClientId >= 0 ? GameClient()->m_PredictedWorld.GetCharacterById(ClientId) : nullptr;
	const CCharacterCore *pCore = pChar ? pChar->Core() : nullptr;
	if(!pChar || !pCore)
		return;

	const bool AimingUp = GameClient()->m_Controls.m_aInputData[Local].m_TargetY < 0;
	const int BaseCheckTicks = MalinkaHookAssistTicks();
	const int CheckTicks = std::max(1, BaseCheckTicks + ((BaseCheckTicks >= 1 && BaseCheckTicks <= 3 && AimingUp) ? 1 : 0));
	const float SensitivityFactor = std::clamp(BaseCheckTicks / 10.0f, 0.2f, 1.0f);
	const bool CurrentlyHoldingHook = GameClient()->m_Controls.m_aInputData[Local].m_Hook != 0;
	const float Speed = length(pCore->m_Vel);

	if(CurrentlyHoldingHook)
	{
		CNetObj_PlayerInput TestInput = GameClient()->m_Controls.m_aInputData[Local];
		TestInput.m_Hook = 1;

		int DangerTick = -1;
		float DangerDistance = 0.0f;
		if(MalinkaPredictFreeze(TestInput, CheckTicks, &DangerTick, &DangerDistance) == EFreezePrediction::DANGER)
		{
			CNetObj_PlayerInput ReleaseInput = GameClient()->m_Controls.m_aInputData[Local];
			ReleaseInput.m_Hook = 0;
			const bool CanAvoidByReleasing = MalinkaPredictFreeze(ReleaseInput, CheckTicks) == EFreezePrediction::SAFE;
			if(!CanAvoidByReleasing && DangerTick > CheckTicks / 2)
				return;

			const float SpeedFactor = std::min(1.0f, Speed / 10.0f);
			const float UrgencyMultiplier = 0.3f + (0.7f * SensitivityFactor) - (0.2f * SpeedFactor);
			const float DistanceMultiplier = 0.5f + (1.5f * SensitivityFactor) + (0.5f * SpeedFactor);
			const int HookUrgencyTicks = std::max(1, (int)(CheckTicks * UrgencyMultiplier));
			const float HookDistanceThreshold = 32.0f * (1.5f + BaseCheckTicks * DistanceMultiplier);
			if((DangerTick <= HookUrgencyTicks || DangerDistance <= HookDistanceThreshold) && CanAvoidByReleasing)
				GameClient()->m_Controls.m_aInputData[Local].m_Hook = 0;
		}
	}
	else
	{
		CNetObj_PlayerInput TestInput = GameClient()->m_Controls.m_aInputData[Local];
		TestInput.m_Hook = 1;

		int DangerTick = -1;
		float DangerDistance = 0.0f;
		if(MalinkaPredictFreeze(TestInput, CheckTicks, &DangerTick, &DangerDistance) == EFreezePrediction::DANGER)
		{
			CNetObj_PlayerInput NoHookInput = GameClient()->m_Controls.m_aInputData[Local];
			NoHookInput.m_Hook = 0;
			const bool CanMoveWithoutHook = MalinkaPredictFreeze(NoHookInput, CheckTicks) == EFreezePrediction::SAFE;
			const float UrgencyMultiplier = 0.2f + (0.6f * SensitivityFactor);
			const int HookUrgencyTicks = std::max(1, (int)(CheckTicks * UrgencyMultiplier));
			if(DangerTick <= HookUrgencyTicks && DangerDistance < 48.0f && CanMoveWithoutHook)
				GameClient()->m_Controls.m_aInputData[Local].m_Hook = 0;
		}
	}
}

bool CMalinkaAvoidFreeze::MalinkaIsAvoidCooldownElapsed(int64_t CurrentTime) const
{
	const int64_t ConfiguredDelay = static_cast<int64_t>(std::max(0, g_Config.m_MalinkaAvoidLegitAfkProtection ? g_Config.m_MalinkaAvoidLegitAfkTime * 100 : 0)) * time_freq() / 1000;
	if(gs_MalinkaLastAvoidTime == 0)
		return true;
	return CurrentTime - gs_MalinkaLastAvoidTime >= ConfiguredDelay;
}

void CMalinkaAvoidFreeze::MalinkaUpdateAvoidCooldown(int64_t CurrentTime)
{
	gs_MalinkaLastAvoidTime = CurrentTime + static_cast<int64_t>(gs_MalinkaAvoidForceMs) * time_freq() / 1000;
}

CMalinkaAvoidFreeze::EFreezePrediction CMalinkaAvoidFreeze::MalinkaPredictFreeze(const CNetObj_PlayerInput &Input, int Ticks, int *pDangerTick, float *pDangerDistance) const
{
	if(pDangerTick)
		*pDangerTick = -1;
	if(pDangerDistance)
		*pDangerDistance = 0.0f;
	if(!GameClient()->Predict())
		return EFreezePrediction::UNAVAILABLE;
	static CGameWorld s_World;
	if(!s_World.CopyWorldIndependent(&GameClient()->m_PredictedWorld))
		return EFreezePrediction::UNAVAILABLE;
	s_World.m_WorldConfig.m_PredictEvents = false;
	const int ClientId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	CCharacter *pChar = s_World.GetCharacterById(ClientId);
	if(!pChar)
		return EFreezePrediction::UNAVAILABLE;
	const vec2 StartPos = pChar->GetPos();
	for(int Step = 0; Step < std::clamp(Ticks, 1, 100); ++Step)
	{
		if(!Malinka::TickSimulation(s_World, {{ClientId, Input}}))
			return EFreezePrediction::UNAVAILABLE;
		pChar = s_World.GetCharacterById(ClientId);
		if(!pChar)
			return EFreezePrediction::UNAVAILABLE;
		if(MalinkaGetFreeze(pChar->m_Pos, pChar->m_FreezeTime))
		{
			if(pDangerTick)
				*pDangerTick = Step + 1;
			if(pDangerDistance)
				*pDangerDistance = distance(StartPos, pChar->m_Pos);
			return EFreezePrediction::DANGER;
		}
	}
	return EFreezePrediction::SAFE;
}

bool CMalinkaAvoidFreeze::HasDangerousTile(vec2 Pos, bool DetectFreeze, bool DetectDeath, bool DetectTele)
{
	return IsDangerousMapIndex(GameClient()->Collision()->GetPureMapIndex(Pos), DetectFreeze, DetectDeath, DetectTele);
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

bool CMalinkaAvoidFreeze::IsSolidTileAt(int TileX, int TileY)
{
	if(TileX < 0 || TileY < 0 || TileX >= GameClient()->Collision()->GetWidth() || TileY >= GameClient()->Collision()->GetHeight())
		return false;
	const int Index = TileY * GameClient()->Collision()->GetWidth() + TileX;
	const int Tile = GameClient()->Collision()->GetTileIndex(Index);
	const int FrontTile = GameClient()->Collision()->GetFrontTileIndex(Index);
	return Tile == TILE_SOLID || FrontTile == TILE_SOLID;
}

bool CMalinkaAvoidFreeze::FindBlatantHookTarget(vec2 Pos, vec2 Vel, vec2 GoalPos, float HookLength, bool DangerAhead, vec2 &OutTarget)
{
	const int TileRadius = std::max(2, (int)std::ceil(HookLength / 32.0f) + 1);
	const int CenterTileX = std::clamp((int)std::floor(Pos.x / 32.0f), 0, GameClient()->Collision()->GetWidth() - 1);
	const int CenterTileY = std::clamp((int)std::floor(Pos.y / 32.0f), 0, GameClient()->Collision()->GetHeight() - 1);
	const vec2 ToGoal = length(GoalPos - Pos) > 0.001f ? normalize(GoalPos - Pos) : vec2(1.0f, 0.0f);
	const float GoalSign = GoalPos.x >= Pos.x ? 1.0f : -1.0f;
	const float Speed = length(Vel);

	float BestScore = -std::numeric_limits<float>::infinity();
	vec2 BestTarget(0.0f, 0.0f);

	for(int TileY = CenterTileY - TileRadius; TileY <= CenterTileY + TileRadius; ++TileY)
	{
		for(int TileX = CenterTileX - TileRadius; TileX <= CenterTileX + TileRadius; ++TileX)
		{
			if(!IsSolidTileAt(TileX, TileY))
				continue;

			const vec2 TilePos(TileX * 32.0f + 32.0f * 0.5f, TileY * 32.0f + 32.0f * 0.5f);
			const vec2 Delta = TilePos - Pos;
			const float Dist = length(Delta);
			if(Dist < 20.0f || Dist > HookLength)
				continue;

			vec2 CollisionPos;
			vec2 BeforeCollision;
			if(GameClient()->Collision()->IntersectLine(Pos, TilePos, &CollisionPos, &BeforeCollision) == TILE_NOHOOK)
				continue;
			if(distance(CollisionPos, TilePos) > 24.0f && distance(BeforeCollision, TilePos) > 24.0f)
				continue;

			const vec2 Dir = normalize(Delta);
			const float PullUp = std::max(0.0f, -Dir.y);
			const float PullDown = std::max(0.0f, Dir.y);
			const float HorizontalTowardGoal = Dir.x * GoalSign;
			const float GoalAlignment = dot(Dir, ToGoal);
			float Score =
				GoalAlignment * 5.5f +
				HorizontalTowardGoal * 7.0f +
				PullUp * 7.5f -
				PullDown * 8.5f -
				(Dist / HookLength) * 1.75f;
			if(TilePos.y < Pos.y - 12.0f)
				Score += 2.5f;
			if(TilePos.y > Pos.y + 12.0f)
				Score -= 3.0f;
			if(std::abs(Dir.x) < 0.10f)
				Score -= 1.5f;
			if(DangerAhead && PullUp > 0.15f)
				Score += 4.5f;
			if(Speed > 2.5f)
			{
				const vec2 VelDir = normalize(Vel);
				Score += dot(VelDir, Dir) * 1.5f;
				if(dot(VelDir, Dir) < -0.25f)
					Score -= 4.0f;
			}

			if(Score > BestScore)
			{
				BestScore = Score;
				BestTarget = TilePos;
			}
		}
	}

	if(BestScore == -std::numeric_limits<float>::infinity())
		return false;

	OutTarget = BestTarget;
	return true;
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

bool CMalinkaAvoidFreeze::HasBlatantDangerAhead(const CCharacter *pCharacter, int Direction, int CheckTicks, bool DetectFreeze, bool DetectDeath, bool DetectTele, bool DetectUnfreeze)
{
	if(pCharacter == nullptr)
		return false;

	const vec2 Pos = pCharacter->GetPos();
	const vec2 Vel = pCharacter->Core()->m_Vel;
	for(int Tick = 1; Tick <= std::max(1, CheckTicks); ++Tick)
	{
		vec2 FuturePos = Pos + Vel * (0.65f * Tick);
		FuturePos.x += Direction * Tick * 4.0f;
		const int MapIndex = GameClient()->Collision()->GetPureMapIndex(FuturePos);
		if(MapIndex < 0)
			return true;
		if(IsDangerousMapIndex(MapIndex, DetectFreeze || DetectUnfreeze, DetectDeath, DetectTele))
			return true;
	}

	return false;
}

int CMalinkaAvoidFreeze::GetBlatantDangerTick(const CCharacter *pCharacter, int Direction, int CheckTicks, bool DetectFreeze, bool DetectDeath, bool DetectTele, bool DetectUnfreeze)
{
	if(pCharacter == nullptr)
		return -1;

	const vec2 Pos = pCharacter->GetPos();
	const vec2 Vel = pCharacter->Core()->m_Vel;
	for(int Tick = 1; Tick <= std::max(1, CheckTicks); ++Tick)
	{
		vec2 FuturePos = Pos + Vel * (0.65f * Tick);
		FuturePos.x += Direction * Tick * 4.0f;
		const int MapIndex = GameClient()->Collision()->GetPureMapIndex(FuturePos);
		if(MapIndex < 0)
			return Tick;
		if(IsDangerousMapIndex(MapIndex, DetectFreeze || DetectUnfreeze, DetectDeath, DetectTele))
			return Tick;
	}

	return -1;
}

int CMalinkaAvoidFreeze::SimulateBlatantFreezeTick(const CNetObj_PlayerInput &BaseInput, int Ticks, bool DetectFreeze, bool DetectDeath, bool DetectTele, bool DetectUnfreeze)
{
	int LocalId = GameClient()->m_Snap.m_LocalClientId;
	CGameWorld SimWorld;
	if(!SimWorld.CopyWorldIndependent(&GameClient()->m_PredictedWorld))
		return 0;
	CCharacter *pSimChar = SimWorld.GetCharacterById(LocalId);
	if(pSimChar == nullptr)
		return 0;

	SimWorld.m_WorldConfig.m_PredictEvents = false;
	for(int Tick = 1; Tick <= std::clamp(Ticks, 1, 100); ++Tick)
	{
		if(!Malinka::TickSimulation(SimWorld, {{LocalId, BaseInput}}))
			return 0;
		pSimChar = SimWorld.GetCharacterById(LocalId);
		if(pSimChar == nullptr)
			return Tick;

		const vec2 Pos = pSimChar->GetPos();
		const int MapIndex = SimWorld.Collision()->GetPureMapIndex(Pos);
		if(MapIndex < 0)
			return Tick;
		if((DetectFreeze || DetectUnfreeze) && (pSimChar->m_FreezeTime > 0 || pSimChar->Core()->m_DeepFrozen || pSimChar->Core()->m_LiveFrozen))
			return Tick;
		if(IsDangerousMapIndex(MapIndex, DetectFreeze || DetectUnfreeze, DetectDeath, DetectTele))
			return Tick;
	}

	return -1;
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
