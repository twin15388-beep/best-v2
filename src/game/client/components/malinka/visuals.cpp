// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#include "simulation_step.h"
#include "visuals.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/dbg.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>
#include <base/windows.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/projectile_data.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/ui_rect.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <limits>
#include <string>
#include <vector>


namespace
{
ColorRGBA EvaluateMalinkaTextColor(unsigned StartColor, unsigned EndColor, bool Rainbow, float Speed, float Position)
{
	if(Rainbow)
	{
		const float Hue = std::fmod((float)time_get() / (float)time_freq() * Speed * 0.1f + Position, 1.0f);
		return color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
	}

	const ColorRGBA Start = color_cast<ColorRGBA>(ColorHSLA(StartColor));
	const ColorRGBA End = color_cast<ColorRGBA>(ColorHSLA(EndColor));
	return ColorRGBA(
		mix(Start.r, End.r, Position),
		mix(Start.g, End.g, Position),
		mix(Start.b, End.b, Position),
		mix(Start.a, End.a, Position));
}

void RenderMalinkaColoredText(ITextRender *pTextRender, float X, float Y, float FontSize, const char *pText, unsigned StartColor, unsigned EndColor, bool Rainbow, bool Segmented, float Speed)
{
	if(!Segmented)
	{
		pTextRender->TextColor(EvaluateMalinkaTextColor(StartColor, EndColor, Rainbow, Speed, 0.0f));
		pTextRender->Text(X, Y, FontSize, pText);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return;
	}

	float CursorX = X;
	int Cursor = 0;
	int GlyphCount = 0;
	while(pText[Cursor] != '\0')
	{
		const int NextCursor = str_utf8_forward(pText, Cursor);
		if(NextCursor <= Cursor)
			break;
		++GlyphCount;
		Cursor = NextCursor;
	}

	char aGlyph[16] = {'\0'};
	Cursor = 0;
	int GlyphIndex = 0;
	while(pText[Cursor] != '\0')
	{
		const int NextCursor = str_utf8_forward(pText, Cursor);
		if(NextCursor <= Cursor)
			break;

		const int GlyphBytes = std::min<int>(NextCursor - Cursor, sizeof(aGlyph) - 1);
		mem_copy(aGlyph, pText + Cursor, GlyphBytes);
		aGlyph[GlyphBytes] = '\0';

		const float Position = GlyphCount > 1 ? GlyphIndex / (float)(GlyphCount - 1) : 0.0f;
		pTextRender->TextColor(EvaluateMalinkaTextColor(StartColor, EndColor, Rainbow, Speed, Position));
		pTextRender->Text(CursorX, Y, FontSize, aGlyph);
		CursorX += pTextRender->TextWidth(FontSize, aGlyph);

		Cursor = NextCursor;
		++GlyphIndex;
	}
	pTextRender->TextColor(pTextRender->DefaultTextColor());
}

ColorRGBA PackedToColor(unsigned Packed)
{
	return color_cast<ColorRGBA>(ColorHSLA(Packed));
}

ColorRGBA MixColor(const ColorRGBA &From, const ColorRGBA &To, float Amount, float Alpha)
{
	return ColorRGBA(
		mix(From.r, To.r, Amount),
		mix(From.g, To.g, Amount),
		mix(From.b, To.b, Amount),
		Alpha);
}

void AddLineSegment(std::vector<IGraphics::CLineItem> &vSegments, vec2 From, vec2 To)
{
	if(distance(From, To) > 0.01f)
		vSegments.emplace_back(From.x, From.y, To.x, To.y);
}

struct SScreenPixelGrid
{
	float m_ScreenX0;
	float m_ScreenY0;
	float m_PixelSizeX;
	float m_PixelSizeY;
};

SScreenPixelGrid GetScreenPixelGrid(const IGraphics *pGraphics)
{
	const CScreenRect Screen = pGraphics->GetScreen();
	const float ScreenX0 = Screen.m_TopLeft.x;
	const float ScreenY0 = Screen.m_TopLeft.y;
	const float ScreenX1 = Screen.m_BottomRight.x;
	const float ScreenY1 = Screen.m_BottomRight.y;

	return {
		ScreenX0,
		ScreenY0,
		(ScreenX1 - ScreenX0) / std::max(1, pGraphics->ScreenWidth()),
		(ScreenY1 - ScreenY0) / std::max(1, pGraphics->ScreenHeight())};
}

vec2 SnapToScreenPixel(const SScreenPixelGrid &Grid, vec2 Pos)
{
	return vec2(
		Grid.m_ScreenX0 + roundf((Pos.x - Grid.m_ScreenX0) / Grid.m_PixelSizeX) * Grid.m_PixelSizeX,
		Grid.m_ScreenY0 + roundf((Pos.y - Grid.m_ScreenY0) / Grid.m_PixelSizeY) * Grid.m_PixelSizeY);
}

float SnapToPixelSpan(float Span, float PixelSize)
{
	return std::max(PixelSize, roundf(Span / PixelSize) * PixelSize);
}

void RenderLineSegments(IGraphics *pGraphics, const std::vector<IGraphics::CLineItem> &vSegments, ColorRGBA Color, float Thickness, float Zoom)
{
	if(vSegments.empty() || Color.a <= 0.0f)
		return;

	const SScreenPixelGrid Grid = GetScreenPixelGrid(pGraphics);
	const float PixelSize = std::max(Grid.m_PixelSizeX, Grid.m_PixelSizeY);
	const float LineWidth = SnapToPixelSpan(std::max(PixelSize, (0.8f + (Thickness - 1.0f) * 0.5f) * Zoom), PixelSize);

	pGraphics->TextureClear();
	std::vector<IGraphics::CFreeformItem> vLineQuadSegments;
	vLineQuadSegments.reserve(vSegments.size());

	for(const auto &LineSegment : vSegments)
	{
		const vec2 InitPos = SnapToScreenPixel(Grid, vec2(LineSegment.m_X0, LineSegment.m_Y0));
		const vec2 FinishPos = SnapToScreenPixel(Grid, vec2(LineSegment.m_X1, LineSegment.m_Y1));
		const vec2 Diff = FinishPos - InitPos;
		if(length(Diff) < 0.001f)
			continue;

		const vec2 Perp = normalize(vec2(Diff.y, -Diff.x)) * (LineWidth * 0.5f);
		const vec2 Pos0 = FinishPos - Perp;
		const vec2 Pos1 = FinishPos + Perp;
		const vec2 Pos2 = InitPos - Perp;
		const vec2 Pos3 = InitPos + Perp;
		vLineQuadSegments.emplace_back(Pos0.x, Pos0.y, Pos1.x, Pos1.y, Pos2.x, Pos2.y, Pos3.x, Pos3.y);
	}

	if(vLineQuadSegments.empty())
		return;

	pGraphics->QuadsBegin();
	pGraphics->SetColor(Color);
	pGraphics->QuadsDrawFreeform(vLineQuadSegments.data(), vLineQuadSegments.size());
	pGraphics->QuadsEnd();
}

void RenderPointMarkers(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, const std::vector<vec2> &vPoints, ColorRGBA Color, float Size)
{
	if(vPoints.empty() || Color.a <= 0.0f || Size <= 0.0f)
		return;

	const SScreenPixelGrid Grid = GetScreenPixelGrid(pGraphics);
	const float SizeX = SnapToPixelSpan(Size, Grid.m_PixelSizeX);
	const float SizeY = SnapToPixelSpan(Size, Grid.m_PixelSizeY);

	if(Texture.IsValid())
		pGraphics->TextureSet(Texture);
	else
		pGraphics->TextureClear();

	pGraphics->QuadsBegin();
	pGraphics->SetColor(Color);
	for(const vec2 &Point : vPoints)
	{
		const vec2 TopLeft = SnapToScreenPixel(Grid, Point - vec2(SizeX * 0.5f, SizeY * 0.5f));
		const IGraphics::CQuadItem Quad(TopLeft.x, TopLeft.y, SizeX, SizeY);
		pGraphics->QuadsDrawTL(&Quad, 1);
	}
	pGraphics->QuadsEnd();
}

void AddProjectilePath(CCollision *pCollision, std::vector<IGraphics::CLineItem> &vSegments, vec2 StartPos, vec2 StartVel, float Curvature, float Speed, float MaxTime, int NumSteps, bool Dotted)
{
	if(length(StartVel) < 0.001f || MaxTime <= 0.0f || NumSteps <= 0)
		return;

	vec2 PrevPos = StartPos;
	for(int Step = 1; Step <= NumSteps; ++Step)
	{
		const float T = MaxTime * Step / (float)NumSteps;
		const vec2 Pos = CalcPos(StartPos, StartVel, Curvature, Speed, T);

		vec2 ColPos;
		vec2 NewPos;
		if(pCollision->IntersectLine(PrevPos, Pos, &ColPos, &NewPos))
		{
			if(!Dotted || Step % 2 == 0)
				AddLineSegment(vSegments, PrevPos, ColPos);
			break;
		}

		if(!Dotted || Step % 2 == 0)
			AddLineSegment(vSegments, PrevPos, Pos);
		PrevPos = Pos;
	}
}

void AddLaserPath(CCollision *pCollision, std::vector<IGraphics::CLineItem> &vSegments, vec2 StartPos, vec2 Direction, float Reach, const CTuningParams *pTuning)
{
	if(length(Direction) < 0.001f || Reach <= 0.0f || pTuning == nullptr)
		return;

	vec2 Pos = StartPos;
	vec2 Dir = normalize(Direction);
	float Energy = Reach;
	int Bounces = 0;
	bool ZeroEnergyBounceInLastTick = false;

	while(Energy > 0.0f)
	{
		vec2 Coltile;
		vec2 To = Pos + Dir * Energy;
		vec2 BeforeCollision = To;
		const int Res = pCollision->IntersectLineTeleWeapon(Pos, To, &Coltile, &BeforeCollision);
		if(Res == 0)
		{
			AddLineSegment(vSegments, Pos, To);
			break;
		}

		AddLineSegment(vSegments, Pos, BeforeCollision);

		vec2 TempPos = BeforeCollision;
		vec2 TempDir = Dir * 4.0f;
		int OriginalTile = 0;
		if(Res == -1)
		{
			OriginalTile = pCollision->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
			pCollision->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), TILE_SOLID);
		}
		pCollision->MovePoint(&TempPos, &TempDir, 1.0f, nullptr);
		if(Res == -1)
			pCollision->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), OriginalTile);

		const float Distance = distance(Pos, TempPos);
		if(Distance == 0.0f && ZeroEnergyBounceInLastTick)
			break;

		Energy -= Distance + pTuning->m_LaserBounceCost;
		ZeroEnergyBounceInLastTick = Distance == 0.0f;
		Pos = TempPos;
		if(length(TempDir) < 0.001f)
			break;
		Dir = normalize(TempDir);

		++Bounces;
		if(Bounces > pTuning->m_LaserBounceNum)
			break;
	}
}

