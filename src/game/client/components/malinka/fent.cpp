// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#include "simulation_step.h"
#include "fent.h"
#include "replay_io.h"
#include "replay_paths.h"
#include "input_helpers.h"

#include <base/hash.h>
#include <engine/map.h>

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/dbg.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/shared/serverinfo.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <queue>
#include <string>

namespace
{

struct SFentScreenPixelGrid
{
	float m_ScreenX0;
	float m_ScreenY0;
	float m_PixelSizeX;
	float m_PixelSizeY;
};

SFentScreenPixelGrid GetFentScreenPixelGrid(const IGraphics *pGraphics)
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

vec2 SnapFentPoint(const SFentScreenPixelGrid &Grid, vec2 Pos)
{
	return vec2(
		Grid.m_ScreenX0 + roundf((Pos.x - Grid.m_ScreenX0) / Grid.m_PixelSizeX) * Grid.m_PixelSizeX,
		Grid.m_ScreenY0 + roundf((Pos.y - Grid.m_ScreenY0) / Grid.m_PixelSizeY) * Grid.m_PixelSizeY);
}

float SnapFentSpan(float Span, float PixelSize)
{
	return std::max(PixelSize, roundf(Span / PixelSize) * PixelSize);
}

void RenderFentLines(IGraphics *pGraphics, const std::vector<IGraphics::CLineItem> &vSegments, ColorRGBA Color, float Thickness, float Zoom)
{
	if(vSegments.empty() || Color.a <= 0.0f)
		return;

	const SFentScreenPixelGrid Grid = GetFentScreenPixelGrid(pGraphics);
	const float PixelSize = std::max(Grid.m_PixelSizeX, Grid.m_PixelSizeY);
	const float LineWidth = SnapFentSpan(std::max(PixelSize, (0.8f + (Thickness - 1.0f) * 0.5f) * Zoom), PixelSize);

	pGraphics->TextureClear();
	std::vector<IGraphics::CFreeformItem> vQuads;
	vQuads.reserve(vSegments.size());
	for(const auto &Segment : vSegments)
	{
		const vec2 Start = SnapFentPoint(Grid, vec2(Segment.m_X0, Segment.m_Y0));
		const vec2 End = SnapFentPoint(Grid, vec2(Segment.m_X1, Segment.m_Y1));
		const vec2 Diff = End - Start;
		if(length(Diff) < 0.001f)
			continue;
		const vec2 Perp = normalize(vec2(Diff.y, -Diff.x)) * (LineWidth * 0.5f);
		vQuads.emplace_back(
			End.x - Perp.x, End.y - Perp.y,
			End.x + Perp.x, End.y + Perp.y,
			Start.x - Perp.x, Start.y - Perp.y,
			Start.x + Perp.x, Start.y + Perp.y);
	}
	if(vQuads.empty())
		return;
	pGraphics->QuadsBegin();
	pGraphics->SetColor(Color);
	pGraphics->QuadsDrawFreeform(vQuads.data(), vQuads.size());
	pGraphics->QuadsEnd();
}

bool IsFreezeTileIndex(int Tile)
{
	return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE;
}

bool IsFinishTileIndex(int Tile)
{
	return Tile == TILE_FINISH;
}

bool IsDeathTileIndex(int Tile)
{
	return Tile == TILE_DEATH;
}

bool IsBlockedTile(const CCollision *pCollision, int Index)
{
	if(!pCollision)
		return true;
	const int Tile = pCollision->GetTileIndex(Index);
	const int FrontTile = pCollision->GetFrontTileIndex(Index);
	if(Tile == TILE_SOLID)
		return true;
	if(IsDeathTileIndex(Tile) || IsDeathTileIndex(FrontTile))
		return true;
	if(IsFreezeTileIndex(Tile) || IsFreezeTileIndex(FrontTile))
		return true;
	return false;
}

bool IsHazardTile(const CCollision *pCollision, int Index)
{
	if(!pCollision || Index < 0)
		return true;
	const int Tile = pCollision->GetTileIndex(Index);
	const int FrontTile = pCollision->GetFrontTileIndex(Index);
	return IsFreezeTileIndex(Tile) || IsFreezeTileIndex(FrontTile) || IsDeathTileIndex(Tile) || IsDeathTileIndex(FrontTile);
}

bool TouchesHazard(const CCollision *pCollision, vec2 Pos, vec2 Extent = vec2(12.0f, 14.0f))
{
	if(!pCollision)
		return true;

	const vec2 aProbeOffsets[] = {
		vec2(0.0f, 0.0f),
		vec2(-Extent.x, 0.0f),
		vec2(Extent.x, 0.0f),
		vec2(0.0f, Extent.y),
		vec2(0.0f, -Extent.y),
		vec2(-Extent.x, Extent.y),
		vec2(Extent.x, Extent.y),
		vec2(-Extent.x, -Extent.y),
		vec2(Extent.x, -Extent.y),
	};

	for(const vec2 &Offset : aProbeOffsets)
	{
		const int Index = pCollision->GetPureMapIndex(Pos + Offset);
		if(Index < 0)
			continue;
		if(IsHazardTile(pCollision, Index))
			return true;
	}
	return false;
}

bool HasSolidSupport(const CCollision *pCollision, vec2 Pos)
{
	if(!pCollision)
		return false;
	return pCollision->TestBox(Pos + vec2(0.0f, 22.0f), vec2(12.0f, 4.0f));
}

bool IsDangerAhead(const CCollision *pCollision, vec2 Pos, int Direction)
{
	if(!pCollision || Direction == 0)
		return false;
	const vec2 Ahead = Pos + vec2((float)Direction * 28.0f, 0.0f);
	const int AheadIndex = pCollision->GetPureMapIndex(Ahead);
	if(AheadIndex < 0)
		return true;
	if(IsHazardTile(pCollision, AheadIndex))
		return true;
	if(!HasSolidSupport(pCollision, Ahead))
	{
		const vec2 ProbeDown = Ahead + vec2(0.0f, 28.0f);
		const int DownIndex = pCollision->GetPureMapIndex(ProbeDown);
		if(DownIndex < 0 || IsHazardTile(pCollision, DownIndex) || !pCollision->TestBox(ProbeDown, vec2(12.0f, 4.0f)))
			return true;
	}
	return false;
}

bool IsActualHazardAhead(const CCollision *pCollision, vec2 Pos, int Direction)
{
	if(!pCollision || Direction == 0)
		return false;
	const vec2 Ahead = Pos + vec2((float)Direction * 28.0f, 0.0f);
	if(TouchesHazard(pCollision, Ahead))
		return true;
	const vec2 ProbeDown = Ahead + vec2(0.0f, 28.0f);
	return TouchesHazard(pCollision, ProbeDown, vec2(12.0f, 18.0f));
}

bool IsFinishAtPos(const CCollision *pCollision, vec2 Pos)
{
	if(!pCollision)
		return false;
	const int Index = pCollision->GetPureMapIndex(Pos);
	return IsFinishTileIndex(pCollision->GetTileIndex(Index)) || IsFinishTileIndex(pCollision->GetFrontTileIndex(Index));
}

vec2 PickNearestFinish(const std::vector<vec2> &vFinishTiles, vec2 Pos, bool &HasFinish)
{
	HasFinish = false;
	float BestDist = std::numeric_limits<float>::max();
	vec2 Best(0.0f, 0.0f);
	for(const vec2 &FinishPos : vFinishTiles)
	{
		const float Dist = distance(Pos, FinishPos);
		if(Dist < BestDist)
		{
			BestDist = Dist;
			Best = FinishPos;
			HasFinish = true;
		}
	}
	return Best;
}

float ScoreHookAnchor(vec2 Pos, vec2 Vel, vec2 FinishPos, vec2 AnchorPos, vec2 PreferredDir)
{
	const vec2 PullDir = normalize(AnchorPos - Pos);
	const vec2 ToFinish = normalize(FinishPos - Pos);
	const float Speed = length(Vel);
	const float FinishSign = FinishPos.x >= Pos.x ? 1.0f : -1.0f;
	const float PullUp = std::max(0.0f, -PullDir.y);
	const float PullDown = std::max(0.0f, PullDir.y);
	const float HorizontalTowardFinish = PullDir.x * FinishSign;
	const float PredictedHorizSpeed = Vel.x + PullDir.x * 18.0f;
	const float PredictedUpward = -Vel.y + PullUp * 16.0f;
	float Score =
		dot(PullDir, ToFinish) * 4.0f +
		HorizontalTowardFinish * 5.5f +
		PullUp * 4.5f +
		PredictedHorizSpeed * FinishSign * 0.6f +
		PredictedUpward * 0.25f;
	if(length(PreferredDir) > 0.001f)
		Score += dot(PullDir, normalize(PreferredDir)) * 3.0f;
	if(Speed > 4.0f)
	{
		const vec2 VelDir = normalize(Vel);
		Score += dot(PullDir, VelDir) * 1.5f;
		if(dot(PullDir, VelDir) < -0.35f)
			Score -= 4.0f;
	}
	if(PullDown > 0.2f)
		Score -= PullDown * 8.0f;
	return Score;
}

bool PickHookTarget(const CCollision *pCollision, const std::vector<vec2> &vHookTiles, vec2 Pos, vec2 Vel, vec2 FinishPos, float HookLength, vec2 &OutTarget, int HookMode = 0, vec2 PreferredDir = vec2(0.0f, 0.0f))
{
	if(!pCollision)
		return false;

	float BestScore = -std::numeric_limits<float>::infinity();
	vec2 Best(0.0f, 0.0f);
	const vec2 ToFinish = normalize(FinishPos - Pos);
	const bool HasPreferredDir = length(PreferredDir) > 0.001f;
	if(HasPreferredDir)
		PreferredDir = normalize(PreferredDir);
	for(const vec2 &TilePos : vHookTiles)
	{
		const float Dist = distance(Pos, TilePos);
		if(Dist > HookLength || Dist < 1.0f)
			continue;
		if(HookMode == 0 && TilePos.y > Pos.y + 24.0f)
			continue;
		if(HookMode == 1 && TilePos.y > Pos.y + 72.0f)
			continue;
		if(HookMode == 2 && TilePos.y < Pos.y - 12.0f)
			continue;
		if(HookMode == 3 && std::abs(TilePos.y - Pos.y) > 96.0f)
			continue;

		vec2 CollisionPos;
		vec2 BeforeCollision;
		if(pCollision->IntersectLine(Pos, TilePos, &CollisionPos, &BeforeCollision))
		{
			if(distance(CollisionPos, TilePos) > 20.0f)
				continue;
		}

		const vec2 Dir = normalize(TilePos - Pos);
		const float Toward = dot(Dir, ToFinish);
		const float HorizontalAbs = std::abs(Dir.x);
		if(HookMode == 0 && Toward < -0.15f)
			continue;
		if(HookMode != 2 && HorizontalAbs < 0.18f)
			continue;
		const float UpBias = TilePos.y < Pos.y - 12.0f ? 1.7f : 0.0f;
		const float DownBias = TilePos.y > Pos.y + 12.0f ? 1.4f : 0.0f;
		float Score = Toward * 3.2f - (Dist / HookLength) + UpBias;
		if(HookMode == 1)
			Score += DownBias * 0.35f + HorizontalAbs * 1.2f;
		else if(HookMode == 2)
		{
			if(TilePos.y > Pos.y + 20.0f && HorizontalAbs < 0.55f)
				continue;
			Score += DownBias * 0.45f + std::max(0.0f, HorizontalAbs - 0.42f) * 3.2f;
		}
		else if(HookMode == 3)
			Score += std::max(0.0f, HorizontalAbs - 0.35f) * 3.0f - (TilePos.y > Pos.y + 8.0f ? 0.8f : 0.0f);
		else
			Score += std::max(0.0f, HorizontalAbs - 0.22f) * 1.6f;
		Score += ScoreHookAnchor(Pos, Vel, FinishPos, TilePos, HasPreferredDir ? PreferredDir : vec2(0.0f, 0.0f));
		if(Score > BestScore)
		{
			BestScore = Score;
			Best = TilePos;
		}
	}

	if(BestScore == -std::numeric_limits<float>::infinity())
		return false;
	OutTarget = Best;
	return true;
}

void EnsureAimNonZero(CNetObj_PlayerInput &Input)
{
	if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
		Input.m_TargetX = 1;
}

void BuildFlowField(const CCollision *pCollision, int Width, int Height, const std::vector<int> &vFinishIndices, std::vector<int8_t> &vOutDirX, std::vector<int8_t> &vOutDirY)
{
	const int Total = Width * Height;
	vOutDirX.assign(Total, 0);
	vOutDirY.assign(Total, 0);
	std::vector<int> vDist(Total, -1);
	std::queue<int> Queue;

	for(int Index : vFinishIndices)
	{
		if(Index < 0 || Index >= Total)
			continue;
		vDist[Index] = 0;
		Queue.push(Index);
	}

	static const int s_aOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	while(!Queue.empty())
	{
		const int Cur = Queue.front();
		Queue.pop();
		const int CurX = Cur % Width;
		const int CurY = Cur / Width;

		for(const auto &Offset : s_aOffsets)
		{
			const int Nx = CurX + Offset[0];
			const int Ny = CurY + Offset[1];
			if(Nx < 0 || Ny < 0 || Nx >= Width || Ny >= Height)
				continue;
			const int Next = Ny * Width + Nx;
			if(vDist[Next] != -1 || IsBlockedTile(pCollision, Next))
				continue;
			vDist[Next] = vDist[Cur] + 1;
			vOutDirX[Next] = (int8_t)(CurX - Nx);
			vOutDirY[Next] = (int8_t)(CurY - Ny);
			Queue.push(Next);
		}
	}
}

bool FindNearestFlowDir(const CCollision *pCollision, int Width, int Height, int StartIndex, const std::vector<int8_t> &vDirX, const std::vector<int8_t> &vDirY, int Radius, int &OutDirX, int &OutDirY)
{
	if(!pCollision || Width <= 0 || Height <= 0)
		return false;

	const int StartX = StartIndex % Width;
	const int StartY = StartIndex / Width;
	int BestDist = std::numeric_limits<int>::max();
	bool Found = false;
	for(int dy = -Radius; dy <= Radius; ++dy)
	{
		for(int dx = -Radius; dx <= Radius; ++dx)
		{
			const int Nx = StartX + dx;
			const int Ny = StartY + dy;
			if(Nx < 0 || Ny < 0 || Nx >= Width || Ny >= Height)
				continue;
			const int Index = Ny * Width + Nx;
			if(IsBlockedTile(pCollision, Index))
				continue;
			const int DirXVal = vDirX[Index];
			const int DirYVal = vDirY[Index];
			if(DirXVal == 0 && DirYVal == 0)
				continue;
			const int Dist = abs(dx) + abs(dy);
			if(Dist < BestDist)
			{
				BestDist = Dist;
				OutDirX = DirXVal;
				OutDirY = DirYVal;
				Found = true;
			}
		}
	}
	return Found;
}

bool HasFlowRoute(const CCollision *pCollision, int Width, int Height, vec2 Pos, const std::vector<int8_t> &vDirX, const std::vector<int8_t> &vDirY)
{
	if(!pCollision || Width <= 0 || Height <= 0)
		return false;
	const int Index = pCollision->GetPureMapIndex(Pos);
	if(Index >= 0 && Index < (int)vDirX.size() && (vDirX[Index] != 0 || vDirY[Index] != 0))
		return true;
	int DummyX = 0;
	int DummyY = 0;
	return FindNearestFlowDir(pCollision, Width, Height, Index, vDirX, vDirY, 6, DummyX, DummyY);
}

CNetObj_PlayerInput BuildFlowInput(const CCollision *pCollision, CCharacter *pChar, const std::vector<int8_t> &vDirX, const std::vector<int8_t> &vDirY, int Width, int Height, vec2 FinishPos, const std::vector<vec2> &vHookTiles, int TickNow, int &LastJumpTick, int Variant)
{
	CNetObj_PlayerInput Input{};
	if(!pCollision || !pChar || Width <= 0 || Height <= 0)
		return Input;

	const vec2 Pos = pChar->GetPos();
	const int Index = pCollision->GetPureMapIndex(Pos);
	if(Index < 0 || Index >= (int)vDirX.size() || Index >= (int)vDirY.size())
	{
		Input.m_TargetX = 1;
		return Input;
	}
	int DirXVal = (Index >= 0 && Index < (int)vDirX.size()) ? vDirX[Index] : 0;
	int DirYVal = (Index >= 0 && Index < (int)vDirY.size()) ? vDirY[Index] : 0;
	if(DirXVal == 0 && DirYVal == 0)
	{
		int FallbackDirX = 0;
		int FallbackDirY = 0;
		if(FindNearestFlowDir(pCollision, Width, Height, Index, vDirX, vDirY, 6, FallbackDirX, FallbackDirY))
		{
			DirXVal = FallbackDirX;
			DirYVal = FallbackDirY;
		}
		else
		{
			const vec2 ToFinish = FinishPos - Pos;
			if(std::abs(ToFinish.x) > 8.0f)
				DirXVal = ToFinish.x > 0.0f ? 1 : -1;
			if(ToFinish.y < -8.0f)
				DirYVal = -1;
		}
	}

	Input.m_Direction = DirXVal == 0 ? 0 : (DirXVal > 0 ? 1 : -1);
	const bool WantsUp = DirYVal < 0;
	const bool Grounded = pChar->IsGrounded();
	const bool BlockedAhead = Input.m_Direction != 0 && pCollision->TestBox(Pos + vec2((float)Input.m_Direction * 16.0f, 0.0f), vec2(28.0f, 28.0f));
	const bool AllowJump = (Variant % 5) != 3;
	const int JumpSpacing = 3 + (Variant % 4);
	if(AllowJump && (WantsUp || BlockedAhead) && Grounded && TickNow - LastJumpTick > JumpSpacing)
	{
		Input.m_Jump = 1;
		LastJumpTick = TickNow;
	}

	vec2 AimTarget = FinishPos - Pos;
	if(length(AimTarget) < 8.0f)
		AimTarget = vec2((float)Input.m_Direction, 0.0f) * 64.0f;

	const float HookLength = 380.0f;
	const bool HookDisabled = (Variant % 6) == 0;
	const bool HookOnlyIfHigh = (Variant % 6) == 1;
	const bool PreferHook = !HookDisabled && ((FinishPos.y < Pos.y - (HookOnlyIfHigh ? 96.0f : 48.0f)) || (!Grounded && BlockedAhead));
	vec2 HookTarget;
	if(PreferHook && PickHookTarget(pCollision, vHookTiles, Pos, pChar->Core() != nullptr ? pChar->Core()->m_Vel : vec2(0.0f, 0.0f), FinishPos, HookLength, HookTarget))
	{
		Input.m_Hook = 1;
		AimTarget = HookTarget - Pos;
	}

	if((Variant % 8) == 5 && Grounded && std::abs(Input.m_Direction) > 0)
	{
		const int ProbeIndex = pCollision->GetPureMapIndex(Pos + vec2((float)Input.m_Direction * 64.0f, 24.0f));
		if(ProbeIndex >= 0 && ProbeIndex < Width * Height)
		{
			const int ProbeTile = pCollision->GetTileIndex(ProbeIndex);
			const int ProbeFront = pCollision->GetFrontTileIndex(ProbeIndex);
			if(IsFreezeTileIndex(ProbeTile) || IsFreezeTileIndex(ProbeFront) || IsDeathTileIndex(ProbeTile) || IsDeathTileIndex(ProbeFront))
				Input.m_Direction = 0;
		}
	}

	Input.m_TargetX = (int)AimTarget.x;
	Input.m_TargetY = (int)AimTarget.y;
	EnsureAimNonZero(Input);
	return Input;
}

std::string SanitizeFileName(const char *pName)
{
	std::string Name = pName && pName[0] ? pName : "map";
	for(char &Ch : Name)
	{
		if(Ch == '<' || Ch == '>' || Ch == ':' || Ch == '"' || Ch == '/' || Ch == '\\' || Ch == '|' || Ch == '?' || Ch == '*')
			Ch = '_';
	}
	return Name;
}

struct SFentPlanQuality
{
	int m_TotalCandidates = 128;
	int m_CandidatesPerSlice = 1;
	int m_HorizonTicks = 50;
	int m_CommitTicks = 10;
};


bool IsFentModeActive()
{
	return g_Config.m_MalinkaAvoidMode == 3;
}

const char *GetAvoidRuntimeLabel()
{
	return "Fent";
}

SFentPlanQuality GetFentPlanQuality()
{
	SFentPlanQuality Quality;

	if(g_Config.m_MalinkaAvoidFentAdvancedSettings)
	{
		Quality.m_CommitTicks = 10;
		Quality.m_HorizonTicks = std::clamp(std::max(50, g_Config.m_MalinkaAvoidFentTweakerTicks), 50, 50);
		const int Total = std::clamp(g_Config.m_MalinkaAvoidFentTweakerInputs * std::max(12, g_Config.m_MalinkaAvoidFentTweakerDosage / 2), 2048, 32768);
		Quality.m_TotalCandidates = Total;
		Quality.m_CandidatesPerSlice = 128;
	}
	else
	{
		switch(g_Config.m_MalinkaAvoidFentQualitySetting)
		{
		case 0:
			Quality = {4096, 32, 50, 10};
			break;
		case 1:
			Quality = {8192, 64, 50, 10};
			break;
		default:
			Quality = {16384, 128, 50, 10};
			break;
		}
	}
	return Quality;
}

float EvaluateCandidateScore(vec2 StartPos, vec2 EndPos, vec2 FinishPos, bool HitFreeze, bool HitFinish, float MaxSpeed, float EndSpeed, int HookTicks, float HookProgressGain, float TotalTravel, float NetProgress, int IdleTicks)
{
	const float StartDist = distance(StartPos, FinishPos);
	const float EndDist = distance(EndPos, FinishPos);
	const float FinishDirX = FinishPos.x >= StartPos.x ? 1.0f : -1.0f;
	const float HorizontalProgress = (EndPos.x - StartPos.x) * FinishDirX;
	const float VerticalProgress = StartPos.y - EndPos.y;
	const float WasteTravel = std::max(0.0f, TotalTravel - NetProgress);
	float Score =
		(StartDist - EndDist) * 8.0f +
		NetProgress * 4.5f +
		HorizontalProgress * 18.0f +
		VerticalProgress * 1.25f +
		MaxSpeed * 5.0f +
		EndSpeed * 13.0f +
		HookProgressGain * 6.5f +
		TotalTravel * 0.08f -
		WasteTravel * 5.5f -
		HookTicks * 14.0f -
		IdleTicks * 12.0f;
	if(HookTicks > 0 && HookProgressGain < 8.0f)
		Score -= 900.0f;
	if(HookTicks == 0 && HorizontalProgress < 96.0f && EndSpeed < 18.0f)
		Score -= 500.0f;
	if(HorizontalProgress < -4.0f)
		Score -= 1200.0f;
	if(HorizontalProgress < 8.0f && TotalTravel > 48.0f)
		Score -= 1200.0f;
	if(EndSpeed < 3.0f && HorizontalProgress < 24.0f)
		Score -= 900.0f;
	if(HitFinish)
		Score += 100000.0f;
	if(HitFreeze)
		Score -= 100000.0f;
	return Score;
}
}

