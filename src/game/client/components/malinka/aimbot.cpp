// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#include <base/time.h>
#include <algorithm>
#include "aimbot.h"

#include <base/vmath.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

void CMalinkaAimbot::OnReset()
{
	std::fill(std::begin(m_WeaponTargetId), std::end(m_WeaponTargetId), -1);
	std::fill(std::begin(m_HookTargetId), std::end(m_HookTargetId), -1);
	std::fill(std::begin(m_WeaponAimPos), std::end(m_WeaponAimPos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_HookAimPos), std::end(m_HookAimPos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_WeaponWorldPos), std::end(m_WeaponWorldPos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_HookWorldPos), std::end(m_HookWorldPos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_NextAutoShoot), std::end(m_NextAutoShoot), 0);
	std::fill(std::begin(m_ScheduledTargetId), std::end(m_ScheduledTargetId), -1);
}

void CMalinkaAimbot::PulseFire(CNetObj_PlayerInput &Input)
{
	Input.m_Fire = (Input.m_Fire + 1) & INPUT_STATE_MASK;
}

int CMalinkaAimbot::FindClosestTargetClientId(const CCharacter *pLocalCharacter)
{
	if(pLocalCharacter == nullptr)
		return -1;

	int ClosestClientId = -1;
	float ClosestDistance = 1e9f;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(ClientId == GameClient()->m_Snap.m_LocalClientId || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;

		const CCharacter *pTarget = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
		const vec2 TargetPos = pTarget != nullptr ? pTarget->GetPos() : GameClient()->m_aClients[ClientId].m_RenderPos;
		const float Distance = distance(pLocalCharacter->GetPos(), TargetPos);
		if(Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestClientId = ClientId;
		}
	}

	return ClosestClientId;
}