vec2 GetLocalAimDirection(CGameClient *pGameClient)
{
	vec2 Direction = pGameClient->m_Controls.GetRenderMousePos(g_Config.m_ClDummy);
	if(g_Config.m_TcScaleMouseDistance)
	{
		const int MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
		if(MaxDistance > 5 && MaxDistance < 1000)
			Direction *= 1000.0f / (float)MaxDistance;
	}

	return length(Direction) > 0.001f ? normalize(Direction) : vec2(0.0f, -1.0f);
}

vec2 GetCharacterAimDirection(CGameClient *pGameClient, int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return vec2(0.0f, 0.0f);

	const auto &CharInfo = pGameClient->m_Snap.m_aCharacters[ClientId];
	if(CharInfo.m_HasExtendedDisplayInfo)
	{
		const vec2 Direction(CharInfo.m_ExtendedData.m_TargetX, CharInfo.m_ExtendedData.m_TargetY);
		return length(Direction) > 0.001f ? normalize(Direction) : vec2(0.0f, 0.0f);
	}

	const float Angle = pGameClient->m_aClients[ClientId].m_RenderCur.m_Angle / 256.0f;
	return direction(Angle);
}

bool IsFrozenCharacter(const CCharacter *pCharacter)
{
	return pCharacter->m_FreezeTime > 0 || pCharacter->Core()->m_DeepFrozen || pCharacter->Core()->m_LiveFrozen;
}

float GetMalinkaOverlayFovForSlot(int Slot)
{
	switch(Slot)
	{
	case -1: return (float)g_Config.m_MalinkaAimbotHookFov;
	case WEAPON_HAMMER: return (float)g_Config.m_MalinkaAimbotHammerFov;
	case WEAPON_GUN: return (float)g_Config.m_MalinkaAimbotPistolFov;
	case WEAPON_SHOTGUN: return (float)g_Config.m_MalinkaAimbotShotgunFov;
	case WEAPON_GRENADE: return (float)g_Config.m_MalinkaAimbotGrenadeFov;
	case WEAPON_LASER: return (float)g_Config.m_MalinkaAimbotLaserFov;
	default: return 90.0f;
	}
}

ColorRGBA GetMalinkaOverlayColorForSlot(int Slot)
{
	switch(Slot)
	{
	case -1: return ColorRGBA(1.0f, 0.28f, 0.25f, 0.82f);
	case WEAPON_HAMMER: return ColorRGBA(1.0f, 0.88f, 0.35f, 0.78f);
	case WEAPON_GUN: return ColorRGBA(0.92f, 0.92f, 0.92f, 0.75f);
	case WEAPON_SHOTGUN: return ColorRGBA(1.0f, 0.66f, 0.24f, 0.78f);
	case WEAPON_GRENADE: return ColorRGBA(1.0f, 0.42f, 0.30f, 0.8f);
	case WEAPON_LASER: return ColorRGBA(0.40f, 0.92f, 1.0f, 0.82f);
	default: return ColorRGBA(1.0f, 1.0f, 1.0f, 0.65f);
	}
}

