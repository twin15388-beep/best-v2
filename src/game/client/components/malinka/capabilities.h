// Explicit missing consumers. Do not advertise these settings as working.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_CAPABILITIES_H
#define GAME_CLIENT_COMPONENTS_MALINKA_CAPABILITIES_H

#include <engine/shared/config.h>

#include <cstddef>

namespace Malinka
{
struct SUnavailableControl
{
	const void *m_pValue;
	const char *m_pConfigName;
	const char *m_pReason;
};
inline const SUnavailableControl *UnavailableControls(size_t &Count)
{
	static const SUnavailableControl aControls[] = {
		{&g_Config.m_MalinkaAimbotGrenadeMovePrediction, "MalinkaAimbotGrenadeMovePrediction", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaAutoEdgeDetectDeath, "MalinkaAutoEdgeDetectDeath", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoEdgeDetectFreeze, "MalinkaAutoEdgeDetectFreeze", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoEdgeDetectTele, "MalinkaAutoEdgeDetectTele", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoEdgeDetectionRange, "MalinkaAutoEdgeDetectionRange", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoEdgeExplorationDepth, "MalinkaAutoEdgeExplorationDepth", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoEdgePopulationSize, "MalinkaAutoEdgePopulationSize", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoEdgeSequenceLength, "MalinkaAutoEdgeSequenceLength", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoEdgeTopK, "MalinkaAutoEdgeTopK", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAutoJumpSaveMode, "MalinkaAutoJumpSaveMode", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantAfkProtection, "MalinkaAvoidBlatantAfkProtection", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantAfkTime, "MalinkaAvoidBlatantAfkTime", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantAimbotMode, "MalinkaAvoidBlatantAimbotMode", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantAimbotSegments, "MalinkaAvoidBlatantAimbotSegments", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantCheckTicks, "MalinkaAvoidBlatantCheckTicks", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantDeath, "MalinkaAvoidBlatantDeath", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantDirection, "MalinkaAvoidBlatantDirection", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantKickInTicks, "MalinkaAvoidBlatantKickInTicks", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantNsif, "MalinkaAvoidBlatantNsif", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantPlayerPrediction, "MalinkaAvoidBlatantPlayerPrediction", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaAvoidBlatantSafeAimTracking, "MalinkaAvoidBlatantSafeAimTracking", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantTeles, "MalinkaAvoidBlatantTeles", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantTrackPoint, "MalinkaAvoidBlatantTrackPoint", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantUnfreeze, "MalinkaAvoidBlatantUnfreeze", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantUnfreezeTicks, "MalinkaAvoidBlatantUnfreezeTicks", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantVisualAimbot, "MalinkaAvoidBlatantVisualAimbot", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidBlatantVisualTrackPoint, "MalinkaAvoidBlatantVisualTrackPoint", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidFentPlayerPrediction, "MalinkaAvoidFentPlayerPrediction", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaAvoidFentTicks, "MalinkaAvoidFentTicks", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidFentUnfreeze, "MalinkaAvoidFentUnfreeze", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidFentUnfreezeRadius, "MalinkaAvoidFentUnfreezeRadius", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidLegitHookPriority, "MalinkaAvoidLegitHookPriority", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidLegitLifePriority, "MalinkaAvoidLegitLifePriority", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidLegitPlayerPrediction, "MalinkaAvoidLegitPlayerPrediction", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaAvoidTileAutoDrag, "MalinkaAvoidTileAutoDrag", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaAvoidTileTrackPoints, "MalinkaAvoidTileTrackPoints", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaChangeNameOnFinish, "MalinkaChangeNameOnFinish", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaChangeNameOnFinishName, "MalinkaChangeNameOnFinishName", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaChangeNameOnFinishRandom, "MalinkaChangeNameOnFinishRandom", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaCursorInSpec, "MalinkaCursorInSpec", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaDiscordRpc, "MalinkaDiscordRpc", "Malinka RPC profiles are not connected; application IDs are unverified."},
		{&g_Config.m_MalinkaDiscordRpcIdCustom, "MalinkaDiscordRpcIdCustom", "Malinka RPC profiles are not connected; application IDs are unverified."},
		{&g_Config.m_MalinkaDiscordRpcMode, "MalinkaDiscordRpcMode", "Malinka RPC profiles are not connected; application IDs are unverified."},
		{&g_Config.m_MalinkaDummyFly, "MalinkaDummyFly", "Only dummy hammer was present in the donor; fly logic is incomplete."},
		{&g_Config.m_MalinkaFancyChatFont, "MalinkaFancyChatFont", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaFastInput, "MalinkaFastInput", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaFastInputOthers, "MalinkaFastInputOthers", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaGhostMove, "MalinkaGhostMove", "Packet-level ghost move is not implemented."},
		{&g_Config.m_MalinkaGhostMoveDirection, "MalinkaGhostMoveDirection", "Packet-level ghost move is not implemented."},
		{&g_Config.m_MalinkaGhostMoveHook, "MalinkaGhostMoveHook", "Packet-level ghost move is not implemented."},
		{&g_Config.m_MalinkaGhostMoveHookClosest, "MalinkaGhostMoveHookClosest", "Packet-level ghost move is not implemented."},
		{&g_Config.m_MalinkaGhostMoveJump, "MalinkaGhostMoveJump", "Packet-level ghost move is not implemented."},
		{&g_Config.m_MalinkaHideBubble, "MalinkaHideBubble", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaHookRide, "MalinkaHookRide", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaIgnoreReplayWarnings, "MalinkaIgnoreReplayWarnings", "Checksum, identity and physics failures cannot be ignored."},
		{&g_Config.m_MalinkaJetRide, "MalinkaJetRide", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaModDetector, "MalinkaModDetector", "No verified moderator/warning lists supplied; name guesses are disabled."},
		{&g_Config.m_MalinkaModDetectorLeave, "MalinkaModDetectorLeave", "No verified moderator/warning lists supplied; name guesses are disabled."},
		{&g_Config.m_MalinkaModDetectorNames, "MalinkaModDetectorNames", "No verified moderator/warning lists supplied; name guesses are disabled."},
		{&g_Config.m_MalinkaModDetectorWarn, "MalinkaModDetectorWarn", "No verified moderator/warning lists supplied; name guesses are disabled."},
		{&g_Config.m_MalinkaModDetectorWarnLeave, "MalinkaModDetectorWarnLeave", "No verified moderator/warning lists supplied; name guesses are disabled."},
		{&g_Config.m_MalinkaPredictionDeathTile, "MalinkaPredictionDeathTile", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaPredictionHeart, "MalinkaPredictionHeart", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaPredictionMoveRestriction, "MalinkaPredictionMoveRestriction", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaPredictionPlayers, "MalinkaPredictionPlayers", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaSendAds, "MalinkaSendAds", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaSpoofVersion, "MalinkaSpoofVersion", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaSpoofVersionNr, "MalinkaSpoofVersionNr", "Not implemented in this adaptation yet; see docs/MALINKA_STATUS.md."},
		{&g_Config.m_MalinkaTasDrawPredictionPathColorOthers, "MalinkaTasDrawPredictionPathColorOthers", "This prediction control has no runtime consumer yet."},
		{&g_Config.m_MalinkaTasFakeAim, "MalinkaTasFakeAim", "Replay aim transformation and equivalence validation are not implemented."},
		{&g_Config.m_MalinkaTasFakeAimMode, "MalinkaTasFakeAimMode", "Replay aim transformation and equivalence validation are not implemented."},
		{&g_Config.m_MalinkaTasReplayVaultAutoSync, "MalinkaTasReplayVaultAutoSync", "Replay vault synchronization is not implemented."},
	};
	Count = sizeof(aControls) / sizeof(aControls[0]);
	return aControls;
}
inline const char *UnavailableReason(const void *pValue)
{
	size_t Count;
	const auto *pControls = UnavailableControls(Count);
	for(size_t Index = 0; Index < Count; ++Index)
		if(pControls[Index].m_pValue == pValue)
			return pControls[Index].m_pReason;
	return nullptr;
}
}

#endif