void CMalinkaFent::OnShutdown()
{
	StopRun(false);
}

void CMalinkaFent::OnReset()
{
	StopRun(false);
}

void CMalinkaFent::OnStateChange(int NewState, int OldState)
{
	(void)OldState;
	if(NewState != IClient::STATE_ONLINE)
		StopRun(false);
}

void CMalinkaFent::EnsureWorldInitialized()
{
	if(m_WorldInitialized || Client()->State() != IClient::STATE_ONLINE)
		return;
	auto pWorld = std::make_unique<CGameWorld>();
	if(!pWorld->CopyWorldIndependent(&GameClient()->m_PredictedWorld))
	{
		GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Fent", "Cannot capture a bounded independent world."));
		return;
	}
	pWorld->m_WorldConfig.m_PredictEvents = false;
	auto pInitial = std::make_unique<CGameWorld>();
	if(!pInitial->CopyWorldIndependent(pWorld.get()))
		return;
	m_pWorld = std::move(pWorld);
	m_pWorldPredicted = std::move(pInitial);
	m_WorldInitialized = true;
}

void CMalinkaFent::ShutdownWorld()
{
	m_pWorld.reset();
	m_pWorldPredicted.reset();
	m_WorldInitialized = false;
}

void CMalinkaFent::ResetWorldFromPredicted()
{
	ShutdownWorld();
	EnsureWorldInitialized();
}

void CMalinkaFent::ResetWorldFromSnapshot()
{
	if(!m_pWorldPredicted)
	{
		ResetWorldFromPredicted();
		return;
	}
	auto pWorld = std::make_unique<CGameWorld>();
	if(!pWorld->CopyWorldIndependent(m_pWorldPredicted.get()))
	{
		ShutdownWorld();
		return;
	}
	m_pWorld = std::move(pWorld);
	m_WorldInitialized = true;
}