void AddFovBoundary(std::vector<IGraphics::CLineItem> &vSegments, vec2 LocalPos, vec2 MouseAim, float FovDegrees, float Length)
{
	const float Fov = FovDegrees * pi / 180.0f;
	const vec2 LeftDir = direction(angle(MouseAim) - Fov * 0.5f);
	const vec2 RightDir = direction(angle(MouseAim) + Fov * 0.5f);
	AddLineSegment(vSegments, LocalPos, LocalPos + LeftDir * Length);
	AddLineSegment(vSegments, LocalPos, LocalPos + RightDir * Length);
}
}

void CMalinkaVisuals::PushNotification(const SWarning &Warning)
{
	SNotification Notification;
	str_copy(Notification.m_aTitle, Warning.m_aWarningTitle[0] ? Warning.m_aWarningTitle : Localize("Warning"));
	str_copy(Notification.m_aMessage, Warning.m_aWarningMsg);
	Notification.m_StartTime = Client()->LocalTime();
	Notification.m_Duration = Warning.m_AutoHide ? 4.0 : 7.0;
	m_vNotifications.push_back(Notification);

	while(m_vNotifications.size() > 5)
		m_vNotifications.pop_front();
}

void CMalinkaVisuals::UpdateMenuTheme()
{
	if(g_Config.m_MalinkaCustomTheme)
	{
		if(!m_MalinkaThemeActive)
		{
			m_StoredUiColor = g_Config.m_UiColor;
			m_MalinkaThemeActive = true;
		}

		if(g_Config.m_MalinkaRainbowMenu)
		{
			const ColorRGBA Col = EvaluateMalinkaTextColor(g_Config.m_MalinkaCustomAccentColor, g_Config.m_MalinkaCustomAccentColor, true, g_Config.m_MalinkaCustomColorRainbowSpeed, 0.0f);
			g_Config.m_UiColor = color_cast<ColorHSLA>(Col).Pack(false);
		}
		else
			g_Config.m_UiColor = g_Config.m_MalinkaCustomAccentColor;
	}
	else if(m_MalinkaThemeActive)
	{
		g_Config.m_UiColor = m_StoredUiColor;
		m_MalinkaThemeActive = false;
	}
}

void CMalinkaVisuals::RenderAimLines()
{
	if(!g_Config.m_MalinkaAimLines)
		return;

	const float Alpha = g_Config.m_MalinkaAimLinesAlpha / 100.0f;
	if(Alpha <= 0.0f)
		return;

	const CTuningParams *pTuning = GameClient()->GetTuning(GameClient()->m_aLocalTuneZone[g_Config.m_ClDummy]);
	const bool IsDdrace = GameClient()->m_GameWorld.m_WorldConfig.m_IsDDRace;

	std::vector<IGraphics::CLineItem> vGunSegments;
	std::vector<IGraphics::CLineItem> vShotgunSegments;
	std::vector<IGraphics::CLineItem> vGrenadeSegments;
	std::vector<IGraphics::CLineItem> vLaserSegments;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;

		const bool Local = ClientId == GameClient()->m_Snap.m_LocalClientId;
		if(g_Config.m_MalinkaAimLinesSelfOnly && !Local)
			continue;

		vec2 Direction = Local && !GameClient()->m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK ?
			GetLocalAimDirection(GameClient()) :
			GetCharacterAimDirection(GameClient(), ClientId);
		if(length(Direction) < 0.001f)
			continue;

		const int Weapon = GameClient()->m_aClients[ClientId].m_RenderCur.m_Weapon;
		const vec2 StartPos = GameClient()->m_aClients[ClientId].m_RenderPos + Direction * 21.0f;

		if(Weapon == WEAPON_GUN && g_Config.m_MalinkaAimLinesGun)
		{
			AddProjectilePath(Collision(), vGunSegments, StartPos, Direction, pTuning->m_GunCurvature, pTuning->m_GunSpeed, pTuning->m_GunLifetime, 28, false);
		}
		else if(Weapon == WEAPON_SHOTGUN && g_Config.m_MalinkaAimLinesShotgun)
		{
			if(IsDdrace)
			{
				AddLaserPath(Collision(), vShotgunSegments, GameClient()->m_aClients[ClientId].m_RenderPos, Direction, pTuning->m_LaserReach, pTuning);
			}
			else
			{
				static const float s_aSpreading[] = {-0.185f, -0.070f, 0.0f, 0.070f, 0.185f};
				for(int i = -2; i <= 2; ++i)
				{
					const float SpreadAngle = angle(Direction) + s_aSpreading[i + 2];
					const float V = 1.0f - (absolute(i) / 2.0f);
					const float SpeedFactor = mix((float)pTuning->m_ShotgunSpeeddiff, 1.0f, V);
					AddProjectilePath(Collision(), vShotgunSegments, StartPos, direction(SpreadAngle) * SpeedFactor, pTuning->m_ShotgunCurvature, pTuning->m_ShotgunSpeed, pTuning->m_ShotgunLifetime, 20, false);
				}
			}
		}
		else if(Weapon == WEAPON_GRENADE && g_Config.m_MalinkaAimLinesGrenade)
		{
			AddProjectilePath(Collision(), vGrenadeSegments, StartPos, Direction, pTuning->m_GrenadeCurvature, pTuning->m_GrenadeSpeed, pTuning->m_GrenadeLifetime, 36, false);
		}
		else if(Weapon == WEAPON_LASER && g_Config.m_MalinkaAimLinesLaser)
		{
			AddLaserPath(Collision(), vLaserSegments, GameClient()->m_aClients[ClientId].m_RenderPos, Direction, pTuning->m_LaserReach, pTuning);
		}
	}

	RenderLineSegments(Graphics(), vGunSegments, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), 1.0f, GameClient()->m_Camera.m_Zoom);
	RenderLineSegments(Graphics(), vShotgunSegments, ColorRGBA(1.0f, 0.75f, 0.25f, Alpha), 1.0f, GameClient()->m_Camera.m_Zoom);
	RenderLineSegments(Graphics(), vGrenadeSegments, ColorRGBA(1.0f, 0.4f, 0.3f, Alpha), 1.0f, GameClient()->m_Camera.m_Zoom);
	RenderLineSegments(Graphics(), vLaserSegments, ColorRGBA(0.45f, 0.95f, 1.0f, Alpha), 1.0f, GameClient()->m_Camera.m_Zoom);
}

