// Malinka input integration. Ported from Pasta-Client; see docs/MALINKA_SOURCES.md.
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <base/time.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
constexpr int64_t gs_MalinkaSelfHitCooldownMs = 300;
constexpr int64_t gs_MalinkaRehookCooldownMs = 2800;
constexpr int64_t gs_MalinkaAutoAledCooldownMs = 250;
}

void CControls::TriggerFireTap(int Dummy)
{
	m_aInputData[Dummy].m_Fire = (m_aInputData[Dummy].m_Fire + 1) & INPUT_STATE_MASK;
	GameClient()->m_MalinkaLaserUnfreeze.m_PendingSelfHitFireRelease[Dummy] = true;
}

bool CControls::IsMouseMoved(int LocalPlayerId) const
{
	const bool HasMoved = m_aMousePos[LocalPlayerId] != GameClient()->m_MalinkaAvoidFreeze.m_AvoidLastMousePos[LocalPlayerId];
	GameClient()->m_MalinkaAvoidFreeze.m_AvoidLastMousePos[LocalPlayerId] = m_aMousePos[LocalPlayerId];
	return HasMoved;
}

bool CControls::IsPlayerActive(int LocalPlayerId) const
{
	const CNetObj_PlayerInput &Input = m_aInputData[LocalPlayerId];
	return Input.m_Direction != 0 || Input.m_Jump != 0 || Input.m_Hook != 0 || IsMouseMoved(LocalPlayerId);
}

vec2 CControls::EnsureValidAim(vec2 Pos)
{
	return Malinka::FiniteAim(Pos);
}

void CControls::ResetMalinkaInput(int Dummy)
{
	m_aMalinkaInjectedHook[Dummy] = false;
	m_aMalinkaInjectedJump[Dummy] = false;
	m_aRawFireState[Dummy] = 0;
	m_aRawJumpState[Dummy] = 0;
	m_aRawHookState[Dummy] = 0;
	m_aMalinkaLastHookState[Dummy] = HOOK_IDLE;
	m_aMalinkaMoonwalkFlip[Dummy] = 0;
	m_aMalinkaAntiAfkPulseEnd[Dummy] = 0;
	m_aMalinkaLastAntiAfkPulse[Dummy] = time_get();
	m_aMalinkaLastRehook[Dummy] = 0;
	m_aMalinkaPendingRehookRelease[Dummy] = false;
	m_aMalinkaAssistAimActive[Dummy] = false;
	m_aMalinkaAssistAimWeapon[Dummy] = -1;
	m_aMalinkaAssistAimPos[Dummy] = vec2(0.0f, 0.0f);
	m_aMalinkaGhostMoveWarned[Dummy] = false;
	m_aMalinkaRenderMouseOverride[Dummy] = false;
	m_aMalinkaRenderMouseTargetId[Dummy] = -1;
	m_aMalinkaInputActivity[Dummy] = time_get();
	m_aMalinkaVisibleMousePos[Dummy] = vec2(0.0f, 0.0f);
	m_aMalinkaSentMousePos[Dummy] = vec2(1.0f, 0.0f);
	m_aMalinkaFakeAimAngle[Dummy] = 0.0f;
	m_aMalinkaFakeAimLastRandomUpdate[Dummy] = 0;
}

void CControls::RestoreMalinkaButtons(int Dummy)
{
	if(m_aMalinkaInjectedHook[Dummy])
		m_aInputData[Dummy].m_Hook = m_aRawHookState[Dummy] != 0;
	if(m_aMalinkaInjectedJump[Dummy])
		m_aInputData[Dummy].m_Jump = m_aRawJumpState[Dummy] != 0;
	m_aMalinkaInjectedHook[Dummy] = false;
	m_aMalinkaInjectedJump[Dummy] = false;
}

