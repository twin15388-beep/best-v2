/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_CONTROLS_H

#include <base/vmath.h>

#include <engine/client.h>
#include <engine/console.h>

#include <generated/protocol.h>

#include <game/client/component.h>
#include <game/client/components/malinka/input_helpers.h>

class CControls : public CComponent
{
public:
	float GetMinMouseDistance() const;
	float GetMaxMouseDistance() const;

	enum class EMouseInputType
	{
		ABSOLUTE,
		RELATIVE,
		AUTOMATED,
	};

	vec2 m_aMousePos[NUM_DUMMIES];
	vec2 m_aMousePosOnAction[NUM_DUMMIES];
	vec2 m_aTargetPos[NUM_DUMMIES];

	EMouseInputType m_aMouseInputType[NUM_DUMMIES];

	int m_aAmmoCount[NUM_WEAPONS];

	int64_t m_LastSendTime;
	CNetObj_PlayerInput m_aInputData[NUM_DUMMIES];
	CNetObj_PlayerInput m_aLastData[NUM_DUMMIES];
	int m_aInputDirectionLeft[NUM_DUMMIES];
	int m_aInputDirectionRight[NUM_DUMMIES];
	int m_aShowHookColl[NUM_DUMMIES];

	// TClient
	CNetObj_PlayerInput m_aFastInput[NUM_DUMMIES];
	bool m_FastInputHookAction = false;
	bool m_FastInputFireAction = false;

	int m_aRawFireState[NUM_DUMMIES] = {};
	int m_aRawJumpState[NUM_DUMMIES] = {};
	int m_aRawHookState[NUM_DUMMIES] = {};
	// Malinka misc runtime state
	int m_aMalinkaLastHookState[NUM_DUMMIES];
	int m_aMalinkaMoonwalkFlip[NUM_DUMMIES];
	int64_t m_aMalinkaInputActivity[NUM_DUMMIES];
	int64_t m_aMalinkaAntiAfkPulseEnd[NUM_DUMMIES];
	int64_t m_aMalinkaLastAntiAfkPulse[NUM_DUMMIES];
	int64_t m_aMalinkaLastRehook[NUM_DUMMIES];
	bool m_aMalinkaPendingRehookRelease[NUM_DUMMIES];
	bool m_aMalinkaAssistAimActive[NUM_DUMMIES];
	int m_aMalinkaAssistAimWeapon[NUM_DUMMIES];
	vec2 m_aMalinkaAssistAimPos[NUM_DUMMIES];
	bool m_aMalinkaGhostMoveWarned[NUM_DUMMIES];
	bool m_aMalinkaRenderMouseOverride[NUM_DUMMIES];
	int m_aMalinkaRenderMouseTargetId[NUM_DUMMIES];
	vec2 m_aMalinkaVisibleMousePos[NUM_DUMMIES];
	vec2 m_aMalinkaSentMousePos[NUM_DUMMIES];
	float m_aMalinkaFakeAimAngle[NUM_DUMMIES];
	int64_t m_aMalinkaFakeAimLastRandomUpdate[NUM_DUMMIES];

	Malinka::CInputStream m_aMalinkaNetworkStream[NUM_DUMMIES];
	bool m_aMalinkaInjectedHook[NUM_DUMMIES]{};
	bool m_aMalinkaInjectedJump[NUM_DUMMIES]{};
	void RestoreMalinkaButtons(int Dummy);
	void ResetMalinkaInput(int Dummy);
	void ApplyMalinkaInput(vec2 Pos);
	vec2 EnsureValidAim(vec2 Pos);
	vec2 GetRenderMousePos(int Dummy);
	void TriggerFireTap(int Dummy);
	bool IsMouseMoved(int LocalPlayerId) const;
	bool IsPlayerActive(int LocalPlayerId) const;

	CControls();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	void OnConsoleInit() override;
	virtual void OnPlayerDeath();

	int SnapInput(int *pData);
	void ClampMousePos();
	void ResetInput(int Dummy);
	bool CheckNewInput();

	// bestclient
	void InputDebounce(int Dummy, int Input);
	bool IsInputDebounceAllowed() const;
	bool IsInputDebounceBlocked() const;
	struct CInputDebounceState
	{
		int m_Direction = 0;
		int m_Jump = 0;
		int m_Hook = 0;
	};
	CInputDebounceState m_aInputDebounce[NUM_DUMMIES];
	// bestclient

private:
	// bestclient
	enum
	{
		INPUT_LEFT,
		INPUT_RIGHT,
		INPUT_JUMP,
		INPUT_HOOK,
	};
	void ApplyInputDebounce(int Dummy, CNetObj_PlayerInput &Input) const;
	// bestclient

	static void ConKeyInputState(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputSet(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData);
};
#endif