void CMalinkaVisuals::RenderAimbotOverlay()
{
	if(!g_Config.m_MalinkaAimbot)
		return;

	const int Dummy = g_Config.m_ClDummy;
	if(GameClient()->m_Snap.m_LocalClientId < 0 || !GameClient()->m_Snap.m_aCharacters[GameClient()->m_Snap.m_LocalClientId].m_Active)
		return;
	CCharacter *pLocalCharacter = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(pLocalCharacter == nullptr)
		return;

	std::vector<IGraphics::CLineItem> vSegments;
	std::vector<IGraphics::CLineItem> vHookFovSegments;
	std::vector<IGraphics::CLineItem> vWeaponFovSegments;
	const vec2 LocalPos = GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_RenderPos;
	const vec2 RawAim = GameClient()->m_Controls.m_aMousePos[Dummy];
	const vec2 MouseAim = normalize(length(RawAim) > 0.001f ? RawAim : vec2(1.0f, 0.0f));

	if(g_Config.m_MalinkaAimbotDrawFov)
	{
		const int Weapon = pLocalCharacter->GetActiveWeapon();
		const float FovLength = 190.0f;
		const bool ShowHookFov = g_Config.m_MalinkaAimbotHook;
		if(ShowHookFov)
			AddFovBoundary(vHookFovSegments, LocalPos, MouseAim, GetMalinkaOverlayFovForSlot(-1), FovLength);
		AddFovBoundary(vWeaponFovSegments, LocalPos, MouseAim, GetMalinkaOverlayFovForSlot(Weapon), FovLength);
	}

	if(g_Config.m_MalinkaAimbotTargetBox)
	{
		auto AddTargetBox = [&](int ClientId, ColorRGBA Color) {
			if(ClientId < 0 || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
				return;
			const vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
			const float HalfW = 18.0f;
			const float HalfH = 26.0f;
			AddLineSegment(vSegments, Pos + vec2(-HalfW, -HalfH), Pos + vec2(HalfW, -HalfH));
			AddLineSegment(vSegments, Pos + vec2(HalfW, -HalfH), Pos + vec2(HalfW, HalfH));
			AddLineSegment(vSegments, Pos + vec2(HalfW, HalfH), Pos + vec2(-HalfW, HalfH));
			AddLineSegment(vSegments, Pos + vec2(-HalfW, HalfH), Pos + vec2(-HalfW, -HalfH));
			RenderLineSegments(Graphics(), vSegments, Color, 2.0f, GameClient()->m_Camera.m_Zoom);
			vSegments.clear();
		};

		AddTargetBox(GameClient()->m_MalinkaAimbot.m_HookTargetId[Dummy], ColorRGBA(1.0f, 0.25f, 0.2f, 0.95f));
		AddTargetBox(GameClient()->m_MalinkaAimbot.m_WeaponTargetId[Dummy], ColorRGBA(1.0f, 0.65f, 0.15f, 0.95f));
	}

	RenderLineSegments(Graphics(), vHookFovSegments, GetMalinkaOverlayColorForSlot(-1), 1.35f, GameClient()->m_Camera.m_Zoom);
	RenderLineSegments(Graphics(), vWeaponFovSegments, GetMalinkaOverlayColorForSlot(pLocalCharacter->GetActiveWeapon()), 1.35f, GameClient()->m_Camera.m_Zoom);
	RenderLineSegments(Graphics(), vSegments, ColorRGBA(1.0f, 1.0f, 1.0f, 0.55f), 1.2f, GameClient()->m_Camera.m_Zoom);
}

void CMalinkaVisuals::RenderTrajectory()
{
	if(!g_Config.m_MalinkaTrajectoryEsp)
		return;
	if(GameClient()->m_MalinkaTas.IsPlaying())
		return;

	CGameWorld FutureWorld;
	if(!FutureWorld.CopyWorldIndependent(GameClient()->Predict() ? &GameClient()->m_PredictedWorld : &GameClient()->m_GameWorld))
		return;

	struct STrajectoryState
	{
		int m_ClientId;
		vec2 m_PreviousPos;
		vec2 m_Offset;
		bool m_Local;
		bool m_Active;
	};

	std::vector<STrajectoryState> vStates;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;

		const bool Local = ClientId == GameClient()->m_Snap.m_LocalClientId;
		if(g_Config.m_MalinkaTrajectoryEspSelfOnly && !Local)
			continue;

		CCharacter *pCharacter = FutureWorld.GetCharacterById(ClientId);
		if(!pCharacter)
			continue;

		const vec2 StartPos = GameClient()->m_aClients[ClientId].m_RenderPos;
		vStates.push_back({ClientId, StartPos, StartPos - pCharacter->GetPos(), Local, true});
	}

	if(vStates.empty())
		return;

	std::array<std::vector<IGraphics::CLineItem>, 3> aTrajectoryBuckets;
	std::array<std::vector<vec2>, 3> aTrajectoryDots;
	const bool Dotted = g_Config.m_MalinkaTrajectoryEspMode == 1;

	for(int Tick = 0; Tick < g_Config.m_MalinkaTrajectoryEspTicks; ++Tick)
	{
		if(!Malinka::TickSimulation(FutureWorld, {}))
			return;
		for(auto &State : vStates)
		{
			if(!State.m_Active)
				continue;

			CCharacter *pCharacter = FutureWorld.GetCharacterById(State.m_ClientId);
			if(!pCharacter)
			{
				State.m_Active = false;
				continue;
			}

			const vec2 CurrentPos = pCharacter->GetPos() + State.m_Offset;
			const int Bucket = IsFrozenCharacter(pCharacter) ? 1 : (State.m_Local ? 0 : 2);
			if(Dotted)
			{
				if(Tick % 2 == 0)
					aTrajectoryDots[Bucket].push_back(CurrentPos);
			}
			else
			{
				AddLineSegment(aTrajectoryBuckets[Bucket], State.m_PreviousPos, CurrentPos);
			}
			State.m_PreviousPos = CurrentPos;
		}
	}

	const float Thickness = (float)g_Config.m_MalinkaTrajectoryEspThickness;
	if(Dotted)
	{
		const float DotSize = std::max(3.0f, Thickness * 2.5f);
		const auto DotTexture = GameClient()->m_ParticlesSkin.m_aSpriteParticles[SPRITE_PART_BALL - SPRITE_PART_SLICE];
		RenderPointMarkers(Graphics(), DotTexture, aTrajectoryDots[0], PackedToColor(g_Config.m_MalinkaTrajectoryEspColorLocal), DotSize);
		RenderPointMarkers(Graphics(), DotTexture, aTrajectoryDots[1], PackedToColor(g_Config.m_MalinkaTrajectoryEspColorFrozen), DotSize);
		RenderPointMarkers(Graphics(), DotTexture, aTrajectoryDots[2], PackedToColor(g_Config.m_MalinkaTrajectoryEspColorOthers), DotSize);
	}
	else
	{
		RenderLineSegments(Graphics(), aTrajectoryBuckets[0], PackedToColor(g_Config.m_MalinkaTrajectoryEspColorLocal), Thickness, GameClient()->m_Camera.m_Zoom);
		RenderLineSegments(Graphics(), aTrajectoryBuckets[1], PackedToColor(g_Config.m_MalinkaTrajectoryEspColorFrozen), Thickness, GameClient()->m_Camera.m_Zoom);
		RenderLineSegments(Graphics(), aTrajectoryBuckets[2], PackedToColor(g_Config.m_MalinkaTrajectoryEspColorOthers), Thickness, GameClient()->m_Camera.m_Zoom);
	}
}