void CMalinkaFent::CacheTiles()
{
	m_vFinishTiles.clear();
	m_vFinishTileIndices.clear();
	m_vHookTiles.clear();
	m_TilesCached = false;
	const CCollision *pCollision = Collision();
	if(!pCollision)
		return;

	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	if(Width <= 0 || Height <= 0 || static_cast<int64_t>(Width) * Height > 2000000)
		return; // Explicit resource bound for flowfield allocation and map scanning.
	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			const int Index = y * Width + x;
			const int Tile = pCollision->GetTileIndex(Index);
			const int FrontTile = pCollision->GetFrontTileIndex(Index);
			if(IsFinishTileIndex(Tile) || IsFinishTileIndex(FrontTile))
			{
				m_vFinishTiles.push_back(pCollision->GetPos(Index));
				m_vFinishTileIndices.push_back(Index);
			}
			if(Tile == TILE_SOLID && FrontTile != TILE_NOHOOK)
				m_vHookTiles.push_back(pCollision->GetPos(Index));
		}
	}
	m_FieldWidth = Width;
	m_FieldHeight = Height;
	m_TilesCached = true;
}

void CMalinkaFent::BuildFlowField()
{
	const CCollision *pCollision = Collision();
	if(!pCollision || !m_TilesCached)
		return;
	::BuildFlowField(pCollision, m_FieldWidth, m_FieldHeight, m_vFinishTileIndices, m_vFieldDirX, m_vFieldDirY);
	const int Total = m_FieldWidth * m_FieldHeight;
	m_FieldReady = m_FieldWidth > 0 && m_FieldHeight > 0 && (int)m_vFieldDirX.size() == Total && (int)m_vFieldDirY.size() == Total;
}

void CMalinkaFent::StartRun()
{
	dbg_msg("fent", "StartRun begin");
	StopRun(false);
	m_PrimarySlot = g_Config.m_ClDummy;
	m_aSimIds = {GameClient()->m_aLocalIds[0], GameClient()->m_aLocalIds[1]};
	if(!Client()->DummyConnected())
		m_aSimIds[!m_PrimarySlot] = -1;
	m_CapturedDummyInput = GameClient()->m_Controls.m_aInputData[!m_PrimarySlot];
	const vec2 DummyAim = Malinka::FiniteAim(vec2(m_CapturedDummyInput.m_TargetX, m_CapturedDummyInput.m_TargetY));
	m_CapturedDummyInput.m_TargetX = static_cast<int>(DummyAim.x);
	m_CapturedDummyInput.m_TargetY = static_cast<int>(DummyAim.y);
	if(m_aSimIds[m_PrimarySlot] < 0 || (m_aSimIds[!m_PrimarySlot] >= 0 && !Malinka::Replay::ValidInput(m_CapturedDummyInput)))
	{
		m_Finished = true;
		GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Fent", "Invalid participant/input; no simulation was started."));
		return;
	}
	ResetWorldFromPredicted();
	if(!m_WorldInitialized)
	{
		dbg_msg("fent", "StartRun abort: world not initialized");
		return;
	}

	CacheTiles();
	BuildFlowField();
	dbg_msg("fent", "tiles=%d finishes=%d hooks=%d field=%dx%d ready=%d", m_TilesCached ? 1 : 0, (int)m_vFinishTiles.size(), (int)m_vHookTiles.size(), m_FieldWidth, m_FieldHeight, m_FieldReady ? 1 : 0);
	m_vFrames.clear();
	m_vTrailPositions.clear();
	m_Accumulator = 0.0;
	m_LastUpdateTime = time_get();
	m_StartDelayFrames = 2;
	m_LastJumpTick = -1000;
	m_StuckTicks = 0;
	m_NoSafePlanStreak = 0;
	m_Finished = false;
	m_Saved = false;
	m_Planning = false;
	m_ExecCursor = 0;
	m_ReplanCountdown = 0;
	m_PlanningTotalCandidates = 0;
	m_PlanningDoneCandidates = 0;
	m_SpectatePauseSent = false;
	m_BestCandidateScore = -1e9f;
	m_LastAcceptedScore = -1e9f;
	m_LastChosenVariant = -1;
	m_RepeatedChoiceCount = 0;
	m_ForcedMoveTicks = 0;
	m_ForcedMoveDir = 0;
	for(int i = 0; i < 6; ++i)
	{
		m_RecentVariants[i] = -1;
		m_RecentVariantPositions[i] = vec2(0.0f, 0.0f);
	}
	for(int i = 0; i < 3; ++i)
		m_BannedVariantsAtLoop[i] = -1;
	m_BannedLoopPos = vec2(0.0f, 0.0f);
	m_LastChoiceStartPos = vec2(0.0f, 0.0f);
	m_BannedVariant = -1;
	m_BannedVariantPos = vec2(0.0f, 0.0f);
	m_HasBannedOpeningInput = false;
	m_BannedOpeningInput = {};
	m_BannedOpeningPos = vec2(0.0f, 0.0f);
	m_HasBannedTransition = false;
	m_BannedTransitionStartPos = vec2(0.0f, 0.0f);
	m_BannedTransitionEndPos = vec2(0.0f, 0.0f);
	m_PlanningSeed = 0;
	m_HaveSafePlan = false;
	m_vPlannedFrames.clear();
	m_vBestPreviewPath.clear();
	m_Running = m_TilesCached && m_FieldReady && !m_vFinishTiles.empty();
	if(!m_Running)
	{
		dbg_msg("fent", "StartRun abort: no runnable state");
		GameClient()->m_MalinkaVisuals.PushNotification(SWarning(GetAvoidRuntimeLabel(), "No finish tile found."));
	}
	else
	{
		const int LocalId = m_aSimIds[m_PrimarySlot];
		CCharacter *pChar = LocalId >= 0 && m_pWorld != nullptr ? m_pWorld->GetCharacterById(LocalId) : nullptr;
		dbg_msg("fent", "localid=%d char=%d", LocalId, pChar != nullptr ? 1 : 0);
		if(pChar == nullptr || !HasFlowRoute(Collision(), m_FieldWidth, m_FieldHeight, pChar->GetPos(), m_vFieldDirX, m_vFieldDirY))
		{
			dbg_msg("fent", "StartRun abort: no reachable route");
			GameClient()->m_MalinkaVisuals.PushNotification(SWarning(GetAvoidRuntimeLabel(), "No reachable route to finish."));
			StopRun(false);
			return;
		}
		m_LastRenderPos = pChar->GetPos();
		m_CurRenderPos = pChar->GetPos();
		m_vTrailPositions.push_back(pChar->GetPos());
		CNetObj_PlayerInput InitialInput{};
		InitialInput.m_TargetX = 1;
		m_vFrames.push_back(CaptureFrame(*m_pWorld, InitialInput, vec2(1.0f, 0.0f)));
	}
	dbg_msg("fent", "StartRun ready");
}

void CMalinkaFent::StopRun(bool KeepResults)
{
	m_Running = false;
	m_Finished = false;
	m_Saved = false;
	m_Accumulator = 0.0;
	m_LastUpdateTime = 0;
	m_StartDelayFrames = 0;
	m_LastJumpTick = -1000;
	m_StuckTicks = 0;
	m_NoSafePlanStreak = 0;
	m_Planning = false;
	m_ExecCursor = 0;
	m_ReplanCountdown = 0;
	m_PlanningTotalCandidates = 0;
	m_PlanningDoneCandidates = 0;
	m_SpectatePauseSent = false;
	m_BestCandidateScore = -1e9f;
	m_LastAcceptedScore = -1e9f;
	m_LastChosenVariant = -1;
	m_RepeatedChoiceCount = 0;
	m_ForcedMoveTicks = 0;
	m_ForcedMoveDir = 0;
	for(int i = 0; i < 6; ++i)
	{
		m_RecentVariants[i] = -1;
		m_RecentVariantPositions[i] = vec2(0.0f, 0.0f);
	}
	for(int i = 0; i < 3; ++i)
		m_BannedVariantsAtLoop[i] = -1;
	m_BannedLoopPos = vec2(0.0f, 0.0f);
	m_LastChoiceStartPos = vec2(0.0f, 0.0f);
	m_BannedVariant = -1;
	m_BannedVariantPos = vec2(0.0f, 0.0f);
	m_HasBannedOpeningInput = false;
	m_BannedOpeningInput = {};
	m_BannedOpeningPos = vec2(0.0f, 0.0f);
	m_HasBannedTransition = false;
	m_BannedTransitionStartPos = vec2(0.0f, 0.0f);
	m_BannedTransitionEndPos = vec2(0.0f, 0.0f);
	m_LastPlannedVariant = 0;
	m_PlanningSeed = 0;
	m_HaveSafePlan = false;
	if(!KeepResults)
	{
		m_vFrames.clear();
		m_vTrailPositions.clear();
		m_vPlannedFrames.clear();
		m_vBestPreviewPath.clear();
	}
	m_vFinishTiles.clear();
	m_vFinishTileIndices.clear();
	m_vHookTiles.clear();
	m_vFieldDirX.clear();
	m_vFieldDirY.clear();
	m_TilesCached = false;
	m_FieldReady = false;
	ShutdownWorld();
}

void CMalinkaFent::BeginPlanning()
{
	if(!m_Running || !m_WorldInitialized || !m_pWorld)
		return;
	const int Total = m_FieldWidth * m_FieldHeight;
	if(!m_FieldReady || m_FieldWidth <= 0 || m_FieldHeight <= 0 || (int)m_vFieldDirX.size() != Total || (int)m_vFieldDirY.size() != Total)
		return;
	m_Planning = true;
	m_ExecCursor = 0;
	m_ReplanCountdown = 0;
	m_vPlannedFrames.clear();
	m_vBestPreviewPath.clear();
	m_BestCandidateScore = -1e9f;
	m_PlanningDoneCandidates = 0;
	m_LastPlannedVariant = 0;
	m_HaveSafePlan = false;
	const SFentPlanQuality Quality = GetFentPlanQuality();
	m_PlanningTotalCandidates = Quality.m_TotalCandidates;
	if(IsFentModeActive() && g_Config.m_MalinkaAvoidFentQualitySetting == 2)
		m_PlanningTotalCandidates = std::min(m_PlanningTotalCandidates, 4096);
	dbg_msg("fent", "BeginPlanning total=%d horizon=%d commit=%d", m_PlanningTotalCandidates, Quality.m_HorizonTicks, Quality.m_CommitTicks);
}