void CControls::ApplyMalinkaInput(vec2 Pos)
{
	if(!static_cast<IEngineGraphics *>(Graphics())->WindowActive() || GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive() || GameClient()->m_GameConsole.IsActive() || GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;
	Pos = EnsureValidAim(Pos);
	vec2 SentAimPos = Pos;
	vec2 VisibleAimPos = Pos;
		const int Dummy = g_Config.m_ClDummy;
		const int BaseHook = m_aInputData[Dummy].m_Hook;
		const int BaseJump = m_aInputData[Dummy].m_Jump;
		const int64_t Now = time_get();
		const bool LeftHeld = m_aInputDirectionLeft[Dummy] != 0;
		const bool RightHeld = m_aInputDirectionRight[Dummy] != 0;
		const bool JumpHeld = m_aRawJumpState[Dummy] != 0;
		const bool HookHeld = m_aRawHookState[Dummy] != 0;
		const bool FireHeld = m_aRawFireState[Dummy] != 0;
		GameClient()->m_MalinkaAimbot.m_WeaponTargetId[Dummy] = -1;
		GameClient()->m_MalinkaAimbot.m_HookTargetId[Dummy] = -1;
		GameClient()->m_MalinkaAimbot.m_WeaponAimPos[Dummy] = vec2(0.0f, 0.0f);
		GameClient()->m_MalinkaAimbot.m_HookAimPos[Dummy] = vec2(0.0f, 0.0f);
		GameClient()->m_MalinkaAimbot.m_WeaponWorldPos[Dummy] = vec2(0.0f, 0.0f);
		GameClient()->m_MalinkaAimbot.m_HookWorldPos[Dummy] = vec2(0.0f, 0.0f);
		m_aMalinkaRenderMouseOverride[Dummy] = false;
		m_aMalinkaRenderMouseTargetId[Dummy] = -1;
		m_aMalinkaAssistAimActive[Dummy] = false;
		m_aMalinkaAssistAimWeapon[Dummy] = -1;
		m_aMalinkaAssistAimPos[Dummy] = vec2(0.0f, 0.0f);
		if(GameClient()->m_MalinkaLaserUnfreeze.m_PendingSelfHitFireRelease[Dummy])
		{
			if(m_aInputData[Dummy].m_Fire & 1)
				GameClient()->m_MalinkaAimbot.PulseFire(m_aInputData[Dummy]);
			GameClient()->m_MalinkaLaserUnfreeze.m_PendingSelfHitFireRelease[Dummy] = false;
		}
		const bool MouseMoved = IsMouseMoved(Dummy);
		if(LeftHeld || RightHeld || JumpHeld || HookHeld || FireHeld || MouseMoved ||
			m_aInputData[Dummy].m_NextWeapon != m_aLastData[Dummy].m_NextWeapon ||
			m_aInputData[Dummy].m_PrevWeapon != m_aLastData[Dummy].m_PrevWeapon ||
			m_aInputData[Dummy].m_WantedWeapon != m_aLastData[Dummy].m_WantedWeapon)
			m_aMalinkaInputActivity[Dummy] = Now;

		CCharacter *pLocalCharacter = nullptr;
		if(!GameClient()->m_Snap.m_SpecInfo.m_Active)
			pLocalCharacter = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_aLocalIds[Dummy]);
		const int HookState = pLocalCharacter != nullptr ? pLocalCharacter->Core()->m_HookState : HOOK_IDLE;
		const bool PulseTick = (Client()->PredGameTick(g_Config.m_ClDummy) & 1) != 0;

		if(g_Config.m_MalinkaMoonwalk && LeftHeld && RightHeld)
		{
			m_aMalinkaMoonwalkFlip[Dummy] ^= 1;
			m_aInputData[Dummy].m_Direction = m_aMalinkaMoonwalkFlip[Dummy] ? -1 : 1;
		}

		const vec2 RealAimPos = Pos;
		// Basic is the only mode with the immediate side probe. It must not
		// override disabled Direction assistance in the configurable modes.
		if(g_Config.m_MalinkaAvoidfreeze && g_Config.m_MalinkaAvoidMode == 0 && pLocalCharacter != nullptr && GameClient()->Predict())
		{
			const int FreezeSide = GameClient()->m_MalinkaAvoidFreeze.DetectFreezeSide(pLocalCharacter->GetPos());
			const bool MovingRight = m_aInputData[Dummy].m_Direction > 0 || pLocalCharacter->Core()->m_Vel.x > 0.35f;
			const bool MovingLeft = m_aInputData[Dummy].m_Direction < 0 || pLocalCharacter->Core()->m_Vel.x < -0.35f;
			if(FreezeSide > 0 && MovingRight && GameClient()->m_MalinkaAvoidFreeze.HasImmediateSideFreeze(pLocalCharacter, 1))
				m_aInputData[Dummy].m_Direction = -1;
			else if(FreezeSide < 0 && MovingLeft && GameClient()->m_MalinkaAvoidFreeze.HasImmediateSideFreeze(pLocalCharacter, -1))
				m_aInputData[Dummy].m_Direction = 1;
		}
		if(g_Config.m_MalinkaAvoidfreeze && pLocalCharacter != nullptr)
		{
			GameClient()->m_MalinkaAvoidFreeze.ApplyAvoid(m_aInputData[Dummy], m_aMalinkaInputActivity[Dummy]);
			// The common output stage writes SentAimPos back into the packet.
			// Previously it silently discarded every Avoid aim adjustment.
			SentAimPos = vec2(m_aInputData[Dummy].m_TargetX, m_aInputData[Dummy].m_TargetY);
		}
		if(g_Config.m_MalinkaAvoidfreeze && g_Config.m_MalinkaAvoidMode == 2 && pLocalCharacter != nullptr && GameClient()->Predict())
		{
			const int LocalId = GameClient()->m_aLocalIds[Dummy];
			const vec2 LocalPos = pLocalCharacter->GetPos();
			const bool FrozenNow = pLocalCharacter->m_FreezeTime > 0 || pLocalCharacter->Core()->m_DeepFrozen || pLocalCharacter->Core()->m_LiveFrozen;
			if(g_Config.m_MalinkaAvoidBlatantFreezeReset)
			{
				if(FrozenNow)
				{
					bool HasFriendNearby = false;
					for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
					{
						if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active || ClientId == LocalId)
							continue;
						const auto &Client = GameClient()->m_aClients[ClientId];
						if(GameClient()->Friends()->IsFriend(Client.m_aName, Client.m_aClan, true) &&
							distance(Client.m_Predicted.m_Pos, LocalPos) < g_Config.m_MalinkaAvoidBlatantFreezeResetFriendDist * 32.0f)
						{
							HasFriendNearby = true;
							break;
						}
					}

					if(!HasFriendNearby)
					{
						if(!GameClient()->m_MalinkaAvoidFreeze.m_BlatantFreezeResetFrozenLast[Dummy])
							GameClient()->m_MalinkaAvoidFreeze.m_BlatantFreezeResetSince[Dummy] = Now;
						else if(Now > GameClient()->m_MalinkaAvoidFreeze.m_BlatantFreezeResetSince[Dummy] + (int64_t)g_Config.m_MalinkaAvoidBlatantFreezeResetWait * time_freq())
							GameClient()->SendKill();
					}
				}
				GameClient()->m_MalinkaAvoidFreeze.m_BlatantFreezeResetFrozenLast[Dummy] = FrozenNow;
			}
		}
		else
		{
			GameClient()->m_MalinkaAvoidFreeze.m_BlatantFreezeResetFrozenLast[Dummy] = false;
			GameClient()->m_MalinkaAvoidFreeze.m_BlatantFreezeResetSince[Dummy] = 0;
		}

		if(g_Config.m_MalinkaPixelWalk && (LeftHeld != RightHeld) && !PulseTick)
			m_aInputData[Dummy].m_Direction = 0;
		if(g_Config.m_MalinkaHookSpam)
			m_aInputData[Dummy].m_Hook = PulseTick ? 1 : 0;
		if(g_Config.m_MalinkaBalanceBot && pLocalCharacter)
		{
			int TargetId = pLocalCharacter->Core()->HookedPlayer();
			if(TargetId < 0 || TargetId >= MAX_CLIENTS)
				TargetId = GameClient()->m_MalinkaAimbot.FindClosestTargetClientId(pLocalCharacter);
			if(const auto *pTarget = GameClient()->m_PredictedWorld.GetCharacterById(TargetId))
			{
				const float Delta = pTarget->GetPos().x - pLocalCharacter->GetPos().x;
				const float DeadZone = static_cast<float>(g_Config.m_MalinkaBalanceBotOffset);
				if(distance(pTarget->GetPos(), pLocalCharacter->GetPos()) <= 800.0f)
					m_aInputData[Dummy].m_Direction = Delta > DeadZone ? 1 : Delta < -DeadZone ? -1 : 0;
			}
		}
		if(g_Config.m_MalinkaHookNearestCollisionAutoHook && pLocalCharacter)
		{
			const float CenterAngle = angle(RealAimPos);
			const float Fov = std::clamp(g_Config.m_MalinkaHookNearestCollisionFov, 1, 360) * pi / 180.0f;
			const float Reach = std::clamp(static_cast<float>(pLocalCharacter->GetTuning(pLocalCharacter->GetOverriddenTuneZone())->m_HookLength), 1.0f, 2000.0f);
			float BestDistance = Reach + 1.0f;
			vec2 BestAim{};
			for(int Ray = 0; Ray <= 64; ++Ray)
			{
				const vec2 Direction = direction(CenterAngle - Fov * 0.5f + Fov * Ray / 64.0f);
				vec2 Hit = pLocalCharacter->GetPos() + Direction * Reach, Before = Hit;
				int TeleNr = 0;
				const int Tile = Collision()->IntersectLineTeleHook(pLocalCharacter->GetPos(), pLocalCharacter->GetPos() + Direction * Reach, &Hit, &Before, &TeleNr);
				const float Dist = distance(pLocalCharacter->GetPos(), Hit);
				if(Tile != 0 && Tile != TILE_NOHOOK && TeleNr == 0 && std::isfinite(Dist) && Dist < BestDistance)
				{
					BestDistance = Dist;
					BestAim = Direction * 1000.0f;
				}
			}
			if(BestDistance <= Reach)
			{
				SentAimPos = VisibleAimPos = BestAim;
				m_aInputData[Dummy].m_Hook = 1;
			}
		}

		if(g_Config.m_MalinkaQuickStop && !LeftHeld && !RightHeld && pLocalCharacter != nullptr)
		{
			const bool GroundedOkay = !g_Config.m_MalinkaQuickStopGrounded || pLocalCharacter->IsGrounded();
			if(GroundedOkay && absolute(pLocalCharacter->Core()->m_Vel.x) > 0.6f)
				m_aInputData[Dummy].m_Direction = pLocalCharacter->Core()->m_Vel.x > 0.0f ? -1 : 1;
		}

		if(g_Config.m_MalinkaAutoJumpSave && !JumpHeld && pLocalCharacter != nullptr && GameClient()->m_MalinkaAvoidFreeze.ShouldAutoJumpSave(pLocalCharacter))
			m_aInputData[Dummy].m_Jump = 1;

		if(g_Config.m_MalinkaAutoRehook && pLocalCharacter != nullptr)
		{
			const bool TargetedPlayer = GameClient()->m_MalinkaAvoidFreeze.HasPlayerNearAim(pLocalCharacter, RealAimPos, 380.0f, 0.18f);
			const int64_t Cooldown = gs_MalinkaRehookCooldownMs * time_freq() / 1000;
			if(HookHeld && HookState <= HOOK_IDLE && m_aMalinkaLastHookState[Dummy] > HOOK_IDLE &&
				TargetedPlayer && Now > m_aMalinkaLastRehook[Dummy] + Cooldown)
			{
				m_aMalinkaPendingRehookRelease[Dummy] = true;
				m_aMalinkaLastRehook[Dummy] = Now;
			}

			if(m_aMalinkaPendingRehookRelease[Dummy])
			{
				m_aInputData[Dummy].m_Hook = 0;
				if(!HookHeld)
					m_aMalinkaPendingRehookRelease[Dummy] = false;
				else if(HookState <= HOOK_IDLE)
					m_aMalinkaPendingRehookRelease[Dummy] = false;
			}
		}

		if(g_Config.m_MalinkaGhostMove)
		{
			if(!m_aMalinkaGhostMoveWarned[Dummy])
			{
				m_aMalinkaGhostMoveWarned[Dummy] = true;
				Client()->AddWarning(SWarning("Ghost Move", "Packet-level ghost move is not available in this build yet."));
			}
		}
		else
		{
			m_aMalinkaGhostMoveWarned[Dummy] = false;
		}

		if(g_Config.m_MalinkaAntiAfk && !LeftHeld && !RightHeld && !JumpHeld && !HookHeld && !FireHeld)
		{
			const int64_t Interval = (int64_t)std::max(1, g_Config.m_MalinkaAntiAfkSeconds) * time_freq();
			if(Now > m_aMalinkaInputActivity[Dummy] + Interval && Now > m_aMalinkaLastAntiAfkPulse[Dummy] + Interval)
			{
				m_aMalinkaLastAntiAfkPulse[Dummy] = Now;
				m_aMalinkaAntiAfkPulseEnd[Dummy] = Now + time_freq() / 25;
			}
			if(Now < m_aMalinkaAntiAfkPulseEnd[Dummy])
				m_aInputData[Dummy].m_Direction = 1;
		}

		if(g_Config.m_MalinkaAutoFire && FireHeld)
			GameClient()->m_MalinkaAimbot.PulseFire(m_aInputData[Dummy]);

		if(g_Config.m_MalinkaAutoAled && pLocalCharacter != nullptr)
		{
			const bool ValidAled = GameClient()->m_MalinkaAutoAled.FindAutoAledTarget(pLocalCharacter, RealAimPos);
			const int64_t Cooldown = gs_MalinkaAutoAledCooldownMs * time_freq() / 1000;
			if(ValidAled && !GameClient()->m_MalinkaAutoAled.m_AutoAledLatched[Dummy] && Now > GameClient()->m_MalinkaAutoAled.m_LastAutoAled[Dummy] + Cooldown)
			{
				GameClient()->m_MalinkaAimbot.PulseFire(m_aInputData[Dummy]);
				GameClient()->m_MalinkaAutoAled.m_LastAutoAled[Dummy] = Now;
			}
			GameClient()->m_MalinkaAutoAled.m_AutoAledLatched[Dummy] = ValidAled;
		}

		const bool ActionFrame = m_aInputData[Dummy].m_Hook != 0 || HookState > HOOK_IDLE || m_aInputData[Dummy].m_Fire != m_aLastData[Dummy].m_Fire;
		if(g_Config.m_MalinkaFakeAim)
		{
			const bool RefreshAim = g_Config.m_MalinkaFakeAimMode == 1 ? ActionFrame :
				(g_Config.m_MalinkaFakeAimSendAlways != 0 || ActionFrame || length(m_aMalinkaSentMousePos[Dummy]) < 0.001f);
			if(RefreshAim)
				m_aMalinkaSentMousePos[Dummy] = GameClient()->m_MalinkaAimbot.BuildFakeAimPos(Dummy, RealAimPos, pLocalCharacter, Now);

			SentAimPos = ActionFrame ? EnsureValidAim(RealAimPos) : EnsureValidAim(m_aMalinkaSentMousePos[Dummy]);
			if(g_Config.m_MalinkaFakeAimVisible)
			{
				VisibleAimPos = EnsureValidAim(m_aMalinkaSentMousePos[Dummy]);
				m_aMalinkaRenderMouseOverride[Dummy] = true;
				m_aMalinkaRenderMouseTargetId[Dummy] = -1;
			}
		}

		if(g_Config.m_MalinkaAimbot && pLocalCharacter != nullptr)
		{
			const int ActiveWeapon = pLocalCharacter->GetActiveWeapon();
			const int WeaponSlot =
				ActiveWeapon == WEAPON_HAMMER ? static_cast<int>(CMalinkaAimbot::AimbotSlot::HAMMER) :
				ActiveWeapon == WEAPON_GUN ? static_cast<int>(CMalinkaAimbot::AimbotSlot::GUN) :
				ActiveWeapon == WEAPON_SHOTGUN ? static_cast<int>(CMalinkaAimbot::AimbotSlot::SHOTGUN) :
				ActiveWeapon == WEAPON_GRENADE ? static_cast<int>(CMalinkaAimbot::AimbotSlot::GRENADE) :
				ActiveWeapon == WEAPON_LASER ? static_cast<int>(CMalinkaAimbot::AimbotSlot::LASER) : -999;

			CMalinkaAimbot::SMalinkaAimbotTarget HookTarget;
			const bool WantsAutoHook = g_Config.m_MalinkaAimbotAutoHookKey || g_Config.m_MalinkaAimbotAutoShoot || g_Config.m_MalinkaAimbotAutoShootKey;
			const bool HasHookTarget = GameClient()->m_MalinkaAimbot.GetEnabled(static_cast<int>(CMalinkaAimbot::AimbotSlot::HOOK)) &&
				GameClient()->m_MalinkaAimbot.FindTarget(pLocalCharacter, static_cast<int>(CMalinkaAimbot::AimbotSlot::HOOK), RealAimPos, &HookTarget);
			if(HasHookTarget)
			{
				GameClient()->m_MalinkaAimbot.m_HookTargetId[Dummy] = HookTarget.m_ClientId;
				GameClient()->m_MalinkaAimbot.m_HookAimPos[Dummy] = HookTarget.m_AimPos;
				GameClient()->m_MalinkaAimbot.m_HookWorldPos[Dummy] = HookTarget.m_WorldPos;
				if(HookHeld || WantsAutoHook)
				{
					SentAimPos = HookTarget.m_AimPos;
					if(!GameClient()->m_MalinkaAimbot.GetSilent(static_cast<int>(CMalinkaAimbot::AimbotSlot::HOOK)))
					{
						VisibleAimPos = HookTarget.m_AimPos;
						m_aMalinkaRenderMouseOverride[Dummy] = true;
						m_aMalinkaRenderMouseTargetId[Dummy] = HookTarget.m_ClientId;
					}
					if(WantsAutoHook)
						m_aInputData[Dummy].m_Hook = 1;
				}
			}

			CMalinkaAimbot::SMalinkaAimbotTarget WeaponTarget;
			const bool HasWeaponTarget = WeaponSlot != -999 && GameClient()->m_MalinkaAimbot.GetEnabled(WeaponSlot) &&
						     GameClient()->m_MalinkaAimbot.FindTarget(pLocalCharacter, WeaponSlot, RealAimPos, &WeaponTarget);
			const bool WantsWeaponAim = WeaponSlot != -999 && GameClient()->m_MalinkaAimbot.GetEnabled(WeaponSlot) &&
				(FireHeld || g_Config.m_MalinkaAimbotAutoShoot || g_Config.m_MalinkaAimbotAutoShootKey);
			if(HasWeaponTarget)
			{
				GameClient()->m_MalinkaAimbot.m_WeaponTargetId[Dummy] = WeaponTarget.m_ClientId;
				GameClient()->m_MalinkaAimbot.m_WeaponAimPos[Dummy] = WeaponTarget.m_AimPos;
				GameClient()->m_MalinkaAimbot.m_WeaponWorldPos[Dummy] = WeaponTarget.m_WorldPos;
			}
			if(WantsWeaponAim && HasWeaponTarget)
			{
				SentAimPos = WeaponTarget.m_AimPos;
				if(!GameClient()->m_MalinkaAimbot.GetSilent(WeaponSlot))
				{
					VisibleAimPos = WeaponTarget.m_AimPos;
					m_aMalinkaRenderMouseOverride[Dummy] = true;
					m_aMalinkaRenderMouseTargetId[Dummy] = WeaponTarget.m_ClientId;
				}

				if(g_Config.m_MalinkaAimbotAutoShoot || g_Config.m_MalinkaAimbotAutoShootKey)
				{
					const bool TargetChanged = GameClient()->m_MalinkaAimbot.m_ScheduledTargetId[Dummy] != WeaponTarget.m_ClientId;
					if(TargetChanged || GameClient()->m_MalinkaAimbot.m_NextAutoShoot[Dummy] == 0)
					{
						const int MinDelay = std::min(g_Config.m_MalinkaAimbotMinShootDelay, g_Config.m_MalinkaAimbotMaxShootDelay);
						const int MaxDelay = std::max(g_Config.m_MalinkaAimbotMinShootDelay, g_Config.m_MalinkaAimbotMaxShootDelay);
						const int DelayTicks = MinDelay + (MaxDelay > MinDelay ? rand() % (MaxDelay - MinDelay + 1) : 0);
						GameClient()->m_MalinkaAimbot.m_NextAutoShoot[Dummy] = Now + DelayTicks * time_freq() / Client()->GameTickSpeed();
						GameClient()->m_MalinkaAimbot.m_ScheduledTargetId[Dummy] = WeaponTarget.m_ClientId;
					}

					if(GameClient()->m_MalinkaLaserUnfreeze.IsWeaponReadyForSelfHit(pLocalCharacter, ActiveWeapon) &&
						Now >= GameClient()->m_MalinkaAimbot.m_NextAutoShoot[Dummy] &&
						!GameClient()->m_MalinkaLaserUnfreeze.m_LastSelfHitFire[Dummy])
					{
						TriggerFireTap(Dummy);
						GameClient()->m_MalinkaAimbot.m_NextAutoShoot[Dummy] = 0;
					}
				}
				else
				{
					GameClient()->m_MalinkaAimbot.m_NextAutoShoot[Dummy] = 0;
					GameClient()->m_MalinkaAimbot.m_ScheduledTargetId[Dummy] = -1;
				}
			}
			else
			{
				GameClient()->m_MalinkaAimbot.m_NextAutoShoot[Dummy] = 0;
				GameClient()->m_MalinkaAimbot.m_ScheduledTargetId[Dummy] = -1;
			}
		}

		const bool NeedUnfreeze = pLocalCharacter != nullptr &&
			(pLocalCharacter->m_FreezeTime > 0 || GameClient()->m_MalinkaAvoidFreeze.HasImpendingFreeze(pLocalCharacter, std::max(2, g_Config.m_MalinkaUnfreezeBotCurDirTicks)));
		if(NeedUnfreeze)
		{
			vec2 SilentAimPos = SentAimPos;
			float TimingError = 999.0f;
			const int64_t SelfHitCooldown = gs_MalinkaSelfHitCooldownMs * time_freq() / 1000;
			const int LaserLookTicks = std::max(std::max(2, g_Config.m_MalinkaUnfreezeBotTicks), pLocalCharacter != nullptr ? pLocalCharacter->m_FreezeTime + 3 : 3);
			if(g_Config.m_MalinkaUnfreezeBot && GameClient()->m_MalinkaLaserUnfreeze.FindBestSelfHitAim(pLocalCharacter, RealAimPos, WEAPON_LASER, LaserLookTicks, &SilentAimPos, &TimingError))
			{
				m_aMalinkaAssistAimActive[Dummy] = true;
				m_aMalinkaAssistAimWeapon[Dummy] = WEAPON_LASER;
				m_aMalinkaAssistAimPos[Dummy] = SilentAimPos;
				m_aInputData[Dummy].m_WantedWeapon = WEAPON_LASER + 1;
				if(pLocalCharacter->GetActiveWeapon() == WEAPON_LASER && GameClient()->m_MalinkaLaserUnfreeze.IsWeaponReadyForSelfHit(pLocalCharacter, WEAPON_LASER) &&
					!GameClient()->m_MalinkaLaserUnfreeze.m_PendingSelfHitFireRelease[Dummy] &&
					Now > GameClient()->m_MalinkaLaserUnfreeze.m_LastSelfHitFire[Dummy] + SelfHitCooldown)
				{
					SentAimPos = SilentAimPos;
					if(!g_Config.m_MalinkaUnfreezeBotSilent)
						VisibleAimPos = SilentAimPos;
					TriggerFireTap(Dummy);
					GameClient()->m_MalinkaLaserUnfreeze.m_LastSelfHitFire[Dummy] = Now;
				}
			}
		}

		if(g_Config.m_MalinkaAutoShotgun && pLocalCharacter != nullptr)
		{
			vec2 SilentAimPos = SentAimPos;
			const int64_t SelfHitCooldown = gs_MalinkaSelfHitCooldownMs * time_freq() / 1000;
			if(GameClient()->m_MalinkaLaserUnfreeze.FindBestSelfHitAim(pLocalCharacter, RealAimPos, WEAPON_SHOTGUN, std::max(2, g_Config.m_MalinkaAutoShotgunTicks), &SilentAimPos, nullptr))
			{
				m_aMalinkaAssistAimActive[Dummy] = true;
				m_aMalinkaAssistAimWeapon[Dummy] = WEAPON_SHOTGUN;
				m_aMalinkaAssistAimPos[Dummy] = SilentAimPos;
				m_aInputData[Dummy].m_WantedWeapon = WEAPON_SHOTGUN + 1;
				if(pLocalCharacter->GetActiveWeapon() == WEAPON_SHOTGUN && GameClient()->m_MalinkaLaserUnfreeze.IsWeaponReadyForSelfHit(pLocalCharacter, WEAPON_SHOTGUN) &&
					!GameClient()->m_MalinkaLaserUnfreeze.m_PendingSelfHitFireRelease[Dummy] && Now > GameClient()->m_MalinkaLaserUnfreeze.m_LastSelfHitFire[Dummy] + SelfHitCooldown)
				{
					SentAimPos = SilentAimPos;
					if(!g_Config.m_MalinkaAutoShotgunSilent)
						VisibleAimPos = SilentAimPos;
					TriggerFireTap(Dummy);
					GameClient()->m_MalinkaLaserUnfreeze.m_LastSelfHitFire[Dummy] = Now;
				}
			}
		}

		if(g_Config.m_MalinkaGhostMove && (g_Config.m_MalinkaGhostMoveHook || g_Config.m_MalinkaGhostMoveHookClosest))
		{
			// Same as above: don't inject fake hook packets until a real packet-level implementation exists.
		}

		GameClient()->m_MalinkaAutoEdge.UpdateAutoEdgeState(Dummy, pLocalCharacter);
		if(g_Config.m_MalinkaAutoEdge && pLocalCharacter != nullptr && GameClient()->m_MalinkaAutoEdge.m_Locked[Dummy] && pLocalCharacter->Core()->m_Vel.y >= -0.5f)
		{
			const float Delta = GameClient()->m_MalinkaAutoEdge.m_LockedPos[Dummy].x - pLocalCharacter->GetPos().x;
			const float VelX = pLocalCharacter->Core()->m_Vel.x;
			const bool LeavingLeft = Delta < 0.0f && (m_aInputData[Dummy].m_Direction < 0 || VelX < -0.35f);
			const bool LeavingRight = Delta > 0.0f && (m_aInputData[Dummy].m_Direction > 0 || VelX > 0.35f);
			if(absolute(Delta) < 14.0f && (LeavingLeft || LeavingRight))
				m_aInputData[Dummy].m_Direction = Delta > 0.0f ? -1 : 1;
			else if(absolute(Delta) < 8.0f && absolute(VelX) > 0.08f)
				m_aInputData[Dummy].m_Direction = VelX > 0.0f ? -1 : 1;
			else if(absolute(Delta) < 3.0f)
				m_aInputData[Dummy].m_Direction = 0;
		}


		m_aMalinkaInjectedHook[Dummy] = m_aInputData[Dummy].m_Hook != BaseHook;
		m_aMalinkaInjectedJump[Dummy] = m_aInputData[Dummy].m_Jump != BaseJump;
		SentAimPos = EnsureValidAim(SentAimPos);
		m_aMalinkaVisibleMousePos[Dummy] = EnsureValidAim(VisibleAimPos);
		m_aInputData[g_Config.m_ClDummy].m_TargetX = (int)SentAimPos.x;
		m_aInputData[g_Config.m_ClDummy].m_TargetY = (int)SentAimPos.y;
		GameClient()->m_MalinkaTas.ObserveInput(m_aInputData[Dummy], pLocalCharacter != nullptr ? pLocalCharacter->GetPos() : GameClient()->m_LocalCharacterPos);

		m_aMalinkaLastHookState[Dummy] = HookState;

}

vec2 CControls::GetRenderMousePos(int Dummy)
{
	if(Dummy < 0 || Dummy >= NUM_DUMMIES)
		return vec2(0.0f, 0.0f);
	if(Dummy == g_Config.m_ClDummy && GameClient()->m_MalinkaTas.HasRenderMouseOverride())
		return EnsureValidAim(GameClient()->m_MalinkaTas.GetRenderMousePos());
	if(m_aMalinkaRenderMouseOverride[Dummy])
	{
		const int TargetId = m_aMalinkaRenderMouseTargetId[Dummy];
		if(TargetId >= 0 && GameClient()->m_Snap.m_LocalClientId >= 0 &&
			TargetId < MAX_CLIENTS &&
			GameClient()->m_Snap.m_aCharacters[TargetId].m_Active)
		{
			const vec2 LocalRenderPos = GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_RenderPos;
			const vec2 TargetRenderPos = GameClient()->m_aClients[TargetId].m_RenderPos;
			return EnsureValidAim(TargetRenderPos - LocalRenderPos);
		}
		return EnsureValidAim(m_aMalinkaVisibleMousePos[Dummy]);
	}
	return EnsureValidAim(m_aMousePos[Dummy]);
}