void CMalinkaVisuals::RenderAutoEdge()
{
	if(!g_Config.m_MalinkaAutoEdge || (!g_Config.m_MalinkaAutoEdgeShowFound && !g_Config.m_MalinkaAutoEdgeShowLocked))
		return;

	const int Dummy = g_Config.m_ClDummy;
	std::vector<IGraphics::CLineItem> vFoundSegments;
	std::vector<IGraphics::CLineItem> vLockedSegments;
	if(g_Config.m_MalinkaAutoEdgeShowFound)
	{
		for(int Index = 0; Index < GameClient()->m_MalinkaAutoEdge.m_FoundCount[Dummy]; ++Index)
		{
			const vec2 Pos = GameClient()->m_MalinkaAutoEdge.m_FoundPos[Dummy][Index];
			const float Inward = Index == 0 ? 10.0f : -10.0f;
			AddLineSegment(vFoundSegments, Pos + vec2(0.0f, -10.0f), Pos + vec2(0.0f, 8.0f));
			AddLineSegment(vFoundSegments, Pos + vec2(0.0f, -10.0f), Pos + vec2(Inward, -5.0f));
			AddLineSegment(vFoundSegments, Pos + vec2(0.0f, 8.0f), Pos + vec2(Inward, 3.0f));
			AddLineSegment(vFoundSegments, Pos + vec2(Inward, -5.0f), Pos + vec2(Inward, 3.0f));
		}
	}

	if(g_Config.m_MalinkaAutoEdgeShowLocked && GameClient()->m_MalinkaAutoEdge.m_Locked[Dummy])
	{
		const vec2 Pos = GameClient()->m_MalinkaAutoEdge.m_LockedPos[Dummy];
		CCharacter *pLocalCharacter = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
		const bool SafeSideLeft = pLocalCharacter != nullptr ? pLocalCharacter->GetPos().x < Pos.x : true;
		const float Inward = SafeSideLeft ? -13.0f : 13.0f;
		AddLineSegment(vLockedSegments, Pos + vec2(0.0f, -14.0f), Pos + vec2(0.0f, 12.0f));
		AddLineSegment(vLockedSegments, Pos + vec2(0.0f, -14.0f), Pos + vec2(Inward, -8.0f));
		AddLineSegment(vLockedSegments, Pos + vec2(0.0f, 12.0f), Pos + vec2(Inward, 6.0f));
		AddLineSegment(vLockedSegments, Pos + vec2(Inward, -8.0f), Pos + vec2(Inward, 6.0f));
		AddLineSegment(vLockedSegments, Pos + vec2(Inward, -1.0f), Pos + vec2(Inward + (SafeSideLeft ? -6.0f : 6.0f), -1.0f));
	}

	RenderLineSegments(Graphics(), vFoundSegments, ColorRGBA(1.0f, 0.76f, 0.25f, 0.92f), 2.0f, GameClient()->m_Camera.m_Zoom);
	RenderLineSegments(Graphics(), vLockedSegments, ColorRGBA(0.96f, 0.2f, 0.18f, 0.98f), 3.0f, GameClient()->m_Camera.m_Zoom);
}

void CMalinkaVisuals::RenderSelfHitEsp()
{
	const int Dummy = g_Config.m_ClDummy;
	if(!GameClient()->m_Controls.m_aMalinkaAssistAimActive[Dummy])
		return;

	const int Weapon = GameClient()->m_Controls.m_aMalinkaAssistAimWeapon[Dummy];
	if((Weapon == WEAPON_LASER && !g_Config.m_MalinkaUnfreezeBotEsp) ||
		(Weapon == WEAPON_SHOTGUN && !g_Config.m_MalinkaAutoShotgun))
		return;

	CCharacter *pLocalCharacter = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(pLocalCharacter == nullptr)
		return;

	const CTuningParams *pTuning = GameClient()->GetTuning(pLocalCharacter->GetOverriddenTuneZone());
	if(pTuning == nullptr)
		return;

	std::vector<IGraphics::CLineItem> vSegments;
	AddLaserPath(Collision(), vSegments, pLocalCharacter->GetPos(), GameClient()->m_Controls.m_aMalinkaAssistAimPos[Dummy], pTuning->m_LaserReach, pTuning);
	const ColorRGBA Color = Weapon == WEAPON_LASER ? ColorRGBA(0.45f, 0.95f, 1.0f, 0.9f) : ColorRGBA(1.0f, 0.72f, 0.28f, 0.9f);
	RenderLineSegments(Graphics(), vSegments, Color, 1.6f, GameClient()->m_Camera.m_Zoom);
}