void CMalinkaFent::RunPlanningSlice()
{
	if(!m_Planning || !m_Running || !m_WorldInitialized || !m_pWorld)
		return;
	const int Total = m_FieldWidth * m_FieldHeight;
	if(!m_FieldReady || m_FieldWidth <= 0 || m_FieldHeight <= 0 || (int)m_vFieldDirX.size() != Total || (int)m_vFieldDirY.size() != Total || Collision() == nullptr)
	{
		StopRun(false);
		return;
	}

	const int LocalId = m_aSimIds[m_PrimarySlot];
	CCharacter *pBaseChar = LocalId >= 0 ? m_pWorld->GetCharacterById(LocalId) : nullptr;
	if(!pBaseChar)
		return;

	const SFentPlanQuality Quality = GetFentPlanQuality();
	bool HasFinish = false;
	const vec2 FinishPos = PickNearestFinish(m_vFinishTiles, pBaseChar->GetPos(), HasFinish);
	if(!HasFinish)
		return;

	if(m_PlanningDoneCandidates == 0)
		dbg_msg("fent", "RunPlanningSlice begin pos=%.1f %.1f finish=%.1f %.1f", pBaseChar->GetPos().x, pBaseChar->GetPos().y, FinishPos.x, FinishPos.y);
	const vec2 PlanningStartPos = pBaseChar->GetPos();

	float BestFallbackScore = -1e9f;
	int BestFallbackSafeFrames = 0;
	int BestFallbackVariant = 0;
	std::vector<SFrame> vBestFallbackFrames;
	std::vector<vec2> vBestFallbackPreview;
	float BestCommitScore = -1e9f;
	int BestCommitVariant = -1;
	std::vector<SFrame> vBestCommitFrames;
	std::vector<vec2> vBestCommitPreview;
	float BestHorizonScore = -1e9f;
	int BestHorizonVariant = -1;
	std::vector<SFrame> vBestHorizonFrames;
	std::vector<vec2> vBestHorizonPreview;
	float BestWindowScore = -1e9f;
	int BestWindowVariant = -1;
	std::vector<SFrame> vBestWindowFrames;
	std::vector<vec2> vBestWindowPreview;
	float BestSurvivorScore = -1e9f;
	int BestSurvivorVariant = -1;
	std::vector<SFrame> vBestSurvivorFrames;
	std::vector<vec2> vBestSurvivorPreview;
	int RejectSameOpeningCount = 0;
	int RejectNullCharCount = 0;
	int RejectOutOfBoundsCount = 0;
	int RejectFreezeStateCount = 0;
	int RejectHazardTileCount = 0;
	int RejectEndsWithHookCount = 0;
	int RejectBannedTransitionCount = 0;
	int RejectCriticalEdgeCount = 0;
	int RejectCommitStagnantCount = 0;
	int FullHorizonSafeCount = 0;
	int FullCommitSafeCount = 0;
	int FullWindowSafeCount = 0;

	for(int Candidate = 0; Candidate < Quality.m_CandidatesPerSlice && m_PlanningDoneCandidates < m_PlanningTotalCandidates; ++Candidate, ++m_PlanningDoneCandidates)
	{
		const int Variant = m_PlanningSeed + m_PlanningDoneCandidates;
		if(distance(m_BannedLoopPos, PlanningStartPos) < 24.0f)
		{
			bool BannedByLoop = false;
			for(int i = 0; i < 3; ++i)
			{
				if(m_BannedVariantsAtLoop[i] == Variant)
				{
					BannedByLoop = true;
					break;
				}
			}
			if(BannedByLoop)
				continue;
		}
		if(m_BannedVariant == Variant && distance(m_BannedVariantPos, PlanningStartPos) < 48.0f)
			continue;
		const int VariantMode = Variant % 288;
		const int DirectionMode = (Variant / 3) % 6;
		const int HookStartTick = Variant % 6;
		const int HookHoldTicks = 12 + ((Variant / 5) % 9);
		const float AimLift = 56.0f + (float)((Variant / 7) % 12) * 18.0f;
		const float AimForward = 72.0f + (float)((Variant / 11) % 14) * 20.0f;
		const bool PreferCeilingHook = (VariantMode >= 8 && VariantMode <= 95) || (VariantMode >= 144 && VariantMode <= 223);
		const bool PreferLowerHook = false;
		const bool PreferFlatHook = (VariantMode >= 96 && VariantMode <= 143) || VariantMode >= 224;
		const bool ForceAnyHook = (VariantMode % 7) == 0;
		const bool BurstJump = (VariantMode % 8) == 2 || (VariantMode % 8) == 5;
		const bool EarlyJump = (VariantMode % 10) == 1 || (VariantMode % 10) == 6;
		const bool BrakeStart = DirectionMode == 2;
		const bool StrongForward = DirectionMode == 4 || DirectionMode == 5;
		const bool ForceOppositeStart = DirectionMode == 3;
		const bool NeutralStart = DirectionMode == 1;
		const bool LateHook = (VariantMode % 17) == 9;
		const bool NoHookVariant = (VariantMode % 64) == 0;
		const bool HighCeilingVariant = (VariantMode % 12) == 5 || (VariantMode % 12) == 9;
		const int HookMode = PreferFlatHook ? 3 : (((VariantMode / 8) % 3) == 1 ? 1 : 0);
		const int OpeningDirCode = Variant % 243; // 3^5
		const int OpeningJumpMask = (Variant / 243) % 32;
		const int OpeningHookMask = (Variant / (243 * 32)) % 32;
		const int OpeningAimAngleBase = (Variant / (243 * 32 * 32)) % 24;
		const int OpeningAimLengthMode = (Variant / 13) % 6;
		const int OpeningAimBend = (Variant / 19) % 7;
		auto GetOpeningDir = [&](int Tick) {
			int Code = OpeningDirCode;
			for(int i = 0; i < Tick; ++i)
				Code /= 3;
			return (Code % 3) - 1;
		};
		std::vector<SFrame> vCandidateFrames;
		std::vector<vec2> vCandidatePreview;
		vCandidateFrames.reserve(Quality.m_HorizonTicks);
		vCandidatePreview.reserve(Quality.m_HorizonTicks);

		int LocalLastJumpTick = m_LastJumpTick;
		bool HitFreeze = false;
		bool HitFinish = false;
		int HazardTick = Quality.m_HorizonTicks;
		if(pBaseChar->Core() == nullptr)
			continue;
		CGameWorld SimWorld;
		SimWorld.Init(Collision(), GameClient()->m_GameWorld.TuningList(), nullptr);
		if(!SimWorld.CopyWorldIndependent(m_pWorld.get()))
		{
			StopRun(true);
			return;
		}
		CCharacter *pSimChar = SimWorld.GetCharacterById(LocalId);
		if(pSimChar == nullptr || pSimChar->Core() == nullptr)
			continue;

		float MaxSpeed = length(pSimChar->Core()->m_Vel);
		const vec2 StartPos = pSimChar->GetPos();
		float LastDist = distance(StartPos, FinishPos);
		float HookProgressGain = 0.0f;
		int HookTicks = 0;
		float TotalTravel = 0.0f;
		float NetProgress = 0.0f;
		int IdleTicks = 0;
		vec2 LastPos = StartPos;
		vec2 SimPos = StartPos;
		int PlannedHookHoldRemaining = 0;
		vec2 PlannedHookAim(0.0f, -160.0f);
		int SimIndex = Collision()->GetPureMapIndex(SimPos);
		if(SimIndex < 0 || SimIndex >= Total)
			continue;

		for(int Tick = 0; Tick < Quality.m_HorizonTicks; ++Tick)
		{
			if(m_PlanningDoneCandidates == 0 && Tick == 0)
				dbg_msg("fent", "candidate0 tick0 before input");
			int DirXVal = m_vFieldDirX[SimIndex];
			int DirYVal = m_vFieldDirY[SimIndex];
			if(DirXVal == 0 && DirYVal == 0)
			{
				int FallbackDirX = 0;
				int FallbackDirY = 0;
				if(FindNearestFlowDir(Collision(), m_FieldWidth, m_FieldHeight, SimIndex, m_vFieldDirX, m_vFieldDirY, 24, FallbackDirX, FallbackDirY))
				{
					DirXVal = FallbackDirX;
					DirYVal = FallbackDirY;
				}
			}

			// If the flow field has no answer here, keep making progress toward the finish
			// instead of freezing in place.
			if(DirXVal == 0)
			{
				const float ToFinishX = FinishPos.x - SimPos.x;
				if(std::abs(ToFinishX) > 8.0f)
					DirXVal = ToFinishX > 0.0f ? 1 : -1;
			}
			if(DirYVal == 0)
			{
				const float ToFinishY = FinishPos.y - SimPos.y;
				if(ToFinishY < -16.0f)
					DirYVal = -1;
			}

			CNetObj_PlayerInput Input{};
			Input.m_Direction = DirXVal == 0 ? 0 : (DirXVal > 0 ? 1 : -1);
			const bool NeedsHorizontalProgress = std::abs(FinishPos.x - SimPos.x) > 96.0f;
			const bool UseDenseOpening = Tick < 6;
			if(UseDenseOpening)
				Input.m_Direction = GetOpeningDir(Tick);
			if(NeutralStart && Tick < 2 && !NeedsHorizontalProgress)
				Input.m_Direction = 0;
			else if(BrakeStart && Tick < 6 && !NeedsHorizontalProgress)
				Input.m_Direction = 0;
			else if(ForceOppositeStart && Tick < 5)
				Input.m_Direction = Input.m_Direction == 0 ? 0 : -Input.m_Direction;
			else if(DirectionMode == 5 && Tick < 4)
				Input.m_Direction = FinishPos.x > SimPos.x ? 1 : -1;
			if(Input.m_Direction == 0 && NeedsHorizontalProgress && !pSimChar->m_FreezeTime)
				Input.m_Direction = FinishPos.x > SimPos.x ? 1 : -1;
			const bool WantsUp = DirYVal < 0;
			const bool DangerAhead = IsDangerAhead(Collision(), SimPos, Input.m_Direction);
			const bool NoSupportAhead = Input.m_Direction != 0 && !HasSolidSupport(Collision(), SimPos + vec2((float)Input.m_Direction * 28.0f, 0.0f));
			const bool HazardEdgeAhead = DangerAhead;
			const int JumpSpacing = 2 + (Variant % 6);
			const bool ForceJumpVariant = BurstJump || VariantMode == 7;
			const bool CriticalEdgeNow = pSimChar->IsGrounded() && HazardEdgeAhead;
			const bool CanJumpNow = CriticalEdgeNow ? true : (Tick - LocalLastJumpTick > JumpSpacing);
			const bool OpeningJumpRaw = UseDenseOpening && ((OpeningJumpMask & (1 << Tick)) != 0);
			const bool OpeningJump = OpeningJumpRaw && (Tick > 0 || WantsUp || DangerAhead || NoSupportAhead);
			const bool HeuristicJumpWanted = (WantsUp || DangerAhead || NoSupportAhead || ForceJumpVariant);
			const bool AllowFirstTickJump = DangerAhead;
			if(HeuristicJumpWanted && CanJumpNow && pSimChar->IsGrounded() && (Tick > 0 || AllowFirstTickJump))
			{
				Input.m_Jump = 1;
				LocalLastJumpTick = Tick;
			}
			if(OpeningJump && pSimChar->IsGrounded())
			{
				Input.m_Jump = 1;
				LocalLastJumpTick = Tick;
			}
			if(EarlyJump && Tick > 0 && Tick <= 1 && pSimChar->IsGrounded())
			{
				Input.m_Jump = 1;
				LocalLastJumpTick = Tick;
			}
			if(Tick == 0)
			{
				const bool FirstTickJumpNeeded = DangerAhead;
				if(!FirstTickJumpNeeded)
					Input.m_Jump = 0;
			}

			vec2 AimTarget = FinishPos - SimPos;
			if(length(AimTarget) < 8.0f)
				AimTarget = vec2((float)Input.m_Direction, 0.0f) * 64.0f;
			if(PreferCeilingHook)
			{
				const int AimMode = VariantMode % 6;
				const float DirSign = (float)(Input.m_Direction == 0 ? 1 : Input.m_Direction);
				if(AimMode == 0)
					AimTarget = vec2(DirSign * AimForward, -AimLift);
				else if(AimMode == 1)
					AimTarget = vec2(DirSign * (AimForward * 1.25f), -(AimLift * 0.7f));
				else if(AimMode == 2)
					AimTarget = vec2(DirSign * (AimForward * 0.75f), -(AimLift * 1.25f));
				else if(AimMode == 3)
					AimTarget = vec2(DirSign * (AimForward * 0.45f), -(AimLift * 0.9f));
				else if(AimMode == 4)
					AimTarget = vec2(DirSign * (AimForward * 1.45f), -(AimLift * 0.45f));
				else
					AimTarget = vec2(DirSign * (AimForward * 0.95f), -(AimLift * 1.5f));
			}
			else if(PreferLowerHook)
			{
				const float DirSign = (float)(Input.m_Direction == 0 ? 1 : Input.m_Direction);
				const int AimMode = VariantMode % 4;
				if(AimMode == 0)
					AimTarget = vec2(DirSign * (AimForward * 0.95f), AimLift * 0.9f);
				else if(AimMode == 1)
					AimTarget = vec2(DirSign * (AimForward * 1.15f), AimLift * 0.7f);
				else if(AimMode == 2)
					AimTarget = vec2(DirSign * (AimForward * 0.7f), AimLift * 1.2f);
				else
					AimTarget = vec2(DirSign * (AimForward * 0.8f), AimLift * 0.8f);
			}
			else if(PreferFlatHook)
			{
				const float DirSign = (float)(Input.m_Direction == 0 ? 1 : Input.m_Direction);
				const int AimMode = VariantMode % 4;
				if(AimMode == 0)
					AimTarget = vec2(DirSign * (AimForward * 1.25f), -AimLift * 0.55f);
				else if(AimMode == 1)
					AimTarget = vec2(DirSign * (AimForward * 1.35f), AimLift * 0.35f);
				else if(AimMode == 2)
					AimTarget = vec2(DirSign * (AimForward * 1.1f), -AimLift * 0.35f);
				else
					AimTarget = vec2(DirSign * (AimForward * 1.05f), AimLift * 0.55f);
			}
			if(HighCeilingVariant)
				AimTarget.y -= 84.0f;
			if(StrongForward)
				AimTarget.x *= 1.35f;
			const bool HookAlreadyActive = pSimChar->Core()->m_HookState > HOOK_IDLE;
			const float CurSpeed = length(pSimChar->Core()->m_Vel);
			const bool NeedSpeedBoost = std::abs(FinishPos.x - SimPos.x) > 160.0f && CurSpeed < 18.0f;
			const bool Airborne = !pSimChar->IsGrounded();
			const bool AllowLowerHook = !CriticalEdgeNow;
			const bool HookWindow = Tick >= (LateHook ? HookStartTick + 4 : HookStartTick) && Tick < (LateHook ? HookStartTick + 4 : HookStartTick) + HookHoldTicks;
			const bool OpeningHook = UseDenseOpening && Tick > 0 && ((OpeningHookMask & (1 << Tick)) != 0) && (DangerAhead || NoSupportAhead || std::abs(DirYVal) > 0);
			const bool GroundedEarlyStart = pSimChar->IsGrounded() && Tick < 2 && !DangerAhead && !NoSupportAhead && !NeedSpeedBoost;
			const bool ShouldTryHook =
				!GroundedEarlyStart &&
				!HookAlreadyActive &&
				!NoHookVariant &&
				(VariantMode >= 3 || DangerAhead || NoSupportAhead || ForceAnyHook || NeedSpeedBoost) &&
				(FinishPos.y < SimPos.y + 160.0f || DangerAhead || NoSupportAhead || NeedSpeedBoost || Airborne || (Tick > 1 && distance(SimPos, LastPos) < 2.0f)) &&
				(HookWindow || DangerAhead || NoSupportAhead || OpeningHook || NeedSpeedBoost || Airborne) &&
				(Tick > 0 || DangerAhead || NoSupportAhead || NeedSpeedBoost || Airborne);
			if(PlannedHookHoldRemaining > 0)
			{
				Input.m_Hook = 1;
				AimTarget = PlannedHookAim;
				--PlannedHookHoldRemaining;
			}
			else if(ShouldTryHook)
			{
				vec2 HookTarget;
				static const float s_aHookAnglesDeg[] = {
					-170.0f, -160.0f, -150.0f, -140.0f, -130.0f, -120.0f, -110.0f, -100.0f, -90.0f, -80.0f, -70.0f, -60.0f, -50.0f, -40.0f, -30.0f, -20.0f, -10.0f,
					0.0f,
					10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f, 160.0f, 170.0f
				};
				float AngleDeg = s_aHookAnglesDeg[UseDenseOpening ? ((OpeningAimAngleBase + Tick * (1 + OpeningAimBend)) % (int)(sizeof(s_aHookAnglesDeg) / sizeof(s_aHookAnglesDeg[0]))) : (Variant % (int)(sizeof(s_aHookAnglesDeg) / sizeof(s_aHookAnglesDeg[0])) )];
				if(Input.m_Direction < 0)
					AngleDeg = 180.0f - AngleDeg;
				const float AngleRad = AngleDeg * pi / 180.0f;
				const vec2 PreferredHookDir = vec2(cosf(AngleRad), sinf(AngleRad));
				const int EffectiveHookMode = HookMode;
				if(PickHookTarget(Collision(), m_vHookTiles, SimPos, pSimChar->Core()->m_Vel, FinishPos, 380.0f, HookTarget, EffectiveHookMode, PreferredHookDir))
				{
					if(!(CriticalEdgeNow && HookTarget.y > SimPos.y + 8.0f) && HookTarget.y <= SimPos.y + 24.0f)
					{
						Input.m_Hook = 1;
						AimTarget = HookTarget - SimPos;
						PlannedHookAim = AimTarget;
						PlannedHookHoldRemaining = std::max(0, HookHoldTicks - 1);
					}
				}
				else if(OpeningHook)
				{
					const float AngleRadFallback = AngleDeg * pi / 180.0f;
					const float AimLength = 96.0f + OpeningAimLengthMode * 48.0f;
					AimTarget = vec2(cosf(AngleRadFallback), sinf(AngleRadFallback)) * AimLength;
					Input.m_Hook = 1;
					PlannedHookAim = AimTarget;
					PlannedHookHoldRemaining = std::max(0, HookHoldTicks - 1);
				}
			}
			else if(HookAlreadyActive)
			{
				Input.m_Hook = 0;
			}
			Input.m_TargetX = (int)AimTarget.x;
			Input.m_TargetY = (int)AimTarget.y;
			EnsureAimNonZero(Input);
		if(Tick == 0)
		{
			const bool SameOpening =
				m_HasBannedOpeningInput &&
					distance(m_BannedOpeningPos, PlanningStartPos) < 128.0f &&
					Input.m_Direction == m_BannedOpeningInput.m_Direction &&
					Input.m_Jump == m_BannedOpeningInput.m_Jump &&
					Input.m_Hook == m_BannedOpeningInput.m_Hook &&
					abs(Input.m_TargetX - m_BannedOpeningInput.m_TargetX) < 96 &&
					abs(Input.m_TargetY - m_BannedOpeningInput.m_TargetY) < 96;
			if(SameOpening)
			{
				++RejectSameOpeningCount;
				HitFreeze = true;
				HazardTick = 0;
				break;
			}
		}
			if(m_PlanningDoneCandidates == 0 && Tick == 0)
				dbg_msg("fent", "candidate0 tick0 input dir=%d jump=%d hook=%d aim=%d,%d", Input.m_Direction, Input.m_Jump, Input.m_Hook, Input.m_TargetX, Input.m_TargetY);
			if(m_PlanningDoneCandidates == 0 && Tick == 0)
				dbg_msg("fent", "candidate0 tick0 before sim tick");
			if(!Malinka::TickSimulation(SimWorld, {{LocalId, Input}, {m_aSimIds[!m_PrimarySlot], m_CapturedDummyInput}}))
			{
				++RejectNullCharCount;
				HitFreeze = true;
				HazardTick = Tick;
				break;
			}
			pSimChar = SimWorld.GetCharacterById(LocalId);
			if(m_PlanningDoneCandidates == 0 && Tick == 0)
				dbg_msg("fent", "candidate0 tick0 after sim tick");
			if(pSimChar == nullptr || pSimChar->Core() == nullptr)
			{
				++RejectNullCharCount;
				HitFreeze = true;
				HazardTick = Tick;
				break;
			}
			SimPos = pSimChar->GetPos();

			const vec2 Mouse((float)Input.m_TargetX, (float)Input.m_TargetY);
			vCandidateFrames.push_back(CaptureFrame(SimWorld, Input, Mouse));
			vCandidatePreview.push_back(SimPos);
			MaxSpeed = std::max(MaxSpeed, length(pSimChar->Core()->m_Vel));
			const vec2 CurPos = SimPos;
			const float CurDist = distance(CurPos, FinishPos);
			const float TravelStep = distance(CurPos, LastPos);
			TotalTravel += TravelStep;
			if(TravelStep < 1.0f)
				++IdleTicks;
			NetProgress += std::max(0.0f, LastDist - CurDist);
			if(Input.m_Hook)
			{
				++HookTicks;
				HookProgressGain += std::max(0.0f, LastDist - CurDist);
			}
			LastDist = CurDist;
			LastPos = CurPos;

			const int TileIndex = Collision()->GetPureMapIndex(CurPos);
			if(TileIndex < 0 || TileIndex >= m_FieldWidth * m_FieldHeight)
			{
				++RejectOutOfBoundsCount;
				HitFreeze = true;
				HazardTick = Tick;
				break;
			}
			SimIndex = TileIndex;
			if(pSimChar->m_FreezeTime > 0 || pSimChar->Core()->m_DeepFrozen || pSimChar->Core()->m_LiveFrozen)
			{
				++RejectFreezeStateCount;
				HitFreeze = true;
				HazardTick = Tick;
				break;
			}
			if(TouchesHazard(Collision(), SimPos))
			{
				++RejectHazardTileCount;
				HitFreeze = true;
				HazardTick = Tick;
				break;
			}
			const int Tile = Collision()->GetTileIndex(TileIndex);
			const int FrontTile = Collision()->GetFrontTileIndex(TileIndex);
			if(IsFinishTileIndex(Tile) || IsFinishTileIndex(FrontTile))
			{
				HitFinish = true;
				break;
			}
		}

		const std::vector<SFrame> vFullCandidateFrames = vCandidateFrames;
		const std::vector<vec2> vFullCandidatePreview = vCandidatePreview;
		const vec2 FullEndPos = !vFullCandidateFrames.empty() ? vFullCandidateFrames.back().m_Pos : StartPos;
		const bool FullEndHasRoute = HasFlowRoute(Collision(), m_FieldWidth, m_FieldHeight, FullEndPos, m_vFieldDirX, m_vFieldDirY);
		const int SafeWindowTicks = std::min(5, Quality.m_HorizonTicks);
		const bool SafeForWindow = !HitFreeze || HazardTick >= SafeWindowTicks;
		const bool SafeForCommit = !HitFreeze || HazardTick >= Quality.m_CommitTicks;
		int CommitFrames = 0;
		if(!vFullCandidateFrames.empty())
		{
			CommitFrames = SafeForCommit ? std::min<int>((int)vFullCandidateFrames.size(), Quality.m_CommitTicks) : std::max(0, std::min<int>((int)vFullCandidateFrames.size(), HazardTick));
		}
		std::vector<SFrame> vCommitFrames = vFullCandidateFrames;
		std::vector<vec2> vCommitPreview = vFullCandidatePreview;
		if(CommitFrames > 0 && (int)vCommitFrames.size() > CommitFrames)
		{
			vCommitFrames.resize(CommitFrames);
			vCommitPreview.resize(std::min<int>((int)vCommitPreview.size(), CommitFrames));
			SimPos = vCommitFrames.back().m_Pos;
		}
		else if(CommitFrames > 0)
		{
			SimPos = vCommitFrames.back().m_Pos;
		}
		else
		{
			SimPos = StartPos;
		}
		const bool CommitEndHasRoute = CommitFrames > 0 && HasFlowRoute(Collision(), m_FieldWidth, m_FieldHeight, SimPos, m_vFieldDirX, m_vFieldDirY);
		if(!vCommitFrames.empty())
		{
			const SFrame &LastFrame = vCommitFrames.back();
			const bool EndsWithHook = LastFrame.m_Input.m_Hook != 0;
			const bool EndsTowardDanger = LastFrame.m_Input.m_Direction != 0 && IsDangerAhead(Collision(), LastFrame.m_Pos, LastFrame.m_Input.m_Direction);
			if(EndsWithHook && (HitFreeze || EndsTowardDanger))
			{
				++RejectEndsWithHookCount;
				// Keep the candidate alive and let scoring decide. Treating this as a
				// hard freeze was falsely killing many otherwise safe horizons.
			}
		}
		if(!vCommitFrames.empty() &&
			m_HasBannedTransition &&
			distance(m_BannedTransitionStartPos, PlanningStartPos) < 72.0f &&
			distance(m_BannedTransitionEndPos, vCommitFrames.back().m_Pos) < 72.0f)
		{
			++RejectBannedTransitionCount;
			HitFreeze = true;
			HazardTick = std::min(HazardTick, std::max(0, Quality.m_CommitTicks - 1));
		}
		float CommitTravel = 0.0f;
		float CommitNetProgress = 0.0f;
		float CommitHookProgress = 0.0f;
		int CommitHookTicks = 0;
		int CommitIdleTicks = 0;
		float CommitMaxSpeed = 0.0f;
		vec2 CommitLastPos = StartPos;
		float CommitLastDist = distance(StartPos, FinishPos);
		for(const SFrame &Frame : vCommitFrames)
		{
			const vec2 PrevPos = CommitLastPos;
			const float StepTravel = distance(CommitLastPos, Frame.m_Pos);
			CommitTravel += StepTravel;
			if(StepTravel < 1.0f)
				++CommitIdleTicks;
			const float CurDist = distance(Frame.m_Pos, FinishPos);
			const float ProgressStep = std::max(0.0f, CommitLastDist - CurDist);
			CommitNetProgress += ProgressStep;
			if(Frame.m_Input.m_Hook != 0)
			{
				++CommitHookTicks;
				CommitHookProgress += ProgressStep;
			}
			const vec2 FrameDelta = Frame.m_Pos - PrevPos;
			CommitMaxSpeed = std::max(CommitMaxSpeed, length(FrameDelta));
			CommitLastDist = CurDist;
			CommitLastPos = Frame.m_Pos;
		}
		float WindowTravel = 0.0f;
		float WindowNetProgress = 0.0f;
		float WindowHookProgress = 0.0f;
		int WindowHookTicks = 0;
		int WindowIdleTicks = 0;
		float WindowMaxSpeed = 0.0f;
		vec2 WindowLastPos = StartPos;
		float WindowLastDist = distance(StartPos, FinishPos);
		const int WindowFrames = SafeForWindow ? std::min<int>((int)vFullCandidateFrames.size(), SafeWindowTicks) : std::max(0, std::min<int>((int)vFullCandidateFrames.size(), HazardTick));
		for(int FrameIndex = 0; FrameIndex < WindowFrames; ++FrameIndex)
		{
			const SFrame &Frame = vFullCandidateFrames[FrameIndex];
			const vec2 PrevPos = WindowLastPos;
			const float StepTravel = distance(WindowLastPos, Frame.m_Pos);
			WindowTravel += StepTravel;
			if(StepTravel < 1.0f)
				++WindowIdleTicks;
			const float CurDist = distance(Frame.m_Pos, FinishPos);
			const float ProgressStep = std::max(0.0f, WindowLastDist - CurDist);
			WindowNetProgress += ProgressStep;
			if(Frame.m_Input.m_Hook != 0)
			{
				++WindowHookTicks;
				WindowHookProgress += ProgressStep;
			}
			const vec2 FrameDelta = Frame.m_Pos - PrevPos;
			WindowMaxSpeed = std::max(WindowMaxSpeed, length(FrameDelta));
			WindowLastDist = CurDist;
			WindowLastPos = Frame.m_Pos;
		}
		float HorizonTravel = 0.0f;
		float HorizonNetProgress = 0.0f;
		float HorizonHookProgress = 0.0f;
		int HorizonHookTicks = 0;
		int HorizonIdleTicks = 0;
		float HorizonMaxSpeed = 0.0f;
		vec2 HorizonLastPos = StartPos;
		float HorizonLastDist = distance(StartPos, FinishPos);
		for(const SFrame &Frame : vFullCandidateFrames)
		{
			const vec2 PrevPos = HorizonLastPos;
			const float StepTravel = distance(HorizonLastPos, Frame.m_Pos);
			HorizonTravel += StepTravel;
			if(StepTravel < 1.0f)
				++HorizonIdleTicks;
			const float CurDist = distance(Frame.m_Pos, FinishPos);
			const float ProgressStep = std::max(0.0f, HorizonLastDist - CurDist);
			HorizonNetProgress += ProgressStep;
			if(Frame.m_Input.m_Hook != 0)
			{
				++HorizonHookTicks;
				HorizonHookProgress += ProgressStep;
			}
			const vec2 FrameDelta = Frame.m_Pos - PrevPos;
			HorizonMaxSpeed = std::max(HorizonMaxSpeed, length(FrameDelta));
			HorizonLastDist = CurDist;
			HorizonLastPos = Frame.m_Pos;
		}
		const float EndSpeed = !vFullCandidateFrames.empty() && vFullCandidateFrames.size() >= 2 ?
			length(vFullCandidateFrames.back().m_Pos - vFullCandidateFrames[vFullCandidateFrames.size() - 2].m_Pos) :
			0.0f;
		int SafePrefixFrames = CommitFrames;
		while(SafePrefixFrames > 0 && vCommitFrames[SafePrefixFrames - 1].m_Input.m_Hook != 0)
			--SafePrefixFrames;
		const bool CommitBad = !SafeForCommit || !SafeForWindow || CommitFrames < Quality.m_CommitTicks;
		const bool FirstInputIsAction = !vCommitFrames.empty() && (vCommitFrames.front().m_Input.m_Jump != 0 || vCommitFrames.front().m_Input.m_Hook != 0);
		const bool EdgeStep = !vCommitFrames.empty() && IsActualHazardAhead(Collision(), StartPos, vCommitFrames.front().m_Input.m_Direction);
		const bool CriticalEdge = EdgeStep;
		const bool SafeDropEdge = !vCommitFrames.empty() &&
			vCommitFrames.front().m_Input.m_Direction != 0 &&
			!HasSolidSupport(Collision(), StartPos + vec2((float)vCommitFrames.front().m_Input.m_Direction * 28.0f, 0.0f)) &&
			!IsDangerAhead(Collision(), StartPos, vCommitFrames.front().m_Input.m_Direction);
		const bool FirstInputNoDirection = !vCommitFrames.empty() && vCommitFrames.front().m_Input.m_Direction == 0;
		const bool FirstWantsUp = FinishPos.y < StartPos.y - 16.0f;
		const bool FirstDangerAhead = !vCommitFrames.empty() && IsDangerAhead(Collision(), StartPos, vCommitFrames.front().m_Input.m_Direction);
		const bool FirstNoSupportAhead = !vCommitFrames.empty() && vCommitFrames.front().m_Input.m_Direction != 0 &&
			!HasSolidSupport(Collision(), StartPos + vec2((float)vCommitFrames.front().m_Input.m_Direction * 28.0f, 0.0f));
		const bool FirstJumpWithoutNeed = !vCommitFrames.empty() &&
			vCommitFrames.front().m_Input.m_Jump != 0 &&
			!FirstDangerAhead &&
			!FirstNoSupportAhead &&
			!FirstWantsUp;
		const bool FirstWrongWay = !vCommitFrames.empty() &&
			vCommitFrames.front().m_Input.m_Direction != 0 &&
			((FinishPos.x > StartPos.x + 24.0f && vCommitFrames.front().m_Input.m_Direction < 0) ||
				(FinishPos.x < StartPos.x - 24.0f && vCommitFrames.front().m_Input.m_Direction > 0)) &&
			!FirstDangerAhead &&
			!FirstNoSupportAhead;
		const bool FirstAirborneNoHook = !vCommitFrames.empty() &&
			!pBaseChar->IsGrounded() &&
			vCommitFrames.front().m_Input.m_Hook == 0 &&
			length(pBaseChar->Core()->m_Vel) < 18.0f;
		const bool CommitStagnant = Quality.m_CommitTicks > 1 ?
			(CommitTravel < 10.0f || CommitNetProgress < 6.0f || CommitIdleTicks >= std::max(2, Quality.m_CommitTicks - 1)) :
			(WindowTravel < 12.0f && WindowNetProgress < 8.0f && !FirstInputIsAction && !SafeDropEdge);
		if(CriticalEdge && !FirstInputIsAction)
		{
			++RejectCriticalEdgeCount;
			continue;
		}
		const vec2 WindowEndPos = WindowFrames > 0 ? vFullCandidateFrames[WindowFrames - 1].m_Pos : StartPos;
		float Score = EvaluateCandidateScore(StartPos, WindowEndPos, FinishPos, CommitBad, HitFinish, std::max(MaxSpeed, std::max(HorizonMaxSpeed, WindowMaxSpeed)), std::max(EndSpeed, CommitMaxSpeed), WindowHookTicks, WindowHookProgress, WindowTravel, WindowNetProgress, WindowIdleTicks);
		Score += HorizonNetProgress * 2.2f + HorizonMaxSpeed * 6.0f - HorizonIdleTicks * 3.0f;
		if(!HitFreeze && HorizonNetProgress > WindowNetProgress)
			Score += (HorizonNetProgress - WindowNetProgress) * 2.0f;
		if(FirstInputNoDirection && std::abs(FinishPos.x - StartPos.x) > 96.0f)
			Score -= 900.0f;
		if(FirstJumpWithoutNeed)
			Score -= 1500.0f;
		if(FirstWrongWay)
			Score -= 2500.0f;
		if(FirstAirborneNoHook)
			Score -= 1800.0f;
		if(!SafeForWindow)
			Score -= 6000.0f;
		if(!CommitEndHasRoute)
			Score -= 1200.0f;
		if(!FullEndHasRoute)
			Score -= 800.0f;
		if(!vCommitFrames.empty() && vCommitFrames.back().m_Input.m_Hook != 0)
			Score -= 1600.0f;
		if(WindowNetProgress < 24.0f)
			Score -= 1800.0f;
		if(CommitNetProgress < 16.0f)
			Score -= 2200.0f;
		if(SafeDropEdge)
			Score += 900.0f;
		if(EdgeStep)
		{
			if(!FirstInputIsAction)
				Score -= 1800.0f;
			else
				Score += 600.0f;
		}
		float HorizonScore = EvaluateCandidateScore(StartPos, FullEndPos, FinishPos, HitFreeze, HitFinish, std::max(MaxSpeed, HorizonMaxSpeed), std::max(EndSpeed, HorizonMaxSpeed), HorizonHookTicks, HorizonHookProgress, HorizonTravel, HorizonNetProgress, HorizonIdleTicks);
		if(!FullEndHasRoute)
			HorizonScore -= 2500.0f;
		if(HorizonNetProgress < WindowNetProgress)
			HorizonScore -= 2500.0f;
		if(HorizonHookTicks == 0 && HorizonNetProgress < 160.0f && HorizonMaxSpeed < 20.0f)
			HorizonScore -= 1000.0f;
		if(!vFullCandidateFrames.empty() && vFullCandidateFrames.back().m_Input.m_Hook != 0)
			HorizonScore -= 2200.0f;
		if(FirstWrongWay)
			HorizonScore -= 3200.0f;
		if(FirstAirborneNoHook)
			HorizonScore -= 2400.0f;
		HorizonScore += HorizonHookProgress * 5.5f + HorizonMaxSpeed * 7.0f;
		if(HorizonNetProgress < 64.0f)
			HorizonScore -= 1800.0f;
		const bool FullCommitSafe = SafeForCommit && CommitFrames == Quality.m_CommitTicks;
		if(FullCommitSafe)
			++FullCommitSafeCount;
		if(FullCommitSafe && !CommitStagnant && !vCommitFrames.empty() && Score > BestCommitScore)
		{
			BestCommitScore = Score;
			BestCommitVariant = Variant;
			vBestCommitFrames = vCommitFrames;
			vBestCommitPreview = vFullCandidatePreview;
		}
		const bool FullHorizonSafe = !HitFreeze && (int)vFullCandidateFrames.size() == Quality.m_HorizonTicks;
		if(FullHorizonSafe)
			++FullHorizonSafeCount;
		if(CommitStagnant)
			++RejectCommitStagnantCount;
		if(FullHorizonSafe && HorizonScore > BestHorizonScore)
		{
			BestHorizonScore = HorizonScore;
			BestHorizonVariant = Variant;
			vBestHorizonFrames = vCommitFrames;
			vBestHorizonPreview = vFullCandidatePreview;
		}
		const bool FullSafeWindow = SafeForWindow && WindowFrames >= SafeWindowTicks && !vFullCandidateFrames.empty();
		if(FullSafeWindow)
			++FullWindowSafeCount;
		if(FullSafeWindow && Score > BestWindowScore)
		{
			BestWindowScore = Score;
			BestWindowVariant = Variant;
			vBestWindowFrames = vFullCandidateFrames;
			if((int)vBestWindowFrames.size() > SafeWindowTicks)
				vBestWindowFrames.resize(SafeWindowTicks);
			vBestWindowPreview = vFullCandidatePreview;
			if((int)vBestWindowPreview.size() > Quality.m_HorizonTicks)
				vBestWindowPreview.resize(Quality.m_HorizonTicks);
		}
		const bool FullTenTickSurvivor = !HitFreeze && CommitFrames == Quality.m_CommitTicks && !vCommitFrames.empty();
		if(FullTenTickSurvivor && Score > BestSurvivorScore)
		{
			BestSurvivorScore = Score;
			BestSurvivorVariant = Variant;
			vBestSurvivorFrames = vCommitFrames;
			vBestSurvivorPreview = vCommitPreview;
		}
	}

	if(m_PlanningDoneCandidates >= m_PlanningTotalCandidates)
	{
		if(BestHorizonVariant >= 0 && !vBestHorizonFrames.empty() && BestHorizonScore > 0.0f)
		{
			m_Planning = false;
			m_ExecCursor = 0;
			m_NoSafePlanStreak = 0;
			m_HaveSafePlan = true;
			m_LastChosenVariant = BestHorizonVariant;
			m_LastPlannedVariant = BestHorizonVariant;
			m_BestCandidateScore = BestHorizonScore;
			m_LastAcceptedScore = BestHorizonScore;
			m_vPlannedFrames = std::move(vBestHorizonFrames);
			m_vBestPreviewPath = std::move(vBestHorizonPreview);
			m_ReplanCountdown = std::min(Quality.m_CommitTicks, (int)m_vPlannedFrames.size());
			if(!m_vPlannedFrames.empty())
			{
				const auto &First = m_vPlannedFrames.front();
				dbg_msg("fent", "chosen first tick dir=%d jump=%d hook=%d aim=%d,%d", First.m_Input.m_Direction, First.m_Input.m_Jump, First.m_Input.m_Hook, First.m_Input.m_TargetX, First.m_Input.m_TargetY);
			}
			dbg_msg("fent", "RunPlanningSlice horizon best=%.2f frames=%d variant=%d", m_BestCandidateScore, (int)m_vPlannedFrames.size(), m_LastPlannedVariant);
			return;
		}
		const bool StrongShortWindow =
			BestWindowVariant >= 0 &&
			!vBestWindowFrames.empty() &&
			BestWindowScore > -500.0f &&
			FullWindowSafeCount >= Quality.m_CandidatesPerSlice / 2 &&
			FullCommitSafeCount >= Quality.m_CandidatesPerSlice / 2;
		if(StrongShortWindow)
		{
			m_Planning = false;
			m_ExecCursor = 0;
			m_NoSafePlanStreak = 0;
			m_HaveSafePlan = true;
			m_LastChosenVariant = BestWindowVariant;
			m_LastPlannedVariant = BestWindowVariant;
			m_BestCandidateScore = BestWindowScore;
			m_LastAcceptedScore = BestWindowScore;
			m_vPlannedFrames = std::move(vBestWindowFrames);
			m_vBestPreviewPath = std::move(vBestWindowPreview);
			m_ReplanCountdown = std::min(Quality.m_CommitTicks, (int)m_vPlannedFrames.size());
			if(!m_vPlannedFrames.empty())
			{
				const auto &First = m_vPlannedFrames.front();
				dbg_msg("fent", "chosen short tick dir=%d jump=%d hook=%d aim=%d,%d", First.m_Input.m_Direction, First.m_Input.m_Jump, First.m_Input.m_Hook, First.m_Input.m_TargetX, First.m_Input.m_TargetY);
			}
			dbg_msg("fent", "RunPlanningSlice window best=%.2f frames=%d variant=%d", m_BestCandidateScore, (int)m_vPlannedFrames.size(), m_LastPlannedVariant);
			return;
		}
		if(!m_HaveSafePlan || m_vPlannedFrames.empty())
		{
			if(BestSurvivorVariant >= 0 && !vBestSurvivorFrames.empty())
			{
				// Do not execute short-horizon survivor fallback for Fent. It must survive the full planning horizon.
			}
			if(BestWindowVariant >= 0 && !vBestWindowFrames.empty())
			{
				// Do not execute short-window fallback for Fent. It must survive the full planning horizon.
			}

			++m_NoSafePlanStreak;
			if(m_NoSafePlanStreak >= 3)
			{
				m_BannedVariant = m_LastChosenVariant;
				m_BannedVariantPos = PlanningStartPos;
				if(!m_vPlannedFrames.empty())
				{
					m_HasBannedOpeningInput = true;
					m_BannedOpeningInput = m_vPlannedFrames.front().m_Input;
					m_BannedOpeningPos = PlanningStartPos;
					m_HasBannedTransition = true;
					m_BannedTransitionStartPos = PlanningStartPos;
					m_BannedTransitionEndPos = m_vPlannedFrames.back().m_Pos;
				}
			}
			const bool MostlySelfBanned =
				RejectBannedTransitionCount >= Quality.m_CandidatesPerSlice / 2 ||
				RejectSameOpeningCount >= Quality.m_CandidatesPerSlice / 2;
			if(MostlySelfBanned)
			{
				m_BannedVariant = -1;
				m_BannedVariantPos = vec2(0.0f, 0.0f);
				m_HasBannedOpeningInput = false;
				m_HasBannedTransition = false;
			}
			const bool HadRecentGoodPlan = m_LastAcceptedScore > 4000.0f;
			const bool CurrentZoneLooksDead =
				FullCommitSafeCount <= 0 ||
				RejectHazardTileCount >= (Quality.m_CandidatesPerSlice * 3) / 4 ||
				RejectEndsWithHookCount >= (Quality.m_CandidatesPerSlice * 3) / 4;
			const bool ShouldHoldGoodPath = HadRecentGoodPlan && !CurrentZoneLooksDead;
			const int RewindNoSafeThreshold = 6;
			if(m_NoSafePlanStreak >= RewindNoSafeThreshold && m_vFrames.size() > 20 && distance(PlanningStartPos, m_vFrames.front().m_Pos) > 96.0f && !MostlySelfBanned && !ShouldHoldGoodPath)
			{
				const vec2 StuckPos = PlanningStartPos;
				const int RewindSteps = std::min(10, std::max(1, (int)m_vFrames.size() - 1));
				m_LastAcceptedScore = -1e9f;
				m_BannedVariantPos = StuckPos;
				m_HasBannedTransition = true;
				m_BannedTransitionEndPos = StuckPos;
				m_BannedTransitionStartPos = m_vFrames[std::max(0, (int)m_vFrames.size() - RewindSteps - 1)].m_Pos;
				m_HasBannedOpeningInput = true;
				m_BannedOpeningInput = m_vFrames.back().m_Input;
				m_BannedOpeningPos = StuckPos;
				if(RewindExecutedFrames(RewindSteps))
				{
					m_NoSafePlanStreak = 0;
					m_Planning = false;
					m_PlanningDoneCandidates = 0;
					m_HaveSafePlan = false;
					m_vPlannedFrames.clear();
					m_vBestPreviewPath.clear();
					dbg_msg("fent", "planning rewind steps=%d at=%.1f %.1f variant=%d", RewindSteps, StuckPos.x, StuckPos.y, m_LastChosenVariant);
					BeginPlanning();
					return;
				}
			}
			if(m_NoSafePlanStreak >= 6 && ShouldHoldGoodPath)
			{
				m_NoSafePlanStreak = 0;
				m_BannedVariant = -1;
				m_BannedVariantPos = vec2(0.0f, 0.0f);
				m_HasBannedOpeningInput = false;
				m_HasBannedTransition = false;
				dbg_msg("fent", "planning hold good path at=%.1f %.1f score=%.2f", PlanningStartPos.x, PlanningStartPos.y, m_LastAcceptedScore);
			}

			m_PlanningDoneCandidates = 0;
			m_BestCandidateScore = -1e9f;
			m_vPlannedFrames.clear();
			m_vBestPreviewPath.clear();
			m_HaveSafePlan = false;
			m_PlanningSeed += m_PlanningTotalCandidates;
			dbg_msg("fent", "RunPlanningSlice no safe plan, counts full_h=%d full_c=%d full_w=%d same=%d null=%d oob=%d freeze=%d hazard=%d endhook=%d banned=%d edge=%d stagnant=%d seed=%d",
				FullHorizonSafeCount, FullCommitSafeCount, FullWindowSafeCount,
				RejectSameOpeningCount, RejectNullCharCount, RejectOutOfBoundsCount, RejectFreezeStateCount, RejectHazardTileCount,
				RejectEndsWithHookCount, RejectBannedTransitionCount, RejectCriticalEdgeCount, RejectCommitStagnantCount, m_PlanningSeed);
			return;
		}
	}
}

