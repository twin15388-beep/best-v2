// The complete set of g_Config reads in the pinned prediction sources.
#pragma once
#define MALINKA_PREDICTION_CONFIG(X) \
    X(ClAntiPingPlayers) X(ClPredictEvents) X(SndEnable) \
    X(SvDeepfly) X(SvDraggerRange) X(SvFreezeDelay) X(SvHit) \
    X(SvOldTeleportHook) X(SvOldTeleportWeapons) X(SvTeam) \
    X(SvTeleportHoldHook) X(TcUnfreezeLagDelayTicks) X(ClPredictFreeze)