void CMalinkaVisuals::RenderTileOutlines()
{
	if(!g_Config.m_MalinkaTileOutlines || Collision()->GetWidth() <= 0 || Collision()->GetHeight() <= 0)
		return;
	const auto Screen = Graphics()->GetScreen();
	if(!std::isfinite(Screen.m_TopLeft.x) || !std::isfinite(Screen.m_TopLeft.y) ||
		!std::isfinite(Screen.m_BottomRight.x) || !std::isfinite(Screen.m_BottomRight.y))
		return;
	const auto TileBound = [](float Coordinate, int Maximum) { return static_cast<int>(std::clamp(std::floor(static_cast<double>(Coordinate) / 32.0), 0.0, static_cast<double>(Maximum))); };
	const int X0 = TileBound(Screen.m_TopLeft.x, Collision()->GetWidth() - 1);
	const int Y0 = TileBound(Screen.m_TopLeft.y, Collision()->GetHeight() - 1);
	const int X1 = TileBound(Screen.m_BottomRight.x, Collision()->GetWidth() - 1);
	const int Y1 = TileBound(Screen.m_BottomRight.y, Collision()->GetHeight() - 1);
	if(static_cast<int64_t>(X1 - X0 + 1) * (Y1 - Y0 + 1) > 10000)
		return; // Extreme zoom-out must not scan/render an unbounded map every frame.
	const auto Category = [&](int X, int Y) {
		if(X < 0 || Y < 0 || X >= Collision()->GetWidth() || Y >= Collision()->GetHeight())
			return -1;
		const int Index = Y * Collision()->GetWidth() + X;
		const int Tile = Collision()->GetTileIndex(Index), Front = Collision()->GetFrontTileIndex(Index);
		const auto Has = [=](int Value) { return Tile == Value || Front == Value; };
		if(Has(TILE_FREEZE) || Has(TILE_DFREEZE) || Has(TILE_LFREEZE)) return 0;
		if(Has(TILE_DEATH)) return 1;
		if(Has(TILE_UNFREEZE) || Has(TILE_DUNFREEZE) || Has(TILE_LUNFREEZE)) return 2;
		if(Collision()->IsTeleport(Index) || Collision()->IsEvilTeleport(Index) || Collision()->IsCheckTeleport(Index) || Collision()->IsCheckEvilTeleport(Index)) return 3;
		if(Tile == TILE_SOLID || Tile == TILE_NOHOOK) return 4;
		return -1;
	};
	const int aEnabled[] = {g_Config.m_MalinkaTileOutlinesFreeze, g_Config.m_MalinkaTileOutlinesKill, g_Config.m_MalinkaTileOutlinesUnfreeze, g_Config.m_MalinkaTileOutlinesTele, g_Config.m_MalinkaTileOutlinesSolid};
	const unsigned aColors[] = {g_Config.m_MalinkaTileOutlineColorFreeze, g_Config.m_MalinkaTileOutlineColorKill, g_Config.m_MalinkaTileOutlineColorUnfreeze, g_Config.m_MalinkaTileOutlineColorTele, g_Config.m_MalinkaTileOutlineColorSolid};
	std::array<std::vector<IGraphics::CLineItem>, 5> aSegments;
	for(int Y = Y0; Y <= Y1; ++Y)
		for(int X = X0; X <= X1; ++X)
		{
			const int Cat = Category(X, Y);
			if(Cat < 0 || !aEnabled[Cat])
				continue;
			auto &v = aSegments[Cat];
			const float Left = X * 32.0f, Top = Y * 32.0f;
			if(Category(X - 1, Y) != Cat) v.emplace_back(Left, Top, Left, Top + 32.0f);
			if(Category(X + 1, Y) != Cat) v.emplace_back(Left + 32.0f, Top, Left + 32.0f, Top + 32.0f);
			if(Category(X, Y - 1) != Cat) v.emplace_back(Left, Top, Left + 32.0f, Top);
			if(Category(X, Y + 1) != Cat) v.emplace_back(Left, Top + 32.0f, Left + 32.0f, Top + 32.0f);
		}
	for(size_t Index = 0; Index < aSegments.size(); ++Index)
	{
		auto Color = color_cast<ColorRGBA>(ColorHSLA(aColors[Index]));
		Color.a = g_Config.m_MalinkaTileOutlinesAlpha / 100.0f;
		RenderLineSegments(Graphics(), aSegments[Index], Color, static_cast<float>(g_Config.m_MalinkaTileOutlinesWidth), GameClient()->m_Camera.m_Zoom);
	}
}

void CMalinkaVisuals::RenderWorldVisuals()
{
	const bool HasSelfHitEsp =
		GameClient()->m_Controls.m_aMalinkaAssistAimActive[g_Config.m_ClDummy] &&
		((GameClient()->m_Controls.m_aMalinkaAssistAimWeapon[g_Config.m_ClDummy] == WEAPON_LASER && g_Config.m_MalinkaUnfreezeBotEsp) ||
			(GameClient()->m_Controls.m_aMalinkaAssistAimWeapon[g_Config.m_ClDummy] == WEAPON_SHOTGUN && g_Config.m_MalinkaAutoShotgun));
	if(!g_Config.m_MalinkaTileOutlines && !g_Config.m_MalinkaAimLines && !g_Config.m_MalinkaTrajectoryEsp &&
		!g_Config.m_MalinkaAimbot &&
		(!g_Config.m_MalinkaAutoEdge || (!g_Config.m_MalinkaAutoEdgeShowFound && !g_Config.m_MalinkaAutoEdgeShowLocked)) &&
		!HasSelfHitEsp)
		return;

	Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->m_Camera.m_Zoom);
	RenderTileOutlines();
	RenderAimbotOverlay();
	RenderAimLines();
	RenderTrajectory();
	RenderAutoEdge();
	RenderSelfHitEsp();
}

// A trail from the actual projectile renderer, using that projectile's own tune
// zone and interpolation time. Unknown owners cannot satisfy "self only".
void CMalinkaVisuals::RenderBulletLine(const CProjectileData &Projectile, float Time, float Curvature, float Speed, float Alpha)
{
	if(!g_Config.m_MalinkaBulletLines || GameClient()->m_Menus.IsActive() ||
		!std::isfinite(Time) || Time <= 0.0f || Time > 120.0f ||
		!std::isfinite(Curvature) || !std::isfinite(Speed) || !std::isfinite(Alpha))
		return;
	if(g_Config.m_MalinkaBulletLinesSelfOnly && (!Projectile.m_ExtraInfo || Projectile.m_Owner < 0 || Projectile.m_Owner != GameClient()->m_Snap.m_LocalClientId))
		return;
	if(!((Projectile.m_Type == WEAPON_GUN && g_Config.m_MalinkaBulletLinesGun) ||
		(Projectile.m_Type == WEAPON_SHOTGUN && g_Config.m_MalinkaBulletLinesShotgun) ||
		(Projectile.m_Type == WEAPON_GRENADE && g_Config.m_MalinkaBulletLinesGrenade)))
		return;
	std::vector<IGraphics::CLineItem> vLines;
	vLines.reserve(64);
	const float Start = std::max(0.0f, Time - 2.0f);
	vec2 Previous = CalcPos(Projectile.m_StartPos, Projectile.m_StartVel, Curvature, Speed, Start);
	if(!std::isfinite(Previous.x) || !std::isfinite(Previous.y))
		return;
	for(int Segment = 1; Segment <= 64; ++Segment)
	{
		const float T = Start + (Time - Start) * Segment / 64.0f;
		const vec2 Next = CalcPos(Projectile.m_StartPos, Projectile.m_StartVel, Curvature, Speed, T);
		if(!std::isfinite(Next.x) || !std::isfinite(Next.y))
			return;
		AddLineSegment(vLines, Previous, Next);
		Previous = Next;
	}
	RenderLineSegments(Graphics(), vLines, ColorRGBA(1.0f, 0.82f, 0.28f, std::clamp(Alpha, 0.0f, 1.0f) * g_Config.m_MalinkaBulletLinesAlpha / 100.0f), 1.0f, GameClient()->m_Camera.m_Zoom);
}