vec2 CMalinkaAimbot::GetAimToClient(const CCharacter *pLocalCharacter, int ClientId, vec2 Fallback)
{
	if(pLocalCharacter == nullptr || ClientId < 0 || ClientId >= MAX_CLIENTS || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		return GameClient()->m_Controls.EnsureValidAim(Fallback);

	const CCharacter *pTarget = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
	const vec2 TargetPos = pTarget != nullptr ? pTarget->GetPos() : GameClient()->m_aClients[ClientId].m_RenderPos;
	return GameClient()->m_Controls.EnsureValidAim(TargetPos - pLocalCharacter->GetPos());
}

vec2 CMalinkaAimbot::BuildFakeAimPos(int Dummy, vec2 RealPos, CCharacter *pLocalCharacter, int64_t Now)
{
	const float Radius = std::max(96.0f, length(RealPos));
	const float AngleSpeed = 0.9f + g_Config.m_MalinkaFakeAimSpeed * 0.22f;

	switch(g_Config.m_MalinkaFakeAimMode)
	{
	case 0:
		return GameClient()->m_Controls.EnsureValidAim(RealPos);
	case 1:
		return GameClient()->m_Controls.EnsureValidAim(RealPos);
	case 2:
	{
		const float Angle = GameClient()->Client()->LocalTime() * AngleSpeed * 2.0f * pi;
		return vec2(std::cos(Angle), std::sin(Angle)) * Radius;
	}
	case 3:
	{
		const int64_t Interval = std::max<int64_t>(1, time_freq() / std::max(2, g_Config.m_MalinkaFakeAimSpeed * 5));
		if(Now > GameClient()->m_Controls.m_aMalinkaFakeAimLastRandomUpdate[Dummy] + Interval)
		{
			GameClient()->m_Controls.m_aMalinkaFakeAimLastRandomUpdate[Dummy] = Now;
			GameClient()->m_Controls.m_aMalinkaFakeAimAngle[Dummy] = (rand() / (float)RAND_MAX) * 2.0f * pi;
		}
		const float Angle = GameClient()->m_Controls.m_aMalinkaFakeAimAngle[Dummy];
		return vec2(std::cos(Angle), std::sin(Angle)) * Radius;
	}
	case 4:
		return GameClient()->m_Controls.EnsureValidAim(-RealPos);
	case 5:
		return GetAimToClient(pLocalCharacter, FindClosestTargetClientId(pLocalCharacter), -RealPos);
	default:
		return GameClient()->m_Controls.EnsureValidAim(RealPos);
	}
}

float CMalinkaAimbot::GetFov(int SlotId)
{
	CMalinkaAimbot::AimbotSlot Slot = static_cast<CMalinkaAimbot::AimbotSlot>(SlotId);
	switch(Slot)
	{
	case CMalinkaAimbot::AimbotSlot::HOOK: return (float)g_Config.m_MalinkaAimbotHookFov;
	case CMalinkaAimbot::AimbotSlot::HAMMER: return (float)g_Config.m_MalinkaAimbotHammerFov;
	case CMalinkaAimbot::AimbotSlot::GUN: return (float)g_Config.m_MalinkaAimbotPistolFov;
	case CMalinkaAimbot::AimbotSlot::SHOTGUN: return (float)g_Config.m_MalinkaAimbotShotgunFov;
	case CMalinkaAimbot::AimbotSlot::GRENADE: return (float)g_Config.m_MalinkaAimbotGrenadeFov;
	case CMalinkaAimbot::AimbotSlot::LASER: return (float)g_Config.m_MalinkaAimbotLaserFov;
	default: return 90.0f;
	}
}

int CMalinkaAimbot::GetPriority(int SlotId)
{
	if(!g_Config.m_MalinkaAimbotAdvancedSettings)
		return g_Config.m_MalinkaAimbotTargetPriority;
	CMalinkaAimbot::AimbotSlot Slot = static_cast<CMalinkaAimbot::AimbotSlot>(SlotId);
	switch(Slot)
	{
	case CMalinkaAimbot::AimbotSlot::HOOK: return g_Config.m_MalinkaAimbotHookTargetPriority;
	case CMalinkaAimbot::AimbotSlot::HAMMER: return g_Config.m_MalinkaAimbotHammerTargetPriority;
	case CMalinkaAimbot::AimbotSlot::GUN: return g_Config.m_MalinkaAimbotPistolTargetPriority;
	case CMalinkaAimbot::AimbotSlot::SHOTGUN: return g_Config.m_MalinkaAimbotShotgunTargetPriority;
	case CMalinkaAimbot::AimbotSlot::GRENADE: return g_Config.m_MalinkaAimbotGrenadeTargetPriority;
	case CMalinkaAimbot::AimbotSlot::LASER: return g_Config.m_MalinkaAimbotLaserTargetPriority;
	default: return g_Config.m_MalinkaAimbotTargetPriority;
	}
}

bool CMalinkaAimbot::GetSilent(int SlotId)
{
	if(!g_Config.m_MalinkaAimbotAdvancedSettings)
		return g_Config.m_MalinkaAimbotSilent != 0;
	CMalinkaAimbot::AimbotSlot Slot = static_cast<CMalinkaAimbot::AimbotSlot>(SlotId);
	switch(Slot)
	{
	case CMalinkaAimbot::AimbotSlot::HOOK: return g_Config.m_MalinkaAimbotHookSilent != 0;
	case CMalinkaAimbot::AimbotSlot::HAMMER: return g_Config.m_MalinkaAimbotHammerSilent != 0;
	case CMalinkaAimbot::AimbotSlot::GUN: return g_Config.m_MalinkaAimbotPistolSilent != 0;
	case CMalinkaAimbot::AimbotSlot::SHOTGUN: return g_Config.m_MalinkaAimbotShotgunSilent != 0;
	case CMalinkaAimbot::AimbotSlot::GRENADE: return g_Config.m_MalinkaAimbotGrenadeSilent != 0;
	case CMalinkaAimbot::AimbotSlot::LASER: return g_Config.m_MalinkaAimbotLaserSilent != 0;
	default: return false;
	}
}

bool CMalinkaAimbot::GetIgnoreFriends(int SlotId)
{
	if(!g_Config.m_MalinkaAimbotAdvancedSettings)
		return g_Config.m_MalinkaAimbotIgnoreFriends != 0;
	CMalinkaAimbot::AimbotSlot Slot = static_cast<CMalinkaAimbot::AimbotSlot>(SlotId);
	switch(Slot)
	{
	case CMalinkaAimbot::AimbotSlot::HOOK: return g_Config.m_MalinkaAimbotHookIgnoreFriends != 0;
	case CMalinkaAimbot::AimbotSlot::HAMMER: return g_Config.m_MalinkaAimbotHammerIgnoreFriends != 0;
	case CMalinkaAimbot::AimbotSlot::GUN: return g_Config.m_MalinkaAimbotPistolIgnoreFriends != 0;
	case CMalinkaAimbot::AimbotSlot::SHOTGUN: return g_Config.m_MalinkaAimbotShotgunIgnoreFriends != 0;
	case CMalinkaAimbot::AimbotSlot::GRENADE: return g_Config.m_MalinkaAimbotGrenadeIgnoreFriends != 0;
	case CMalinkaAimbot::AimbotSlot::LASER: return g_Config.m_MalinkaAimbotLaserIgnoreFriends != 0;
	default: return false;
	}
}

bool CMalinkaAimbot::GetEnabled(int SlotId)
{
	CMalinkaAimbot::AimbotSlot Slot = static_cast<CMalinkaAimbot::AimbotSlot>(SlotId);
	switch(Slot)
	{
	case CMalinkaAimbot::AimbotSlot::HOOK: return g_Config.m_MalinkaAimbotHook != 0;
	case CMalinkaAimbot::AimbotSlot::HAMMER: return g_Config.m_MalinkaAimbotHammer != 0;
	case CMalinkaAimbot::AimbotSlot::GUN: return g_Config.m_MalinkaAimbotPistol != 0;
	case CMalinkaAimbot::AimbotSlot::SHOTGUN: return g_Config.m_MalinkaAimbotShotgun != 0;
	case CMalinkaAimbot::AimbotSlot::GRENADE: return g_Config.m_MalinkaAimbotGrenade != 0;
	case CMalinkaAimbot::AimbotSlot::LASER: return g_Config.m_MalinkaAimbotLaser != 0;
	default: return false;
	}
}

vec2 CMalinkaAimbot::NormalizeMalinkaAim(vec2 Pos)
{
	constexpr float CameraMaxDistance = 200.0f;
	const float FollowFactor = (g_Config.m_ClDyncam ? g_Config.m_ClDyncamFollowFactor : g_Config.m_ClMouseFollowfactor) / 100.0f;
	const float DeadZone = g_Config.m_ClDyncam ? g_Config.m_ClDyncamDeadzone : g_Config.m_ClMouseDeadzone;
	const float MaxDistance = g_Config.m_ClMouseMaxDistance;
	const float MouseMax = std::min((FollowFactor != 0.0f ? CameraMaxDistance / FollowFactor + DeadZone : MaxDistance), MaxDistance);
	const vec2 SafeAim = GameClient()->m_Controls.EnsureValidAim(Pos);
	return normalize(SafeAim) * std::max(1.0f, MouseMax);
}

float CMalinkaAimbot::MalinkaAngleDiff(vec2 From, vec2 To)
{
	return absolute(atan2(std::sin(angle(GameClient()->m_Controls.EnsureValidAim(From)) - angle(GameClient()->m_Controls.EnsureValidAim(To))),
		       std::cos(angle(GameClient()->m_Controls.EnsureValidAim(From)) - angle(GameClient()->m_Controls.EnsureValidAim(To))))) *
	       180.0f / pi;
}

bool CMalinkaAimbot::IsMalinkaTargetAllowed(const CCharacter *pLocalCharacter, int ClientId, int Slot)
{
	if(pLocalCharacter == nullptr || ClientId < 0 || ClientId >= MAX_CLIENTS || ClientId == GameClient()->m_Snap.m_LocalClientId)
		return false;
	if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		return false;
	if(GetIgnoreFriends(Slot))
	{
		const bool FriendByFlag = GameClient()->m_aClients[ClientId].m_Friend;
		const bool FriendByNameClan = GameClient()->Friends()->IsFriend(GameClient()->m_aClients[ClientId].m_aName, GameClient()->m_aClients[ClientId].m_aClan, true);
		const bool FriendByName = GameClient()->Friends()->IsFriend(GameClient()->m_aClients[ClientId].m_aName, "", false);
		if(FriendByFlag || FriendByNameClan || FriendByName)
			return false;
	}
	return true;
}

bool CMalinkaAimbot::HasMalinkaDirectLineOfSight(vec2 From, vec2 To)
{
	vec2 Col;
	vec2 NewPos;
	return !GameClient()->Collision()->IntersectLine(From, To, &Col, &NewPos);
}

vec2 CMalinkaAimbot::PredictMalinkaWeaponTarget(const CCharacter *pLocalCharacter, vec2 TargetPos, vec2 TargetVel, int SlotId)
{
	if(pLocalCharacter == nullptr)
		return TargetPos;

	const vec2 StartPos = pLocalCharacter->GetPos();
	const float Distance = distance(StartPos, TargetPos);
	CMalinkaAimbot::AimbotSlot Slot = static_cast<CMalinkaAimbot::AimbotSlot>(SlotId);
	switch(Slot)
	{
	case CMalinkaAimbot::AimbotSlot::GUN:
		TargetPos += TargetVel * (Distance / std::max(1.0f, (float)GameClient()->m_aTuning[g_Config.m_ClDummy].m_GunSpeed));
		break;
	case CMalinkaAimbot::AimbotSlot::SHOTGUN:
		TargetPos += TargetVel * (Distance / std::max(1.0f, (float)GameClient()->m_aTuning[g_Config.m_ClDummy].m_ShotgunSpeed));
		break;
	case CMalinkaAimbot::AimbotSlot::GRENADE:
		TargetPos += TargetVel * (Distance / std::max(1.0f, (float)GameClient()->m_aTuning[g_Config.m_ClDummy].m_GrenadeSpeed));
		break;
	case CMalinkaAimbot::AimbotSlot::LASER:
		TargetPos += TargetVel * 0.18f;
		break;
	default:
		break;
	}
	return TargetPos;
}

bool CMalinkaAimbot::MalinkaPredictHook(vec2 MyPos, vec2 MyVel, vec2 &TargetPos, vec2 TargetVel)
{
	const vec2 Delta = TargetPos - MyPos;
	const vec2 DeltaVel = TargetVel - MyVel;
	const float HookSpeed = length(TargetVel) + GameClient()->m_aTuning[g_Config.m_ClDummy].m_HookFireSpeed;
	const float A = dot(DeltaVel, DeltaVel) - HookSpeed * HookSpeed;
	const float B = 2.0f * dot(DeltaVel, Delta);
	const float C = dot(Delta, Delta);
	const float Solution = B * B - 4.0f * A * C;
	if(Solution <= 0.0f)
		return false;

	const float Time = absolute(2.0f * C / (std::sqrt(Solution) - B)) + (float)GameClient()->Client()->GetPredictionTime() / 100.0f;
	TargetPos += TargetVel * Time;
	return true;
}

bool CMalinkaAimbot::MalinkaIntersectCharacter(vec2 HookPos, vec2 TargetPos, vec2 &NewPos)
{
	vec2 ClosestPoint;
	if(closest_point_on_line(HookPos, NewPos, TargetPos, ClosestPoint) && distance(TargetPos, ClosestPoint) < CCharacterCore::PhysicalSize() + 2.0f)
	{
		NewPos = ClosestPoint;
		return true;
	}
	return false;
}

bool CMalinkaAimbot::MalinkaHookLineHits(vec2 InitPos, vec2 TargetPos, vec2 ScanDir)
{
	vec2 ExDirection = normalize(ScanDir);
	vec2 FinishPos = InitPos + ExDirection * ((float)GameClient()->m_aTuning[g_Config.m_ClDummy].m_HookLength - CCharacterCore::PhysicalSize() * 1.5f);
	vec2 OldPos = InitPos + ExDirection * CCharacterCore::PhysicalSize() * 1.5f;
	vec2 NewPos = OldPos;
	bool DoBreak = false;

	do
	{
		OldPos = NewPos;
		NewPos = OldPos + ExDirection * GameClient()->m_aTuning[g_Config.m_ClDummy].m_HookFireSpeed;
		if(distance(InitPos, NewPos) > GameClient()->m_aTuning[g_Config.m_ClDummy].m_HookLength)
		{
			NewPos = InitPos + normalize(NewPos - InitPos) * GameClient()->m_aTuning[g_Config.m_ClDummy].m_HookLength;
			DoBreak = true;
		}

		int TeleNr = 0;
		const int Hit = GameClient()->Collision()->IntersectLineTeleHook(OldPos, NewPos, &FinishPos, nullptr, &TeleNr);
		if(MalinkaIntersectCharacter(OldPos, TargetPos, FinishPos))
			return true;
		if(Hit)
			break;

		NewPos.x = round_to_int(NewPos.x);
		NewPos.y = round_to_int(NewPos.y);
		if(OldPos == NewPos)
			break;

		ExDirection.x = round_to_int(ExDirection.x * 256.0f) / 256.0f;
		ExDirection.y = round_to_int(ExDirection.y * 256.0f) / 256.0f;
	} while(!DoBreak);
	return false;
}

vec2 CMalinkaAimbot::MalinkaHookEdgeScan(const CCharacter *pLocalCharacter, vec2 TargetPos, vec2 TargetVel)
{
	if(pLocalCharacter == nullptr)
		return vec2(0.0f, 0.0f);

	vec2 MyPos = pLocalCharacter->GetPos();
	vec2 MyVel = pLocalCharacter->Core()->m_Vel;
	if(!MalinkaPredictHook(MyPos, MyVel, TargetPos, TargetVel))
		return vec2(0.0f, 0.0f);
	if(MalinkaHookLineHits(MyPos, TargetPos, TargetPos - MyPos))
		return TargetPos - MyPos;

	const float VisibleAngle = atan2(TargetPos.y - MyPos.y, TargetPos.x - MyPos.x) + pi * 0.5f;
	const float Accuracy = std::max(1.0f, (float)(g_Config.m_MalinkaAimbotAdvancedSettings ? g_Config.m_MalinkaAimbotHookAccuracy : g_Config.m_MalinkaAimbotEdgeAccuracy));
	vec2 aHitPoints[128];
	int HitPointsCount = 0;
	for(float Angle = VisibleAngle; Angle < pi + VisibleAngle; Angle += 1.0f / Accuracy)
	{
		if(HitPointsCount >= 128)
			break;
		const vec2 Pos = vec2(round_to_int(TargetPos.x + std::cos(Angle) * CCharacterCore::PhysicalSize()), round_to_int(TargetPos.y + std::sin(Angle) * CCharacterCore::PhysicalSize()));
		const vec2 Dir = Pos - MyPos;
		if(MalinkaHookLineHits(MyPos, TargetPos, Dir))
			aHitPoints[HitPointsCount++] = Dir;
	}

	if(HitPointsCount <= 0)
		return vec2(0.0f, 0.0f);
	return aHitPoints[(HitPointsCount - 1) / 2];
}

bool CMalinkaAimbot::FindTarget(const CCharacter *pLocalCharacter, int SlotId, vec2 RealAimPos, SMalinkaAimbotTarget *pOutTarget)
{
	CMalinkaAimbot::AimbotSlot Slot = static_cast<CMalinkaAimbot::AimbotSlot>(SlotId);
	if(GameClient() == nullptr || pLocalCharacter == nullptr || pOutTarget == nullptr || !GetEnabled(SlotId))
		return false;

	const vec2 LocalPos = pLocalCharacter->GetPos();
	const float Fov = GetFov(SlotId); // TODO: make it accept CMalinkaAimbot::AimbotSlot
	const int Priority = GetPriority(SlotId);
	SMalinkaAimbotTarget BestTarget;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!IsMalinkaTargetAllowed(pLocalCharacter, ClientId, SlotId))
			continue;

		CCharacter *pTarget = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
		if(pTarget == nullptr)
			continue;

		vec2 WorldPos = pTarget->GetPos();
		vec2 AimPos = WorldPos - LocalPos;
		if(Slot == CMalinkaAimbot::AimbotSlot::HOOK)
			AimPos = MalinkaHookEdgeScan(pLocalCharacter, WorldPos, pTarget->Core()->m_Vel);
		else if(Slot == CMalinkaAimbot::AimbotSlot::HAMMER)
		{
			if(distance(LocalPos, WorldPos) > pLocalCharacter->GetProximityRadius())
				continue;
			if(!HasMalinkaDirectLineOfSight(LocalPos, WorldPos))
				continue;
		}
		else
		{
			WorldPos = PredictMalinkaWeaponTarget(pLocalCharacter, WorldPos, pTarget->Core()->m_Vel, SlotId);
			if(!HasMalinkaDirectLineOfSight(LocalPos, WorldPos))
				continue;
			AimPos = WorldPos - LocalPos;
		}

		if(length(AimPos) < 0.001f)
			continue;
		if(MalinkaAngleDiff(RealAimPos, AimPos) > Fov * 0.5f)
			continue;

		const float Score = Priority == 0 ? MalinkaAngleDiff(RealAimPos, AimPos) : distance(LocalPos, pTarget->GetPos());
		if(BestTarget.m_ClientId == -1 || Score < BestTarget.m_Score)
		{
			BestTarget.m_ClientId = ClientId;
			BestTarget.m_AimPos = NormalizeMalinkaAim(AimPos);
			BestTarget.m_WorldPos = WorldPos;
			BestTarget.m_Score = Score;
			BestTarget.m_Visible = true;
		}
	}

	if(BestTarget.m_ClientId < 0)
		return false;

	*pOutTarget = BestTarget;

	return true;
}