bool CMalinkaFent::IsFinishReached() const
{
	if(!m_WorldInitialized || !m_pWorld)
		return false;
	const int LocalId = m_aSimIds[m_PrimarySlot];
	if(LocalId < 0)
		return false;
	const CCharacter *pChar = m_pWorld->GetCharacterById(LocalId);
	return pChar != nullptr && IsFinishAtPos(Collision(), pChar->GetPos());
}

CMalinkaFent::SFrame CMalinkaFent::CaptureFrame(CGameWorld &World, const CNetObj_PlayerInput &Input, vec2 Mouse) const
{
	SFrame Frame;
	Frame.m_Tick = World.m_GameTick;
	Frame.m_Input = Input;
	Frame.m_Mouse = Mouse;
	if(const auto *pChar = World.GetCharacterById(m_aSimIds[m_PrimarySlot]))
		Frame.m_Pos = pChar->GetPos();
	if(const auto *pDummy = World.GetCharacterById(m_aSimIds[!m_PrimarySlot]))
	{
		Frame.m_HasDummy = true;
		Frame.m_DummyInput = m_CapturedDummyInput;
		Frame.m_DummyPos = pDummy->GetPos();
	}
	return Frame;
}

bool CMalinkaFent::TickWorld(const CNetObj_PlayerInput &Input, vec2 Mouse)
{
	if(!m_WorldInitialized || !m_pWorld)
		return false;
	CNetObj_PlayerInput AppliedInput = Input;
	const vec2 Aim = Malinka::FiniteAim(Mouse);
	AppliedInput.m_TargetX = static_cast<int>(Aim.x);
	AppliedInput.m_TargetY = static_cast<int>(Aim.y);
	if(!Malinka::Replay::ValidInput(AppliedInput) ||
		!Malinka::TickSimulation(*m_pWorld, {{m_aSimIds[m_PrimarySlot], AppliedInput}, {m_aSimIds[!m_PrimarySlot], m_CapturedDummyInput}}))
	{
		m_Running = false;
		m_Finished = true;
		m_Planning = false;
		GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Fent", "Simulation unavailable; run stopped without accepting a frame."));
		return false;
	}
	return true;
}