void CMalinkaVisuals::RenderFrozenHud(float Width, float Height)
{
	if(!g_Config.m_MalinkaShowFrozenHud && !g_Config.m_MalinkaShowWhenLast)
		return;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	const bool HasLocal = LocalId >= 0 && LocalId < MAX_CLIENTS;
	const int LocalTeam = HasLocal ? GameClient()->m_Teams.Team(LocalId) : -1;
	std::array<int, MAX_CLIENTS> aFrozen{};
	size_t FrozenCount = 0;
	int Teammates = 0, UnfrozenTeammates = 0;
	bool KnownTeamFreeze = true, LocalFrozen = true;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const auto &Snap = GameClient()->m_Snap.m_aCharacters[ClientId];
		if(!Snap.m_Active)
			continue;
		const auto &Data = GameClient()->m_aClients[ClientId];
		const bool Frozen = Data.m_DeepFrozen || Data.m_LiveFrozen || Data.m_FreezeEnd != 0;
		const bool SameTeam = HasLocal && GameClient()->m_Teams.SameTeam(LocalId, ClientId);
		if(ClientId == LocalId)
			LocalFrozen = Frozen;
		if(SameTeam)
		{
			++Teammates;
			UnfrozenTeammates += !Frozen;
			KnownTeamFreeze &= Snap.m_HasExtendedData;
		}
		if(Snap.m_HasExtendedData && Frozen && (!g_Config.m_MalinkaShowFrozenHudOnlyInTeam || SameTeam))
			aFrozen[FrozenCount++] = ClientId;
	}
	if(g_Config.m_MalinkaShowWhenLast && HasLocal && LocalTeam > TEAM_FLOCK && LocalTeam < TEAM_SUPER &&
		KnownTeamFreeze && Teammates > 1 && UnfrozenTeammates == 1 && !LocalFrozen)
	{
		const float Font = 12.0f;
		TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MalinkaShowWhenLastColor)));
		TextRender()->Text((Width - TextRender()->TextWidth(Font, g_Config.m_MalinkaShowWhenLastText)) * 0.5f, Height * 0.65f, Font, g_Config.m_MalinkaShowWhenLastText);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	if(!g_Config.m_MalinkaShowFrozenHud || FrozenCount == 0)
		return;
	const float Size = std::clamp(g_Config.m_MalinkaShowFrozenHudSize, 8, 24);
	const float CardWidth = 110.0f, RowHeight = Size + 5.0f;
	const int Columns = std::clamp(static_cast<int>((Width - 16.0f) / CardWidth), 1, 6);
	const int Rows = std::clamp(g_Config.m_MalinkaShowFrozenHudMaxRows, 1, 6);
	const size_t Count = std::min(FrozenCount, static_cast<size_t>(Columns * Rows));
	const int UsedRows = static_cast<int>((Count + Columns - 1) / Columns);
	for(size_t Index = 0; Index < Count; ++Index)
	{
		const int ClientId = aFrozen[Index];
		const auto &Data = GameClient()->m_aClients[ClientId];
		const float X = 8.0f + (Index % Columns) * CardWidth;
		const float Y = Height - 38.0f - UsedRows * RowHeight + (Index / Columns) * RowHeight;
		CUIRect Card{X, Y, CardWidth - 3.0f, RowHeight - 2.0f};
		Card.Draw(ColorRGBA(0.03f, 0.07f, 0.12f, 0.72f), IGraphics::CORNER_ALL, 3.0f);
		float TextX = X + 4.0f;
		if(g_Config.m_MalinkaShowFrozenHudSkins)
		{
			CTeeRenderInfo Info = Data.m_RenderInfo;
			Info.m_Size = Size;
			RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_PAIN, vec2(1, 0), vec2(X + Size * 0.6f, Y + RowHeight * 0.5f));
			TextX += Size + 3.0f;
		}
		char aText[96];
		if(g_Config.m_MalinkaFrozenTeesText == 1)
			str_format(aText, sizeof(aText), "%s (#%d)", Data.m_aName, ClientId);
		else if(g_Config.m_MalinkaFrozenTeesText == 2)
		{
			if(Data.m_DeepFrozen || Data.m_FreezeEnd < 0)
				str_format(aText, sizeof(aText), "%s: deep", Data.m_aName);
			else if(Data.m_LiveFrozen)
				str_format(aText, sizeof(aText), "%s: live", Data.m_aName);
			else
				str_format(aText, sizeof(aText), "%s: %.1fs", Data.m_aName, std::max(0.0, (static_cast<double>(Data.m_FreezeEnd) - Client()->GameTick(g_Config.m_ClDummy)) / Client()->GameTickSpeed()));
		}
		else
			str_copy(aText, Data.m_aName, sizeof(aText));
		CTextCursor Cursor;
		const float Font = Size * 0.45f;
		Cursor.SetPosition(vec2(TextX, Y + (RowHeight - Font) * 0.5f));
		Cursor.m_FontSize = Font;
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END;
		Cursor.m_LineWidth = X + CardWidth - 7.0f - TextX;
		Cursor.m_MaxLines = 1;
		TextRender()->TextEx(&Cursor, aText);
	}
}

void CMalinkaVisuals::RenderFeaturesHud(float Width, float Height)
{
	if(!g_Config.m_MalinkaFeaturesHud)
		return;

	std::vector<const char *> vFeatures;
	if(g_Config.m_MalinkaAimbot)
		vFeatures.push_back("Aim");
	if(g_Config.m_MalinkaAimbotAutoShoot || g_Config.m_MalinkaAimbotAutoShootKey)
		vFeatures.push_back("Auto Shoot");
	if(g_Config.m_MalinkaAimbotAutoHookKey)
		vFeatures.push_back("Auto Hook");
	if(g_Config.m_MalinkaAimbotDrawFov)
		vFeatures.push_back("Draw FOV");
	if(g_Config.m_MalinkaAimbotTargetBox)
		vFeatures.push_back("Target Box");
	if(g_Config.m_MalinkaAimbotTargetGlow)
		vFeatures.push_back("Target Glow");
	if(g_Config.m_MalinkaAvoidfreeze)
		vFeatures.push_back("Avoid");
	if(g_Config.m_MalinkaAutoFire)
		vFeatures.push_back("Auto Fire");
	if(g_Config.m_MalinkaAutoRehook)
		vFeatures.push_back("Auto Rehook");
	if(g_Config.m_MalinkaAutoJumpSave)
		vFeatures.push_back("Jump Save");
	if(g_Config.m_MalinkaQuickStop)
		vFeatures.push_back("Quick Stop");
	if(g_Config.m_MalinkaAutoAled)
		vFeatures.push_back("Auto Aled");
	if(g_Config.m_MalinkaUnfreezeBot)
		vFeatures.push_back("Auto Unfreeze");
	if(g_Config.m_MalinkaAutoShotgun)
		vFeatures.push_back("Auto Shotgun");
	if(g_Config.m_MalinkaFakeAim)
		vFeatures.push_back("Fake Aim");
	if(g_Config.m_MalinkaAutoEdge)
		vFeatures.push_back("Auto Edge");
	if(g_Config.m_MalinkaWatermark)
		vFeatures.push_back("Watermark");
	if(g_Config.m_MalinkaNotifications)
		vFeatures.push_back("Notifications");
	if(g_Config.m_MalinkaCustomColorTee)
		vFeatures.push_back("Rainbow Tee");
	if(g_Config.m_MalinkaCustomColorHook)
		vFeatures.push_back("Rainbow Hook");
	if(g_Config.m_MalinkaCustomColorWeapon)
		vFeatures.push_back("Rainbow Weapon");
	if(g_Config.m_MalinkaTrail)
		vFeatures.push_back("Trail");
	if(g_Config.m_MalinkaSmoothCam || g_Config.m_MalinkaSuperDyncam)
		vFeatures.push_back("Smooth Cam");
	if(g_Config.m_MalinkaAimLines)
		vFeatures.push_back("Aim Lines");
	if(g_Config.m_MalinkaBulletLines)
		vFeatures.push_back("Bullet Lines");
	if(g_Config.m_MalinkaTrajectoryEsp)
		vFeatures.push_back("Trajectory");
	if(g_Config.m_MalinkaEmoteSpam)
		vFeatures.push_back("Emote Spam");

	if(vFeatures.empty())
		return;

	const float FontSize = std::clamp(3.5f + g_Config.m_MalinkaFeaturesHudFontSize * 0.7f, 4.0f, 18.0f);
	const float TagPaddingX = 5.0f;
	const float TagPaddingY = 2.0f;
	const float TagSpacing = 2.0f;
	float Y = 34.0f;
	const float BackgroundAlpha = std::clamp(g_Config.m_MalinkaFeaturesHudOpacity / 160.0f, 0.12f, 0.85f);

	for(size_t i = 0; i < vFeatures.size(); ++i)
	{
		const float TextWidth = TextRender()->TextWidth(FontSize, vFeatures[i]);
		const float TagWidth = TextWidth + TagPaddingX * 2.0f;
		const float TagHeight = FontSize + TagPaddingY * 2.0f;
		CUIRect TagRect = {Width - TagWidth, Y, TagWidth, TagHeight};
		TagRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, BackgroundAlpha), IGraphics::CORNER_L, 3.0f);

		RenderMalinkaColoredText(
			TextRender(),
			TagRect.x + TagPaddingX,
			TagRect.y + TagPaddingY - 0.25f,
			FontSize,
			vFeatures[i],
			g_Config.m_MalinkaFeaturesHudGradientStartCol,
			g_Config.m_MalinkaFeaturesHudGradientEndCol,
			g_Config.m_MalinkaFeaturesHudGradientRainbow,
			g_Config.m_MalinkaFeaturesHudSegmented,
			(float)g_Config.m_MalinkaFeaturesHudGradientSpeed);

		Y += TagHeight + TagSpacing;
	}
}

void CMalinkaVisuals::RenderWatermark(float Width, float Height)
{
	if(!g_Config.m_MalinkaWatermark)
		return;

	char aTimeBuf[32];
	str_timestamp_format(aTimeBuf, sizeof(aTimeBuf), "%H:%M");

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Malinka %s", aTimeBuf);

	const float FontSize = 7.25f;
	const float TextWidth = TextRender()->TextWidth(FontSize, aBuf);
	const float BoxPaddingX = 8.0f;
	const float BoxPaddingY = 2.5f;
	const float BoxWidth = TextWidth + BoxPaddingX * 2.0f + 2.0f;
	const float BoxHeight = FontSize + BoxPaddingY * 2.0f;
	const float X = std::min(Width - BoxWidth - 8.0f, Width * 0.5f + 56.0f);
	const float Y = 0.0f;
	(void)Height;

	CUIRect WatermarkRect = {X, Y, BoxWidth, BoxHeight};
	WatermarkRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.34f), IGraphics::CORNER_B, 3.0f);

	const float CenteredTextX = X + (BoxWidth - TextWidth) * 0.5f;
	RenderMalinkaColoredText(
		TextRender(),
		CenteredTextX,
		Y + BoxPaddingY - 0.5f,
		FontSize,
		aBuf,
		g_Config.m_MalinkaWatermarkGradientStartCol,
		g_Config.m_MalinkaWatermarkGradientEndCol,
		g_Config.m_MalinkaWatermarkGradientRainbow,
		g_Config.m_MalinkaWatermarkSegmented,
		(float)g_Config.m_MalinkaWatermarkGradientSpeed);
}

void CMalinkaVisuals::RenderNotifications(float Width, float Height)
{
	if(!g_Config.m_MalinkaNotifications)
		return;

	const double Now = Client()->LocalTime();
	while(!m_vNotifications.empty())
	{
		const SNotification &Notification = m_vNotifications.front();
		if(Now - Notification.m_StartTime > Notification.m_Duration)
			m_vNotifications.pop_front();
		else
			break;
	}

	float Y = 18.0f;
	for(const SNotification &Notification : m_vNotifications)
	{
		const float Elapsed = (float)(Now - Notification.m_StartTime);
		const float FadeIn = std::min(Elapsed / 0.25f, 1.0f);
		const float FadeOut = std::min((float)((Notification.m_StartTime + Notification.m_Duration - Now) / 0.35), 1.0f);
		const float Alpha = std::clamp(std::min(FadeIn, FadeOut), 0.0f, 1.0f);
		const float Slide = (1.0f - FadeIn) * 30.0f;

		const float TitleSize = 9.0f;
		const float TextSize = 7.5f;
		const float TitleWidth = TextRender()->TextWidth(TitleSize, Notification.m_aTitle);
		const float TextWidth = TextRender()->TextWidth(TextSize, Notification.m_aMessage);
		const float BoxWidth = std::max(TitleWidth, TextWidth) + 16.0f;
		const float BoxHeight = 26.0f;
		const float X = Width - BoxWidth - 18.0f + Slide;
		(void)Height;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.08f, 0.08f, 0.08f, 0.82f * Alpha);
		IGraphics::CQuadItem Quad(X, Y, BoxWidth, BoxHeight);
		Graphics()->QuadsDrawTL(&Quad, 1);
		Graphics()->QuadsEnd();

		TextRender()->TextColor(ColorRGBA(1.0f, 0.85f, 0.35f, Alpha));
		TextRender()->Text(X + 8.0f, Y + 4.0f, TitleSize, Notification.m_aTitle);
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
		TextRender()->Text(X + 8.0f, Y + 13.0f, TextSize, Notification.m_aMessage);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		Y += BoxHeight + 6.0f;
	}
}

void CMalinkaVisuals::OnRender()
{
	UpdateMenuTheme();

	if(GameClient()->m_Menus.IsActive())
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderWorldVisuals();

	const float Width = 300.0f * Graphics()->ScreenAspect();
	const float Height = 300.0f;
	Graphics()->MapScreen(CScreenRect(0.0f, 0.0f, Width, Height));

	RenderFrozenHud(Width, Height);
	RenderFeaturesHud(Width, Height);
	RenderWatermark(Width, Height);
	RenderNotifications(Width, Height);
}