bool CMalinkaFent::RewindExecutedFrames(int Steps)
{
	if(Steps <= 0 || m_vFrames.size() <= 1)
		return false;

	const int KeepCount = std::max<int>(1, (int)m_vFrames.size() - Steps);
	m_vFrames.resize(KeepCount);

	ResetWorldFromSnapshot();
	if(!m_WorldInitialized || !m_pWorld)
		return false;

	const int LocalId = m_aSimIds[m_PrimarySlot];
	if(LocalId < 0)
		return false;

	m_vTrailPositions.clear();
	CCharacter *pStartChar = m_pWorld->GetCharacterById(LocalId);
	if(pStartChar)
	{
		m_LastRenderPos = pStartChar->GetPos();
		m_CurRenderPos = pStartChar->GetPos();
		m_vTrailPositions.push_back(pStartChar->GetPos());
	}

	for(size_t i = 1; i < m_vFrames.size(); ++i)
	{
		if(!TickWorld(m_vFrames[i].m_Input, m_vFrames[i].m_Mouse))
			return false;
		if(CCharacter *pAfter = m_pWorld->GetCharacterById(LocalId))
		{
			m_vFrames[i] = CaptureFrame(*m_pWorld, m_vFrames[i].m_Input, m_vFrames[i].m_Mouse);
			m_vTrailPositions.push_back(pAfter->GetPos());
			m_LastRenderPos = m_CurRenderPos;
			m_CurRenderPos = pAfter->GetPos();
		}
	}

	if(m_vFrames.size() == 1 && pStartChar)
	{
		m_vFrames[0].m_Tick = m_pWorld->m_GameTick;
		m_vFrames[0].m_Pos = pStartChar->GetPos();
	}

	m_vPlannedFrames.clear();
	m_vBestPreviewPath.clear();
	m_ExecCursor = 0;
	m_ReplanCountdown = 0;
	m_StuckTicks = 0;
	m_Accumulator = 0.0;
	return true;
}

void CMalinkaFent::AdvanceOneStep()
{
	if(!m_Running || !m_WorldInitialized || !m_pWorld || m_Finished)
		return;
	if(m_Planning)
	{
		RunPlanningSlice();
		return;
	}

	const int LocalId = m_aSimIds[m_PrimarySlot];
	if(LocalId < 0)
		return;
	CCharacter *pChar = m_pWorld->GetCharacterById(LocalId);
	if(!pChar)
		return;
	bool HasFinish = false;
	const vec2 FinishPos = PickNearestFinish(m_vFinishTiles, pChar->GetPos(), HasFinish);

	const bool CriticalEdgeNow =
		IsDangerAhead(Collision(), pChar->GetPos(), 1) || IsDangerAhead(Collision(), pChar->GetPos(), -1);
	const bool DownwardHookActive =
		pChar->Core() != nullptr &&
		pChar->Core()->m_HookState > HOOK_IDLE &&
		pChar->Core()->m_HookPos.y > pChar->GetPos().y + 8.0f;
	if(CriticalEdgeNow && DownwardHookActive)
	{
		CNetObj_PlayerInput ReleaseInput{};
		ReleaseInput.m_Direction = 0;
		ReleaseInput.m_TargetX = 1;
		ReleaseInput.m_TargetY = -1;
		ReleaseInput.m_Hook = 0;
		m_LastRenderPos = pChar->GetPos();
		if(!TickWorld(ReleaseInput, vec2((float)ReleaseInput.m_TargetX, (float)ReleaseInput.m_TargetY)))
			return;
		if(CCharacter *pAfterRelease = m_pWorld->GetCharacterById(LocalId))
		{
			m_CurRenderPos = pAfterRelease->GetPos();
			if(m_vFrames.size() >= Malinka::Replay::MAX_FRAMES)
			{
				m_Running = false;
				m_Finished = true;
				return;
			}
			m_vFrames.push_back(CaptureFrame(*m_pWorld, ReleaseInput, vec2((float)ReleaseInput.m_TargetX, (float)ReleaseInput.m_TargetY)));
			m_vTrailPositions.push_back(pAfterRelease->GetPos());
		}
		m_vPlannedFrames.clear();
		m_vBestPreviewPath.clear();
		m_ExecCursor = 0;
		m_ReplanCountdown = 0;
		BeginPlanning();
		return;
	}

	if(m_vPlannedFrames.empty() || m_ExecCursor >= (int)m_vPlannedFrames.size())
	{
		BeginPlanning();
		return;
	}

	SFrame Planned = m_vPlannedFrames[m_ExecCursor++];
	const int PlannedIndex = Collision() ? Collision()->GetPureMapIndex(Planned.m_Pos) : -1;
	if(PlannedIndex < 0 || (Collision() && IsHazardTile(Collision(), PlannedIndex)))
	{
		m_vPlannedFrames.clear();
		m_ExecCursor = 0;
		m_ReplanCountdown = 0;
		BeginPlanning();
		return;
	}
	const CNetObj_PlayerInput Input = Planned.m_Input;
	const vec2 Mouse = Planned.m_Mouse;
	m_LastRenderPos = pChar->GetPos();
	if(!TickWorld(Input, Mouse))
		return;
	if(CCharacter *pAfter = m_pWorld->GetCharacterById(LocalId))
	{
		m_CurRenderPos = pAfter->GetPos();
		if(m_vFrames.size() >= Malinka::Replay::MAX_FRAMES)
		{
			m_Running = false;
			m_Finished = true;
			GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Fent", "Stopped at the replay resource limit. Save the buffer."));
			return;
		}
		m_vFrames.push_back(CaptureFrame(*m_pWorld, Input, Mouse));
		m_vTrailPositions.push_back(pAfter->GetPos());
		const float ProgressDir = HasFinish && std::abs(FinishPos.x - m_LastRenderPos.x) > 8.0f ? (FinishPos.x > m_LastRenderPos.x ? 1.0f : -1.0f) : 0.0f;
		const float HorizontalProgress = (pAfter->GetPos().x - m_LastRenderPos.x) * ProgressDir;
		if(distance(m_LastRenderPos, pAfter->GetPos()) < 1.5f || (ProgressDir != 0.0f && HorizontalProgress < 2.5f))
			++m_StuckTicks;
		else
			m_StuckTicks = 0;
		if((int)m_vTrailPositions.size() > 20000)
			m_vTrailPositions.erase(m_vTrailPositions.begin(), m_vTrailPositions.begin() + ((int)m_vTrailPositions.size() - 20000));
	}

	if(m_ReplanCountdown > 0)
		--m_ReplanCountdown;
	if(m_StuckTicks >= 6)
	{
		const vec2 StuckEndPos = m_CurRenderPos;
		const int RewindSteps = std::min(10, std::max(1, (int)m_vFrames.size() - 1));
		m_BannedVariant = m_LastChosenVariant;
		m_BannedVariantPos = StuckEndPos;
		if(!m_vFrames.empty())
		{
			const int StartIndex = std::max(0, (int)m_vFrames.size() - RewindSteps - 1);
			m_HasBannedTransition = true;
			m_BannedTransitionStartPos = m_vFrames[StartIndex].m_Pos;
			m_BannedTransitionEndPos = StuckEndPos;
			m_HasBannedOpeningInput = true;
			m_BannedOpeningInput = m_vFrames.back().m_Input;
			m_BannedOpeningPos = StuckEndPos;
		}
		if(!RewindExecutedFrames(10))
		{
			m_StuckTicks = 0;
			m_vPlannedFrames.clear();
			m_vBestPreviewPath.clear();
			m_ExecCursor = 0;
			m_ReplanCountdown = 0;
		}
		dbg_msg("fent", "stuck rewind steps=%d at=%.1f %.1f variant=%d", 10, StuckEndPos.x, StuckEndPos.y, m_LastChosenVariant);
		BeginPlanning();
		return;
	}
	if(m_ReplanCountdown <= 0)
		BeginPlanning();

	if(IsFinishReached())
	{
		m_Finished = true;
		m_Running = false;
		if(!m_Saved && SaveReplay())
		{
			m_Saved = true;
			GameClient()->m_MalinkaVisuals.PushNotification(SWarning(GetAvoidRuntimeLabel(), "Replay saved."));
		}
	}
}

bool CMalinkaFent::SaveReplay() const
{
	if(m_vFrames.size() < 2 || !GameClient()->Map()->IsLoaded())
		return false;
	// The planner keeps an initial-state sentinel followed by post-tick
	// positions. Export explicit pre-input positions in the Malinka format.
	std::vector<SFrame> vExport;
	vExport.reserve(m_vFrames.size() - 1);
	for(size_t Index = 1; Index < m_vFrames.size(); ++Index)
	{
		SFrame Frame = m_vFrames[Index];
		Frame.m_Tick = static_cast<int>(Index - 1);
		Frame.m_Pos = m_vFrames[Index - 1].m_Pos;
		Frame.m_HasDummy = m_vFrames[Index - 1].m_HasDummy;
		Frame.m_DummyPos = m_vFrames[Index - 1].m_DummyPos;
		Frame.m_DummyInput = m_CapturedDummyInput;
		vExport.push_back(Frame);
	}
	char aMapSha256[SHA256_MAXSTRSIZE], aTimestamp[64];
	sha256_str(GameClient()->Map()->Sha256(), aMapSha256, sizeof(aMapSha256));
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), "%Y%m%d_%H%M%S");
	const Malinka::Replay::SMetadata Meta{GameClient()->Map()->FullName(), aMapSha256, Malinka::Replay::PHYSICS, false};
	const auto Root = Malinka::Replay::Root(Storage());
	const auto Dir = Root / "fent";
	const auto Path = Dir / (SanitizeFileName(GameClient()->Map()->BaseName()) + "_" + aTimestamp + ".tas");
	if(!Malinka::Replay::WithinRoot(Root, Path) ||
		!Malinka::Replay::WithinRoot(Root, std::filesystem::path(Path).concat(".new")) ||
		!Malinka::Replay::WithinRoot(Root, std::filesystem::path(Path).concat(".bak")))
		return false;
	std::error_code Ec;
	std::filesystem::create_directories(Dir, Ec);
	std::string Error;
	return !Ec && Malinka::Replay::Write(Path, vExport, Meta, Error);
}

bool CMalinkaFent::SaveCurrentReplay()
{
	if(!SaveReplay())
	{
		GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Fent", "Failed to save replay."));
		return false;
	}

	GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Fent", "Replay saved."));
	return true;
}

vec2 CMalinkaFent::GetInterpolatedRenderPos() const
{
	vec2 RenderPos = mix(m_LastRenderPos, m_CurRenderPos, std::clamp((float)m_Accumulator, 0.0f, 1.0f));
	if(length(RenderPos) < 0.01f)
		RenderPos = m_CurRenderPos;
	return RenderPos;
}


void CMalinkaFent::OnUpdate() try
{
	const bool IdentityMatches = !m_Running || (m_PrimarySlot == g_Config.m_ClDummy &&
		m_aSimIds[m_PrimarySlot] == GameClient()->m_aLocalIds[m_PrimarySlot] &&
		(m_aSimIds[!m_PrimarySlot] < 0 || (Client()->DummyConnected() && m_aSimIds[!m_PrimarySlot] == GameClient()->m_aLocalIds[!m_PrimarySlot])));
	const bool ShouldRun = IdentityMatches && Client()->State() == IClient::STATE_ONLINE && IsFentModeActive() && g_Config.m_MalinkaAvoidfreeze != 0 && !GameClient()->m_FastPractice.Enabled() && !GameClient()->m_MalinkaTas.IsRecording() && !GameClient()->m_MalinkaTas.IsPlaying();
	if(ShouldRun && !m_Running && !m_Finished)
		StartRun();
	else if(!ShouldRun && (m_Running || m_Finished || m_WorldInitialized))
		StopRun(false);
	else if(m_Running)
	{
		const int64_t Now = time_get();
		if(m_LastUpdateTime == 0)
			m_LastUpdateTime = Now;
		const double Delta = std::clamp((double)(Now - m_LastUpdateTime) / (double)time_freq(), 0.0, 0.1);
		m_LastUpdateTime = Now;
		m_Accumulator += Delta * 50.0;

		if(m_StartDelayFrames > 0)
		{
			--m_StartDelayFrames;
			return;
		}

		if(!m_Planning && m_vPlannedFrames.empty())
			BeginPlanning();

		const bool WasPlanning = m_Planning;
		if(m_Planning)
			RunPlanningSlice();
		if(WasPlanning && !m_Planning)
			return;

		if(m_Accumulator >= 1.0 && m_Running && !m_Planning)
		{
			m_Accumulator = std::min(m_Accumulator - 1.0, 1.0);
			AdvanceOneStep();
		}
	}
}

catch(const std::exception &)
{
	m_Running = false;
	m_Finished = true;
	m_Planning = false;
	ShutdownWorld();
	GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Fent", "Allocation/filesystem failure; local run stopped."));
}

void CMalinkaFent::RenderPath()
{
	const bool RenderPathEnabled = g_Config.m_MalinkaAvoidFentRenderPath != 0;
	if(!RenderPathEnabled || m_StartDelayFrames > 0)
		return;

	std::vector<vec2> vPath = m_vTrailPositions;
	if(vPath.size() < 2)
		return;

	std::vector<IGraphics::CLineItem> vTrailSegments;
	vTrailSegments.reserve(vPath.size());
	for(size_t i = 1; i < vPath.size(); ++i)
	{
		const vec2 P0 = vPath[i - 1];
		const vec2 P1 = vPath[i];
		if(distance(P0, P1) < 0.01f)
			continue;
		vTrailSegments.emplace_back(P0.x, P0.y, P1.x, P1.y);
	}

	RenderFentLines(Graphics(), vTrailSegments, ColorRGBA(0.95f, 0.7f, 0.15f, 0.38f), 1.05f, GameClient()->m_Camera.m_Zoom);

	if(m_vBestPreviewPath.size() >= 2)
	{
		std::vector<IGraphics::CLineItem> vPlannedSegments;
		vPlannedSegments.reserve(m_vBestPreviewPath.size());
		vec2 Prev = GetInterpolatedRenderPos();
		for(const vec2 &Pos : m_vBestPreviewPath)
		{
			if(distance(Prev, Pos) >= 0.01f)
				vPlannedSegments.emplace_back(Prev.x, Prev.y, Pos.x, Pos.y);
			Prev = Pos;
		}
		RenderFentLines(Graphics(), vPlannedSegments, ColorRGBA(1.0f, 0.9f, 0.35f, 0.78f), 0.9f, GameClient()->m_Camera.m_Zoom);
	}
}

void CMalinkaFent::RenderMarker()
{
	if((!m_Running && !m_Finished) || !m_WorldInitialized || m_StartDelayFrames > 0)
		return;

	vec2 RenderPos = GetInterpolatedRenderPos();

	const SFentScreenPixelGrid Grid = GetFentScreenPixelGrid(Graphics());
	const float Size = SnapFentSpan(20.0f, std::max(Grid.m_PixelSizeX, Grid.m_PixelSizeY));
	const vec2 SnappedPos = SnapFentPoint(Grid, RenderPos);
	if(!GameClient()->m_GameSkin.m_SpriteWeaponGrenadeProjectile.IsValid())
		return;
	Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponGrenadeProjectile);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.95f);
	Graphics()->QuadsSetRotation((float)(Client()->LocalTime() * 3.5f));
	IGraphics::CQuadItem Quad(SnappedPos.x - Size * 0.5f, SnappedPos.y - Size * 0.5f, Size, Size);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->QuadsEnd();
}

void CMalinkaFent::RenderFinishTiles()
{
	if(m_vFinishTiles.empty() || m_StartDelayFrames > 0)
		return;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 0.12f, 0.12f, 0.95f);
	for(const vec2 &Pos : m_vFinishTiles)
	{
		const float Half = 16.0f;
		const float Thickness = 2.0f;
		const IGraphics::CQuadItem Top(Pos.x - Half, Pos.y - Half, Half * 2.0f, Thickness);
		const IGraphics::CQuadItem Bottom(Pos.x - Half, Pos.y + Half - Thickness, Half * 2.0f, Thickness);
		const IGraphics::CQuadItem Left(Pos.x - Half, Pos.y - Half, Thickness, Half * 2.0f);
		const IGraphics::CQuadItem Right(Pos.x + Half - Thickness, Pos.y - Half, Thickness, Half * 2.0f);
		Graphics()->QuadsDrawTL(&Top, 1);
		Graphics()->QuadsDrawTL(&Bottom, 1);
		Graphics()->QuadsDrawTL(&Left, 1);
		Graphics()->QuadsDrawTL(&Right, 1);
	}
	Graphics()->QuadsEnd();
}

void CMalinkaFent::RenderPathfinding()
{
	const bool RenderFieldEnabled = g_Config.m_MalinkaAvoidFentRenderPathfinding != 0;
	if(!RenderFieldEnabled || !m_FieldReady || m_FieldWidth <= 0 || m_FieldHeight <= 0 || Collision() == nullptr || m_StartDelayFrames > 0)
		return;
	const int Total = m_FieldWidth * m_FieldHeight;
	if((int)m_vFieldDirX.size() != Total || (int)m_vFieldDirY.size() != Total)
		return;

	std::vector<IGraphics::CLineItem> vSegments;
	vSegments.reserve((size_t)Total / 4);
	for(int Index = 0; Index < Total; ++Index)
	{
		const int8_t DirX = m_vFieldDirX[Index];
		const int8_t DirY = m_vFieldDirY[Index];
		if(DirX == 0 && DirY == 0)
			continue;
		const vec2 Pos = Collision()->GetPos(Index);
		const vec2 End = Pos + vec2((float)DirX, (float)DirY) * 7.0f;
		vSegments.emplace_back(Pos.x, Pos.y, End.x, End.y);
	}
	RenderFentLines(Graphics(), vSegments, ColorRGBA(0.9f, 0.2f, 0.2f, 0.18f), 1.0f, GameClient()->m_Camera.m_Zoom);
}

void CMalinkaFent::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	if(!m_Running && !m_Finished)
		return;
	if(!m_WorldInitialized)
		return;
	if(m_StartDelayFrames > 0)
		return;

	Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->m_Camera.m_Zoom);

	RenderPath();
	RenderMarker();
	RenderPathfinding();
	RenderFinishTiles();
}
