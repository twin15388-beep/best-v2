// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#include "replay_paths.h"
#include "capabilities.h"

#include <base/time.h>
#include <algorithm>
#include <base/math.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
constexpr float gs_MalinkaFontSize = 14.0f;
constexpr float gs_MalinkaHeadlineFontSize = 20.0f;
constexpr float gs_MalinkaHeadlineHeight = gs_MalinkaHeadlineFontSize;
constexpr float gs_MalinkaLineSize = 20.0f;
constexpr float gs_MalinkaColorPickerLineSize = 25.0f;
constexpr float gs_MalinkaColorPickerLabelSize = 13.0f;
constexpr float gs_MalinkaColorPickerSpacing = 5.0f;
constexpr float gs_MalinkaMargin = 10.0f;
constexpr float gs_MalinkaMarginSmall = 5.0f;
constexpr float gs_MalinkaSectionSpacing = 30.0f;
constexpr float gs_MalinkaColumnSpacing = 30.0f;
constexpr float gs_MalinkaSectionBoxPadding = 20.0f;

enum
{
	MALINKA_TAB_AIMBOT = 0,
	MALINKA_TAB_MISC,
	MALINKA_TAB_VISUALS,
	MALINKA_TAB_AVOID,
	MALINKA_TAB_TAS,
	MALINKA_TAB_SETTINGS,
	NUM_MALINKA_TABS,
};

struct CMalinkaScrollState
{
	CScrollRegion m_ScrollRegion;
	std::vector<CUIRect> m_vSectionBoxes;
	vec2 m_PrevScrollOffset = vec2(0.0f, 0.0f);
};

struct SBoolEntry
{
	const char *m_pLabel;
	int *m_pValue;
};

bool MalinkaUnavailable(CMenus *pMenus, CUIRect &View, const char *pLabel, const void *pValue)
{
	const char *pReason = Malinka::UnavailableReason(pValue);
	if(!pReason)
		return false;
	CUIRect Row;
	View.HSplitTop(gs_MalinkaLineSize, &Row, &View);
	char aLabel[256];
	str_format(aLabel, sizeof(aLabel), "%s [not implemented]", pLabel);
	pMenus->MenuUi()->DoLabel(&Row, aLabel, gs_MalinkaFontSize * 0.85f, TEXTALIGN_ML);
	pMenus->MalinkaTooltip(pValue, &Row, pReason);
	return true;
}

int DoMalinkaCheckBox(CMenus *pMenus, const void *pId, const char *pText, int *pValue, CUIRect *pRect, float VMargin)
{
	if(MalinkaUnavailable(pMenus, *pRect, pText, pValue))
		return 0;
	return pMenus->DoButton_CheckBoxAutoVMarginAndSet(pId, pText, pValue, pRect, VMargin);
}

ColorRGBA GetMalinkaAccentColor()
{
	return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MalinkaRainbowMenu ? g_Config.m_UiColor : g_Config.m_MalinkaCustomAccentColor, true));
}

ColorRGBA GetMalinkaBackgroundColor()
{
	ColorRGBA Bg = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MalinkaCustomBgColor, true));
	Bg.a = 1.0f;
	return Bg;
}

ColorRGBA GetMalinkaPanelColor(float AccentAmount, float Alpha)
{
	if(!g_Config.m_MalinkaCustomTheme)
		return ColorRGBA(0.0f, 0.0f, 0.0f, Alpha);

	const ColorRGBA Bg = GetMalinkaBackgroundColor();
	const ColorRGBA Accent = GetMalinkaAccentColor();
	return ColorRGBA(
		mix(Bg.r, Accent.r, AccentAmount),
		mix(Bg.g, Accent.g, AccentAmount),
		mix(Bg.b, Accent.b, AccentAmount),
		Alpha);
}

template<size_t N>
bool DoMalinkaDropDown(CMenus *pMenus, CUIRect &View, const char *pLabel, int &Value, const std::array<const char *, N> &aNames, CUi::SDropDownState &State, CScrollRegion &ScrollRegion, float LabelWidth = 170.0f)
{
	if(MalinkaUnavailable(pMenus, View, pLabel, &Value))
		return false;
	State.m_SelectionPopupContext.m_pScrollRegion = &ScrollRegion;
	CUIRect Row, Label, DropDown;
	View.HSplitTop(gs_MalinkaLineSize, &Row, &View);
	Row.VSplitLeft(LabelWidth, &Label, &DropDown);
	pMenus->MenuUi()->DoLabel(&Label, pLabel, gs_MalinkaFontSize, TEXTALIGN_ML);
	auto aDropDownNames = aNames;
	const int NewValue = pMenus->MenuUi()->DoDropDown(&DropDown, Value, aDropDownNames.data(), aDropDownNames.size(), State);
	if(NewValue != Value)
	{
		Value = NewValue;
		return true;
	}
	return false;
}

bool DoMalinkaDropDownDynamic(CMenus *pMenus, CUIRect &View, const char *pLabel, int &Value, const std::vector<const char *> &vNames, CUi::SDropDownState &State, CScrollRegion &ScrollRegion, float LabelWidth = 170.0f)
{
	if(MalinkaUnavailable(pMenus, View, pLabel, &Value))
		return false;
	State.m_SelectionPopupContext.m_pScrollRegion = &ScrollRegion;
	CUIRect Row, Label, DropDown;
	View.HSplitTop(gs_MalinkaLineSize, &Row, &View);
	Row.VSplitLeft(LabelWidth, &Label, &DropDown);
	pMenus->MenuUi()->DoLabel(&Label, pLabel, gs_MalinkaFontSize, TEXTALIGN_ML);
	std::vector<const char *> vDropDownNames = vNames;
	const int NewValue = pMenus->MenuUi()->DoDropDown(&DropDown, Value, vDropDownNames.empty() ? nullptr : &vDropDownNames[0], vDropDownNames.size(), State);
	if(NewValue != Value)
	{
		Value = NewValue;
		return true;
	}
	return false;
}

static void BeginMalinkaScrollColumns(CUIRect MainView, CMalinkaScrollState &State, CUIRect &LeftView, CUIRect &RightView);
static void EndMalinkaScrollColumns(CUIRect MainView, CMalinkaScrollState &State, const CUIRect &LeftView, const CUIRect &RightView);
static size_t BeginMalinkaSection(CMenus *pMenus, CUIRect &Column, CMalinkaScrollState &State, const char *pTitle);
static void FinishMalinkaSection(CUIRect &Column, CMalinkaScrollState &State, size_t Index);
static void DoMalinkaCheckBoxes(CMenus *pMenus, CUIRect &View, const SBoolEntry *pEntries, size_t Count);
static void DoMalinkaSlider(CMenus *pMenus, CUIRect &View, int *pValue, const char *pLabel, int Min, int Max, const char *pSuffix = "", const char *pTooltip = nullptr);
static bool DoMalinkaButton(CMenus *pMenus, CUIRect &View, CButtonContainer &ButtonId, const char *pLabel, ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f));
static void DoMalinkaEditBox(CMenus *pMenus, CUIRect &View, CLineInput *pInput, const char *pLabel, const char *pDefault, char *pBuf, size_t BufSize);
static void DoMalinkaColorPicker(CMenus *pMenus, CUIRect &View, CButtonContainer &ResetId, const char *pLabel, unsigned *pColorValue, ColorRGBA DefaultColor);

struct SMalinkaWeaponConfig
{
	const char *m_pTitle;
	int *m_pEnable;
	int *m_pFov;
	int *m_pPriority;
	int *m_pSilent;
	int *m_pIgnoreFriends;
	int *m_pAccuracy;
};

static void RenderMalinkaWeaponSection(CMenus *pMenus, CUIRect &Column, CMalinkaScrollState &State, const SMalinkaWeaponConfig &Weapon, bool First);
static void RenderMalinkaGradientSection(CMenus *pMenus, CUIRect &Column, CMalinkaScrollState &State, const char *pTitle, int *pEnable, int *pSegmented, int *pSpeed, unsigned *pStartColor, unsigned *pEndColor, int *pRainbow, bool First);

void RenderMalinkaAimbot(CMenus *pMenus, CUIRect MainView);
void RenderMalinkaMisc(CMenus *pMenus, CUIRect MainView);
void RenderMalinkaVisuals(CMenus *pMenus, CUIRect MainView);
void RenderMalinkaAvoid(CMenus *pMenus, CUIRect MainView);
void RenderMalinkaTas(CMenus *pMenus, CUIRect MainView);
void RenderMalinkaSettings(CMenus *pMenus, CUIRect MainView);
}

void CMenus::MalinkaTooltip(const void *pId, const CUIRect *pRect, const char *pText)
{
	GameClient()->m_Tooltips.DoToolTip(pId, pRect, pText, 360.0f);
}

void CMenus::RenderSettingsMalinka(CUIRect MainView)
{
	if(GameClient()->m_MalinkaSession.IsHidden())
		return;
	CUIRect StatusLine;
	MainView.HSplitTop(18.0f, &StatusLine, &MainView);
	Ui()->DoLabel(&StatusLine, "Experimental adaptation, not a complete KRX port. Disabled controls are marked; F1: malinka_status", 11.0f, TEXTALIGN_ML);
	MainView.HSplitTop(18.0f, &StatusLine, &MainView);
	Ui()->DoLabel(&StatusLine, "Insert hides this tab until process restart; it does not disable features. Remote official badge is not authorized.", 11.0f, TEXTALIGN_ML);
	static int s_CurTab = MALINKA_TAB_AIMBOT;
	static CButtonContainer s_aTabButtons[NUM_MALINKA_TABS];
	const std::array<const char *, NUM_MALINKA_TABS> aTabNames = {
		Localize("Aimbot"),
		Localize("Misc"),
		Localize("Visuals & HUD"),
		Localize("Avoid"),
		"TAS",
		Localize("Settings"),
	};

	CUIRect TabBar, Button;
	MainView.HSplitTop(gs_MalinkaLineSize, &TabBar, &MainView);
	const float TabWidth = TabBar.w / static_cast<float>(NUM_MALINKA_TABS);
	for(int i = 0; i < NUM_MALINKA_TABS; ++i)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = i == 0 ? IGraphics::CORNER_L : i == NUM_MALINKA_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aTabButtons[i], aTabNames[i], s_CurTab == i, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
			s_CurTab = i;
	}

	MainView.HSplitTop(gs_MalinkaMargin, nullptr, &MainView);

	if(s_CurTab == MALINKA_TAB_AIMBOT)
		RenderMalinkaAimbot(this, MainView);
	else if(s_CurTab == MALINKA_TAB_MISC)
		RenderMalinkaMisc(this, MainView);
	else if(s_CurTab == MALINKA_TAB_VISUALS)
		RenderMalinkaVisuals(this, MainView);
	else if(s_CurTab == MALINKA_TAB_AVOID)
		RenderMalinkaAvoid(this, MainView);
	else if(s_CurTab == MALINKA_TAB_TAS)
		RenderMalinkaTas(this, MainView);
	else
		RenderMalinkaSettings(this, MainView);
}

namespace
{
static void BeginMalinkaScrollColumns(CUIRect MainView, CMalinkaScrollState &State, CUIRect &LeftView, CUIRect &RightView)
{
	const float OriginalY = MainView.y;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_ForceShowScrollbar = true;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	State.m_ScrollRegion.Begin(&MainView, &ScrollParams);
	const vec2 ScrollOffset(0.0f, MainView.y - OriginalY);
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, gs_MalinkaColumnSpacing);
	LeftView.VSplitLeft(gs_MalinkaMarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(gs_MalinkaMarginSmall, &RightView, nullptr);

	for(CUIRect &Section : State.m_vSectionBoxes)
	{
		Section.w += gs_MalinkaSectionBoxPadding;
		Section.h += gs_MalinkaSectionBoxPadding;
		Section.x -= gs_MalinkaSectionBoxPadding * 0.5f;
		Section.y -= gs_MalinkaSectionBoxPadding * 0.5f;
		Section.y -= State.m_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(GetMalinkaPanelColor(0.08f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	}

	State.m_PrevScrollOffset = ScrollOffset;
	State.m_vSectionBoxes.clear();
}

static void EndMalinkaScrollColumns(CUIRect MainView, CMalinkaScrollState &State, const CUIRect &LeftView, const CUIRect &RightView)
{
	CUIRect ScrollRegionRect;
	ScrollRegionRect.x = MainView.x;
	float ContentBottom = std::max(LeftView.y, RightView.y);
	for(const CUIRect &Section : State.m_vSectionBoxes)
		ContentBottom = std::max(ContentBottom, Section.y + Section.h);
	ScrollRegionRect.y = ContentBottom + gs_MalinkaMarginSmall * 2.0f;
	ScrollRegionRect.w = MainView.w;
	ScrollRegionRect.h = 0.0f;
	State.m_ScrollRegion.AddRect(ScrollRegionRect);
	State.m_ScrollRegion.End();
}

static size_t BeginMalinkaSection(CMenus *pMenus, CUIRect &Column, CMalinkaScrollState &State, const char *pTitle)
{
	State.m_vSectionBoxes.push_back(Column);
	CUIRect Label;
	Column.HSplitTop(gs_MalinkaHeadlineHeight, &Label, &Column);
	pMenus->MenuUi()->DoLabel(&Label, pTitle, gs_MalinkaHeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(gs_MalinkaMarginSmall, nullptr, &Column);
	return State.m_vSectionBoxes.size() - 1;
}

static void FinishMalinkaSection(CUIRect &Column, CMalinkaScrollState &State, size_t Index)
{
	State.m_vSectionBoxes[Index].h = Column.y - State.m_vSectionBoxes[Index].y;
}

static void DoMalinkaCheckBoxes(CMenus *pMenus, CUIRect &View, const SBoolEntry *pEntries, size_t Count)
{
	for(size_t i = 0; i < Count; ++i)
		DoMalinkaCheckBox(pMenus, pEntries[i].m_pValue, pEntries[i].m_pLabel, pEntries[i].m_pValue, &View, gs_MalinkaLineSize);
}

static void DoMalinkaSlider(CMenus *pMenus, CUIRect &View, int *pValue, const char *pLabel, int Min, int Max, const char *pSuffix, const char *pTooltip)
{
	if(MalinkaUnavailable(pMenus, View, pLabel, pValue))
		return;
	CUIRect Button;
	View.HSplitTop(gs_MalinkaLineSize, &Button, &View);
	pMenus->MenuUi()->DoScrollbarOption(pValue, pValue, &Button, pLabel, Min, Max, &CUi::ms_LinearScrollbarScale, 0, pSuffix);
	if(pTooltip)
		pMenus->MalinkaTooltip(pValue, &Button, pTooltip);
}

static bool DoMalinkaButton(CMenus *pMenus, CUIRect &View, CButtonContainer &ButtonId, const char *pLabel, ColorRGBA Color)
{
	CUIRect Button;
	View.HSplitTop(gs_MalinkaLineSize, &Button, &View);
	if(g_Config.m_MalinkaCustomTheme && Color.r == 1.0f && Color.g == 1.0f && Color.b == 1.0f)
		Color = GetMalinkaPanelColor(0.20f, 0.62f);
	return pMenus->DoButtonLineSize_MenuHelper(&ButtonId, pLabel, 0, &Button, gs_MalinkaLineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, Color) != 0;
}

static void DoMalinkaEditBox(CMenus *pMenus, CUIRect &View, CLineInput *pInput, const char *pLabel, const char *pDefault, char *pBuf, size_t BufSize)
{
	if(MalinkaUnavailable(pMenus, View, pLabel, pBuf))
		return;
	CUIRect Row;
	View.HSplitTop(gs_MalinkaLineSize, &Row, &View);
	pMenus->DoEditBoxWithLabel(pInput, &Row, pLabel, pDefault, pBuf, BufSize);
}

static void DoMalinkaColorPicker(CMenus *pMenus, CUIRect &View, CButtonContainer &ResetId, const char *pLabel, unsigned *pColorValue, ColorRGBA DefaultColor)
{
	if(MalinkaUnavailable(pMenus, View, pLabel, pColorValue))
		return;
	pMenus->DoLine_ColorPickerHelper(&ResetId, gs_MalinkaColorPickerLineSize, gs_MalinkaColorPickerLabelSize, gs_MalinkaColorPickerSpacing, &View, pLabel, pColorValue, DefaultColor, false);
}

static void RenderMalinkaWeaponSection(CMenus *pMenus, CUIRect &Column, CMalinkaScrollState &State, const SMalinkaWeaponConfig &Weapon, bool First)
{
	if(!First)
		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t SectionIndex = BeginMalinkaSection(pMenus, Column, State, Weapon.m_pTitle);

	DoMalinkaCheckBox(pMenus, Weapon.m_pEnable, Localize("Enable"), Weapon.m_pEnable, &Column, gs_MalinkaLineSize);
	if(*Weapon.m_pEnable)
	{
		DoMalinkaSlider(pMenus, Column, Weapon.m_pFov, Localize("FOV"), 1, 360);
		static std::array<const char *, 2> s_aPriorityNames = {Localize("Closest to Crosshair"), Localize("Closest to Player")};
		static CUi::SDropDownState s_aPriorityStates[6];
		static CScrollRegion s_aPriorityScrollRegions[6];
		const int WeaponIndex =
			Weapon.m_pEnable == &g_Config.m_MalinkaAimbotHook ? 0 :
			Weapon.m_pEnable == &g_Config.m_MalinkaAimbotHammer ? 1 :
			Weapon.m_pEnable == &g_Config.m_MalinkaAimbotPistol ? 2 :
			Weapon.m_pEnable == &g_Config.m_MalinkaAimbotShotgun ? 3 :
			Weapon.m_pEnable == &g_Config.m_MalinkaAimbotGrenade ? 4 : 5;
		DoMalinkaDropDown(pMenus, Column, Localize("Target Priority"), *Weapon.m_pPriority, s_aPriorityNames, s_aPriorityStates[WeaponIndex], s_aPriorityScrollRegions[WeaponIndex]);
		DoMalinkaCheckBox(pMenus, Weapon.m_pSilent, Localize("Silent"), Weapon.m_pSilent, &Column, gs_MalinkaLineSize);
		DoMalinkaCheckBox(pMenus, Weapon.m_pIgnoreFriends, Localize("Ignore Friends"), Weapon.m_pIgnoreFriends, &Column, gs_MalinkaLineSize);
		if(Weapon.m_pAccuracy != nullptr)
			DoMalinkaSlider(pMenus, Column, Weapon.m_pAccuracy, Localize("Accuracy"), 1, 20);
	}

	FinishMalinkaSection(Column, State, SectionIndex);
}

static void RenderMalinkaGradientSection(CMenus *pMenus, CUIRect &Column, CMalinkaScrollState &State, const char *pTitle, int *pEnable, int *pSegmented, int *pSpeed, unsigned *pStartColor, unsigned *pEndColor, int *pRainbow, bool First)
{
	if(!First)
		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t SectionIndex = BeginMalinkaSection(pMenus, Column, State, pTitle);

	DoMalinkaCheckBox(pMenus, pEnable, Localize("Enable"), pEnable, &Column, gs_MalinkaLineSize);
	if(*pEnable)
	{
		DoMalinkaCheckBox(pMenus, pSegmented, Localize("Segmented"), pSegmented, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, pSpeed, Localize("Speed"), 0, 20);
		static CButtonContainer s_aStartIds[3];
		static CButtonContainer s_aEndIds[3];
		const int Index = pEnable == &g_Config.m_MalinkaWatermark ? 0 : 1;
		DoMalinkaColorPicker(pMenus, Column, s_aStartIds[Index], Localize("Start"), pStartColor, ColorRGBA(0.0f, 1.0f, 0.0f));
		DoMalinkaColorPicker(pMenus, Column, s_aEndIds[Index], Localize("End"), pEndColor, ColorRGBA(0.0f, 0.5f, 0.0f));
		DoMalinkaCheckBox(pMenus, pRainbow, Localize("Rainbow"), pRainbow, &Column, gs_MalinkaLineSize);
	}

	FinishMalinkaSection(Column, State, SectionIndex);
}

void RenderMalinkaAimbot(CMenus *pMenus, CUIRect MainView)
{
	static CMalinkaScrollState s_ScrollState;
	CUIRect LeftView, RightView;
	BeginMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);

	CUIRect Column = LeftView;
	const size_t MainSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Main"));
	static const SBoolEntry s_aMainChecks[] = {
		{Localize("Enable"), &g_Config.m_MalinkaAimbot},
		{Localize("Draw FOV"), &g_Config.m_MalinkaAimbotDrawFov},
		{Localize("Target Box"), &g_Config.m_MalinkaAimbotTargetBox},
		{Localize("Target Glow"), &g_Config.m_MalinkaAimbotTargetGlow},
		{Localize("Auto Shoot"), &g_Config.m_MalinkaAimbotAutoShoot},
		{Localize("Global Silent"), &g_Config.m_MalinkaAimbotSilent},
		{Localize("Global Ignore Friends"), &g_Config.m_MalinkaAimbotIgnoreFriends},
		{Localize("Advanced Settings"), &g_Config.m_MalinkaAimbotAdvancedSettings},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aMainChecks, std::size(s_aMainChecks));

	static std::array<const char *, 2> s_aPriorityNames = {Localize("Closest to Crosshair"), Localize("Closest to Player")};
	static CUi::SDropDownState s_GlobalPriorityState;
	static CScrollRegion s_GlobalPriorityScrollRegion;
	DoMalinkaDropDown(pMenus, Column, Localize("Target Priority"), g_Config.m_MalinkaAimbotTargetPriority, s_aPriorityNames, s_GlobalPriorityState, s_GlobalPriorityScrollRegion);
	if(g_Config.m_MalinkaAimbotAdvancedSettings)
	{
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAimbotGrenadeMovePrediction, Localize("Grenade Move Prediction"), &g_Config.m_MalinkaAimbotGrenadeMovePrediction, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAimbotEdgeAccuracy, Localize("Edge Accuracy"), 1, 20);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAimbotMinShootDelay, Localize("Min Shoot Delay"), 1, 30);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAimbotMaxShootDelay, Localize("Max Shoot Delay"), 1, 30);
	}
	FinishMalinkaSection(Column, s_ScrollState, MainSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t HotkeySection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Hotkeys"));
	static CButtonContainer s_AimbotReader;
	static CButtonContainer s_AimbotClear;
	static CButtonContainer s_AutoHookReader;
	static CButtonContainer s_AutoHookClear;
	static CButtonContainer s_AutoShootReader;
	static CButtonContainer s_AutoShootClear;
	pMenus->DoLine_KeyReader(Column, s_AimbotReader, s_AimbotClear, "Aimbot", "toggle malinka_aimbot 1 0");
	pMenus->DoLine_KeyReader(Column, s_AutoHookReader, s_AutoHookClear, "Aimbot Auto Hook", "+toggle malinka_aimbot_autohook_key 1 0");
	pMenus->DoLine_KeyReader(Column, s_AutoShootReader, s_AutoShootClear, "Aimbot Auto Shoot", "+toggle malinka_aimbot_autoshoot_key 1 0");
	FinishMalinkaSection(Column, s_ScrollState, HotkeySection);

	Column = RightView;
	const SMalinkaWeaponConfig aWeapons[] = {
		{"Hook", &g_Config.m_MalinkaAimbotHook, &g_Config.m_MalinkaAimbotHookFov, &g_Config.m_MalinkaAimbotHookTargetPriority, &g_Config.m_MalinkaAimbotHookSilent, &g_Config.m_MalinkaAimbotHookIgnoreFriends, &g_Config.m_MalinkaAimbotHookAccuracy},
		{"Hammer", &g_Config.m_MalinkaAimbotHammer, &g_Config.m_MalinkaAimbotHammerFov, &g_Config.m_MalinkaAimbotHammerTargetPriority, &g_Config.m_MalinkaAimbotHammerSilent, &g_Config.m_MalinkaAimbotHammerIgnoreFriends, nullptr},
		{"Pistol", &g_Config.m_MalinkaAimbotPistol, &g_Config.m_MalinkaAimbotPistolFov, &g_Config.m_MalinkaAimbotPistolTargetPriority, &g_Config.m_MalinkaAimbotPistolSilent, &g_Config.m_MalinkaAimbotPistolIgnoreFriends, nullptr},
		{"Shotgun", &g_Config.m_MalinkaAimbotShotgun, &g_Config.m_MalinkaAimbotShotgunFov, &g_Config.m_MalinkaAimbotShotgunTargetPriority, &g_Config.m_MalinkaAimbotShotgunSilent, &g_Config.m_MalinkaAimbotShotgunIgnoreFriends, nullptr},
		{"Grenade", &g_Config.m_MalinkaAimbotGrenade, &g_Config.m_MalinkaAimbotGrenadeFov, &g_Config.m_MalinkaAimbotGrenadeTargetPriority, &g_Config.m_MalinkaAimbotGrenadeSilent, &g_Config.m_MalinkaAimbotGrenadeIgnoreFriends, nullptr},
		{"Laser", &g_Config.m_MalinkaAimbotLaser, &g_Config.m_MalinkaAimbotLaserFov, &g_Config.m_MalinkaAimbotLaserTargetPriority, &g_Config.m_MalinkaAimbotLaserSilent, &g_Config.m_MalinkaAimbotLaserIgnoreFriends, nullptr},
	};
	for(size_t i = 0; i < std::size(aWeapons); ++i)
		RenderMalinkaWeaponSection(pMenus, Column, s_ScrollState, aWeapons[i], i == 0);

	EndMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);
}

void RenderMalinkaMisc(CMenus *pMenus, CUIRect MainView)
{
	static CMalinkaScrollState s_ScrollState;
	CUIRect LeftView, RightView;
	BeginMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);

	CUIRect Column = LeftView;
	const size_t BotsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Bots"));
	static const SBoolEntry s_aBotChecks[] = {
		{Localize("Moonwalk"), &g_Config.m_MalinkaMoonwalk},
		{Localize("Auto Fire"), &g_Config.m_MalinkaAutoFire},
		{Localize("Auto Rehook"), &g_Config.m_MalinkaAutoRehook},
		{Localize("Auto Jump Save"), &g_Config.m_MalinkaAutoJumpSave},
		{Localize("Quick Stop"), &g_Config.m_MalinkaQuickStop},
		{Localize("Avoid Freeze (Basic)"), &g_Config.m_MalinkaAvoidfreeze},
		{Localize("Jet Ride"), &g_Config.m_MalinkaJetRide},
		{Localize("Auto Aled"), &g_Config.m_MalinkaAutoAled},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aBotChecks, std::size(s_aBotChecks));
	FinishMalinkaSection(Column, s_ScrollState, BotsSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t ProtectionSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Protections"));
	static const SBoolEntry s_aProtectionChecks[] = {
		{Localize("Random Timeout Seed"), &g_Config.m_MalinkaRandomTimeoutSeed},
		{Localize("Version Spoofer"), &g_Config.m_MalinkaSpoofVersion},
		{Localize("Advanced Options"), &g_Config.m_MalinkaSpoofVersionAdvancedSettings},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aProtectionChecks, std::size(s_aProtectionChecks));
	if(g_Config.m_MalinkaSpoofVersion || g_Config.m_MalinkaSpoofVersionAdvancedSettings)
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaSpoofVersionNr, Localize("Version Nr"), 0, 99999);
	FinishMalinkaSection(Column, s_ScrollState, ProtectionSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t ModSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Mod Detector"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaModDetector, Localize("Enable"), &g_Config.m_MalinkaModDetector, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaModDetector)
	{
		static const SBoolEntry s_aModChecks[] = {
			{Localize("Detect by Names"), &g_Config.m_MalinkaModDetectorNames},
			{Localize("Detect Suspicious Players"), &g_Config.m_MalinkaModDetectorWarn},
			{Localize("Leave on Mod Detection"), &g_Config.m_MalinkaModDetectorLeave},
			{Localize("Leave on Warn Detected"), &g_Config.m_MalinkaModDetectorWarnLeave},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aModChecks, std::size(s_aModChecks));
	}
	FinishMalinkaSection(Column, s_ScrollState, ModSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t EdgeSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Auto Edge"));
	static const SBoolEntry s_aEdgeChecks[] = {
		{Localize("Enable"), &g_Config.m_MalinkaAutoEdge},
		{Localize("Detect Freeze"), &g_Config.m_MalinkaAutoEdgeDetectFreeze},
		{Localize("Detect Death"), &g_Config.m_MalinkaAutoEdgeDetectDeath},
		{Localize("Detect Teleporters"), &g_Config.m_MalinkaAutoEdgeDetectTele},
		{Localize("Show Found Edges"), &g_Config.m_MalinkaAutoEdgeShowFound},
		{Localize("Show Locked Edge"), &g_Config.m_MalinkaAutoEdgeShowLocked},
		{Localize("Advanced Settings"), &g_Config.m_MalinkaAutoEdgeAdvancedSettings},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aEdgeChecks, std::size(s_aEdgeChecks));
	if(g_Config.m_MalinkaAutoEdgeAdvancedSettings)
	{
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoEdgeDetectionRange, Localize("Detection Range"), 1, 40);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoEdgePopulationSize, Localize("Population Size"), 10, 1000);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoEdgeExplorationDepth, Localize("Exploration Depth"), 1, 40);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoEdgeTopK, Localize("Top-K Candidates"), 1, 128);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoEdgeSequenceLength, Localize("Sequence Length"), 1, 32);
	}
	FinishMalinkaSection(Column, s_ScrollState, EdgeSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t ShotgunSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Auto Shotgun"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAutoShotgun, Localize("Enable"), &g_Config.m_MalinkaAutoShotgun, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaAutoShotgun)
	{
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAutoShotgunAdvancedSettings, Localize("Advanced Settings"), &g_Config.m_MalinkaAutoShotgunAdvancedSettings, &Column, gs_MalinkaLineSize);
		if(g_Config.m_MalinkaAutoShotgunAdvancedSettings)
		{
			static std::array<const char *, 2> s_aBounceNames = {Localize("Least"), Localize("Most")};
			static std::array<const char *, 2> s_aSpeedNames = {Localize("Least Speed"), Localize("Most Speed")};
			static CUi::SDropDownState s_BounceState;
			static CUi::SDropDownState s_SpeedState;
			static CScrollRegion s_BounceScroll;
			static CScrollRegion s_SpeedScroll;
			DoMalinkaDropDown(pMenus, Column, Localize("Bounces"), g_Config.m_MalinkaAutoShotgunMostBounces, s_aBounceNames, s_BounceState, s_BounceScroll);
			DoMalinkaDropDown(pMenus, Column, Localize("Speed"), g_Config.m_MalinkaAutoShotgunHighestVel, s_aSpeedNames, s_SpeedState, s_SpeedScroll);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAutoShotgunSilent, Localize("Silent"), &g_Config.m_MalinkaAutoShotgunSilent, &Column, gs_MalinkaLineSize);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoShotgunPoints, Localize("Points"), 1, 64);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoShotgunTicks, Localize("Ticks"), 1, 64);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoShotgunFov, Localize("FOV"), 1, 360);
		}
	}
	FinishMalinkaSection(Column, s_ScrollState, ShotgunSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t UnfreezeSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Auto Unfreeze"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaUnfreezeBot, Localize("Enable"), &g_Config.m_MalinkaUnfreezeBot, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaUnfreezeBot)
	{
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaUnfreezeBotEsp, Localize("ESP"), &g_Config.m_MalinkaUnfreezeBotEsp, &Column, gs_MalinkaLineSize);
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaUnfreezeBotAdvancedSettings, Localize("Advanced Settings"), &g_Config.m_MalinkaUnfreezeBotAdvancedSettings, &Column, gs_MalinkaLineSize);
		if(g_Config.m_MalinkaUnfreezeBotAdvancedSettings)
		{
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaUnfreezeBotPoints, Localize("Points"), 1, 64);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaUnfreezeBotCurDirTicks, Localize("Current Dir Ticks"), 1, 64);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaUnfreezeBotTicks, Localize("Ticks"), 1, 64);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaUnfreezeBotFov, Localize("FOV"), 1, 360);
		}
	}
	FinishMalinkaSection(Column, s_ScrollState, UnfreezeSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t BenchmarkSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Prediction Benchmark"));
	static CButtonContainer s_BenchmarkButton;
	if(DoMalinkaButton(pMenus, Column, s_BenchmarkButton, g_Config.m_MalinkaPredictionBenchmark ? "Stop Benchmark" : "Start Benchmark", g_Config.m_MalinkaPredictionBenchmark ? ColorRGBA(0.8f, 0.1f, 0.1f, 0.85f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f)))
		g_Config.m_MalinkaPredictionBenchmark ^= 1;
	DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaPredictionBenchmarkRuns, Localize("Runs"), 1, 1000);
	DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaPredictionBenchmarkThreads, Localize("Threads (memory/16-worker cap)"), 1, 16);
	FinishMalinkaSection(Column, s_ScrollState, BenchmarkSection);

	Column = RightView;
	const size_t OtherSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Other"));
	static const SBoolEntry s_aOtherChecks[] = {
		{Localize("Auto Team"), &g_Config.m_MalinkaAutoTeam},
		{Localize("Anti AFK"), &g_Config.m_MalinkaAntiAfk},
		{Localize("Kill on Freeze"), &g_Config.m_MalinkaKillOnFreeze},
		{Localize("Ignore Replay Warnings"), &g_Config.m_MalinkaIgnoreReplayWarnings},
		{Localize("Hide Chat Bubble"), &g_Config.m_MalinkaHideBubble},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aOtherChecks, std::size(s_aOtherChecks));
	FinishMalinkaSection(Column, s_ScrollState, OtherSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t TrollSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Troll"));
	static const SBoolEntry s_aTrollChecks[] = {
		{Localize("Emote Spam"), &g_Config.m_MalinkaEmoteSpam},
		{Localize("Killsay/Deathsays"), &g_Config.m_MalinkaKillsay},
		{Localize("Fancy Chat Font"), &g_Config.m_MalinkaFancyChatFont},
		{Localize("Mass Mention Spam"), &g_Config.m_MalinkaMassMentionSpam},
		{Localize("Chat Repeater"), &g_Config.m_MalinkaChatRepeater},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aTrollChecks, std::size(s_aTrollChecks));
	if(g_Config.m_MalinkaChatRepeater)
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaChatRepeaterLength, Localize("Min Length"), 1, 256);
	FinishMalinkaSection(Column, s_ScrollState, TrollSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t IdSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("ID Stealer"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaIdStealer, Localize("Enable"), &g_Config.m_MalinkaIdStealer, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaIdStealer)
	{
		static const SBoolEntry s_aIdChecks[] = {
			{Localize("Closest Player"), &g_Config.m_MalinkaIdStealerClosest},
			{Localize("Steal Name"), &g_Config.m_MalinkaIdStealerName},
			{Localize("Steal Clan"), &g_Config.m_MalinkaIdStealerClan},
			{Localize("Steal Skin"), &g_Config.m_MalinkaIdStealerSkin},
			{Localize("Steal Flag"), &g_Config.m_MalinkaIdStealerFlag},
			{Localize("Steal Eye Emote"), &g_Config.m_MalinkaIdStealerEmote},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aIdChecks, std::size(s_aIdChecks));
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaIdStealerSpeed, Localize("Stealer Speed"), 1, 30, "s");
	}
	FinishMalinkaSection(Column, s_ScrollState, IdSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t GhostSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Ghost Move"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaGhostMove, Localize("Enable"), &g_Config.m_MalinkaGhostMove, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaGhostMove)
	{
		static const SBoolEntry s_aGhostChecks[] = {
			{Localize("Direction"), &g_Config.m_MalinkaGhostMoveDirection},
			{Localize("Jump"), &g_Config.m_MalinkaGhostMoveJump},
			{Localize("Hook"), &g_Config.m_MalinkaGhostMoveHook},
			{Localize("Hook Closest"), &g_Config.m_MalinkaGhostMoveHookClosest},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aGhostChecks, std::size(s_aGhostChecks));
	}
	FinishMalinkaSection(Column, s_ScrollState, GhostSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t DummySection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Dummy Fly"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaDummyFly, Localize("Enable"), &g_Config.m_MalinkaDummyFly, &Column, gs_MalinkaLineSize);
	FinishMalinkaSection(Column, s_ScrollState, DummySection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t AutoVoteKickSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Auto Vote Kick"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAutoVoteKick, Localize("Enable"), &g_Config.m_MalinkaAutoVoteKick, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaAutoVoteKick)
	{
		static std::array<const char *, 2> s_aKickModes = {Localize("Random Target"), Localize("Specific ID")};
		static CUi::SDropDownState s_KickModeState;
		static CScrollRegion s_KickModeScroll;
		DoMalinkaDropDown(pMenus, Column, Localize("Mode"), g_Config.m_MalinkaAutoVoteKickMode, s_aKickModes, s_KickModeState, s_KickModeScroll);
		if(g_Config.m_MalinkaAutoVoteKickMode == 1)
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoVoteKickTarget, Localize("Target ID"), 0, 63);
		static CLineInput s_KickReasonInput(g_Config.m_MalinkaAutoVoteKickReason, sizeof(g_Config.m_MalinkaAutoVoteKickReason));
		DoMalinkaEditBox(pMenus, Column, &s_KickReasonInput, "Kick Reason", "vote kick", g_Config.m_MalinkaAutoVoteKickReason, sizeof(g_Config.m_MalinkaAutoVoteKickReason));
	}
	FinishMalinkaSection(Column, s_ScrollState, AutoVoteKickSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t FakeAimSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Fake Aim"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaFakeAim, Localize("Enable"), &g_Config.m_MalinkaFakeAim, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaFakeAim)
	{
		static const SBoolEntry s_aFakeAimChecks[] = {
			{Localize("Send Always"), &g_Config.m_MalinkaFakeAimSendAlways},
			{Localize("Visible"), &g_Config.m_MalinkaFakeAimVisible},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aFakeAimChecks, std::size(s_aFakeAimChecks));
		static std::array<const char *, 6> s_aFakeAimModes = {
			Localize("Mouse Pos"),
			Localize("Robot Aim"),
			Localize("Spinbot"),
			Localize("Random"),
			Localize("Fake Angles"),
			Localize("Aimbot Troll Aim"),
		};
		static CUi::SDropDownState s_FakeAimModeState;
		static CScrollRegion s_FakeAimModeScroll;
		DoMalinkaDropDown(pMenus, Column, Localize("Mode"), g_Config.m_MalinkaFakeAimMode, s_aFakeAimModes, s_FakeAimModeState, s_FakeAimModeScroll);
		if(g_Config.m_MalinkaFakeAimMode == 2 || g_Config.m_MalinkaFakeAimMode == 3)
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaFakeAimSpeed, Localize("Speed"), 1, 50);
	}
	FinishMalinkaSection(Column, s_ScrollState, FakeAimSection);

	EndMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);
}

void RenderMalinkaVisuals(CMenus *pMenus, CUIRect MainView)
{
	static CMalinkaScrollState s_ScrollState;
	CUIRect LeftView, RightView;
	BeginMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);

	CUIRect Column = LeftView;
	const size_t HudSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("HUD"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaFeaturesHud, Localize("Features HUD"), &g_Config.m_MalinkaFeaturesHud, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaFeaturesHud)
	{
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaFeaturesHudSegmented, Localize("Segmented"), &g_Config.m_MalinkaFeaturesHudSegmented, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaFeaturesHudOpacity, Localize("Opacity"), 0, 100);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaFeaturesHudFontSize, Localize("Font Size"), 1, 20);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaFeaturesHudGradientSpeed, Localize("Speed"), 0, 20);
		static CButtonContainer s_HudStartReset;
		static CButtonContainer s_HudEndReset;
		DoMalinkaColorPicker(pMenus, Column, s_HudStartReset, Localize("Start"), &g_Config.m_MalinkaFeaturesHudGradientStartCol, ColorRGBA(0.0f, 1.0f, 0.0f));
		DoMalinkaColorPicker(pMenus, Column, s_HudEndReset, Localize("End"), &g_Config.m_MalinkaFeaturesHudGradientEndCol, ColorRGBA(0.0f, 0.5f, 0.0f));
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaFeaturesHudGradientRainbow, Localize("Rainbow"), &g_Config.m_MalinkaFeaturesHudGradientRainbow, &Column, gs_MalinkaLineSize);
	}
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaNotifications, Localize("Notifications"), &g_Config.m_MalinkaNotifications, &Column, gs_MalinkaLineSize);
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTasHud, Localize("TAS HUD"), &g_Config.m_MalinkaTasHud, &Column, gs_MalinkaLineSize);
	FinishMalinkaSection(Column, s_ScrollState, HudSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t FrozenHudSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, "Frozen Tees HUD");
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaShowFrozenHud, "Enable", &g_Config.m_MalinkaShowFrozenHud, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaShowFrozenHud)
	{
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaShowFrozenHudSkins, "Skins", &g_Config.m_MalinkaShowFrozenHudSkins, &Column, gs_MalinkaLineSize);
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaShowFrozenHudOnlyInTeam, "Same team only", &g_Config.m_MalinkaShowFrozenHudOnlyInTeam, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaShowFrozenHudMaxRows, "Maximum rows", 1, 6);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaShowFrozenHudSize, "Size", 8, 24);
		static std::array<const char *, 3> s_aFrozenText = {"Name", "Name and ID", "Name and freeze time"};
		static CUi::SDropDownState s_FrozenTextState;
		static CScrollRegion s_FrozenTextScroll;
		DoMalinkaDropDown(pMenus, Column, "Text", g_Config.m_MalinkaFrozenTeesText, s_aFrozenText, s_FrozenTextState, s_FrozenTextScroll);
	}
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaShowWhenLast, "Last unfrozen teammate", &g_Config.m_MalinkaShowWhenLast, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaShowWhenLast)
	{
		static CLineInput s_LastText;
		static CButtonContainer s_LastColor;
		DoMalinkaEditBox(pMenus, Column, &s_LastText, "Text", "Last!", g_Config.m_MalinkaShowWhenLastText, sizeof(g_Config.m_MalinkaShowWhenLastText));
		DoMalinkaColorPicker(pMenus, Column, s_LastColor, "Color", &g_Config.m_MalinkaShowWhenLastColor, ColorRGBA(1, 0, 0));
	}
	FinishMalinkaSection(Column, s_ScrollState, FrozenHudSection);

	RenderMalinkaGradientSection(pMenus, Column, s_ScrollState, Localize("Watermark"), &g_Config.m_MalinkaWatermark, &g_Config.m_MalinkaWatermarkSegmented, &g_Config.m_MalinkaWatermarkGradientSpeed, &g_Config.m_MalinkaWatermarkGradientStartCol, &g_Config.m_MalinkaWatermarkGradientEndCol, &g_Config.m_MalinkaWatermarkGradientRainbow, false);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t ColorsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Local Colors"));
	static CButtonContainer s_TeeColor, s_HookColor, s_WeaponColor;
	DoMalinkaColorPicker(pMenus, Column, s_TeeColor, "Tee Color", &g_Config.m_MalinkaCustomColorTeeValue, ColorRGBA(0.0f, 1.0f, 0.0f));
	DoMalinkaColorPicker(pMenus, Column, s_HookColor, "Hook Color", &g_Config.m_MalinkaCustomColorHookValue, ColorRGBA(1.0f, 0.0f, 0.0f));
	DoMalinkaColorPicker(pMenus, Column, s_WeaponColor, "Weapon Color", &g_Config.m_MalinkaCustomColorWeaponValue, ColorRGBA(0.0f, 0.0f, 1.0f));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaCustomColorWeaponRainbow, "Rainbow Weapon", &g_Config.m_MalinkaCustomColorWeaponRainbow, &Column, gs_MalinkaLineSize);
	FinishMalinkaSection(Column, s_ScrollState, ColorsSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t CustomMenuSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Custom Menu"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaCustomTheme, Localize("Enable"), &g_Config.m_MalinkaCustomTheme, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaCustomTheme)
	{
		static CButtonContainer s_AccentReset;
		static CButtonContainer s_BgReset;
		DoMalinkaColorPicker(pMenus, Column, s_AccentReset, Localize("Accent Color"), &g_Config.m_MalinkaCustomAccentColor, ColorRGBA(0.0f, 1.0f, 0.0f));
		DoMalinkaColorPicker(pMenus, Column, s_BgReset, Localize("Background Color"), &g_Config.m_MalinkaCustomBgColor, ColorRGBA(0.1f, 0.1f, 0.1f));
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaRainbowMenu, Localize("Rainbow Menu"), &g_Config.m_MalinkaRainbowMenu, &Column, gs_MalinkaLineSize);
	}
	FinishMalinkaSection(Column, s_ScrollState, CustomMenuSection);

	Column = RightView;
	const size_t AimLinesSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Aim Lines"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAimLines, Localize("Draw Aim Lines"), &g_Config.m_MalinkaAimLines, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaAimLines)
	{
		static const SBoolEntry s_aAimChecks[] = {
			{Localize("Self Only"), &g_Config.m_MalinkaAimLinesSelfOnly},
			{Localize("Pistol"), &g_Config.m_MalinkaAimLinesGun},
			{Localize("Shotgun"), &g_Config.m_MalinkaAimLinesShotgun},
			{Localize("Grenade"), &g_Config.m_MalinkaAimLinesGrenade},
			{Localize("Laser"), &g_Config.m_MalinkaAimLinesLaser},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aAimChecks, std::size(s_aAimChecks));
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAimLinesAlpha, Localize("Alpha"), 0, 100);
	}
	FinishMalinkaSection(Column, s_ScrollState, AimLinesSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t BulletLinesSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Bullet Lines"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaBulletLines, Localize("Draw Bullet Lines"), &g_Config.m_MalinkaBulletLines, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaBulletLines)
	{
		static const SBoolEntry s_aBulletChecks[] = {
			{Localize("Self Only"), &g_Config.m_MalinkaBulletLinesSelfOnly},
			{Localize("Pistol"), &g_Config.m_MalinkaBulletLinesGun},
			{Localize("Shotgun"), &g_Config.m_MalinkaBulletLinesShotgun},
			{Localize("Grenade"), &g_Config.m_MalinkaBulletLinesGrenade},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aBulletChecks, std::size(s_aBulletChecks));
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaBulletLinesAlpha, Localize("Alpha"), 0, 100);
	}
	FinishMalinkaSection(Column, s_ScrollState, BulletLinesSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t CustomColorsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Custom Colors"));
	static const SBoolEntry s_aColorChecks[] = {
		{Localize("Tee"), &g_Config.m_MalinkaCustomColorTee},
		{Localize("Hook"), &g_Config.m_MalinkaCustomColorHook},
		{Localize("Weapon"), &g_Config.m_MalinkaCustomColorWeapon},
		{Localize("Rainbow Tee"), &g_Config.m_MalinkaCustomColorTeeRainbow},
		{Localize("Rainbow Hook"), &g_Config.m_MalinkaCustomColorHookRainbow},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aColorChecks, std::size(s_aColorChecks));
	DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaCustomColorRainbowSpeed, Localize("Rainbow Speed"), 1, 500, "%");
	FinishMalinkaSection(Column, s_ScrollState, CustomColorsSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t FrozenColorSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Custom Frozen Color"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaCustomFrozenColor, Localize("Enable"), &g_Config.m_MalinkaCustomFrozenColor, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaCustomFrozenColor)
	{
		static CButtonContainer s_FrozenColorReset;
		DoMalinkaColorPicker(pMenus, Column, s_FrozenColorReset, Localize("Color"), &g_Config.m_MalinkaCustomFrozenColorValue, ColorRGBA(0.2f, 0.6f, 1.0f));
	}
	FinishMalinkaSection(Column, s_ScrollState, FrozenColorSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t TrajectorySection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Trajectory"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTrajectoryEsp, Localize("Enable"), &g_Config.m_MalinkaTrajectoryEsp, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaTrajectoryEsp)
	{
		static std::array<const char *, 2> s_aTrajectoryModes = {Localize("Full Line"), Localize("Dotted")};
		static CUi::SDropDownState s_TrajectoryModeState;
		static CScrollRegion s_TrajectoryModeScroll;
		static CButtonContainer s_LocalTrajectoryReset;
		static CButtonContainer s_FrozenTrajectoryReset;
		static CButtonContainer s_OtherTrajectoryReset;
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTrajectoryEspSelfOnly, Localize("Self Only"), &g_Config.m_MalinkaTrajectoryEspSelfOnly, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaTrajectoryEspTicks, Localize("Ticks"), 1, 200);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaTrajectoryEspThickness, Localize("Path Thickness"), 1, 10);
		DoMalinkaDropDown(pMenus, Column, Localize("Path Mode"), g_Config.m_MalinkaTrajectoryEspMode, s_aTrajectoryModes, s_TrajectoryModeState, s_TrajectoryModeScroll);
		DoMalinkaColorPicker(pMenus, Column, s_LocalTrajectoryReset, Localize("Local Color"), &g_Config.m_MalinkaTrajectoryEspColorLocal, ColorRGBA(0.0f, 1.0f, 0.0f));
		DoMalinkaColorPicker(pMenus, Column, s_FrozenTrajectoryReset, Localize("Frozen Color"), &g_Config.m_MalinkaTrajectoryEspColorFrozen, ColorRGBA(0.0f, 1.0f, 1.0f));
		DoMalinkaColorPicker(pMenus, Column, s_OtherTrajectoryReset, Localize("Others Color"), &g_Config.m_MalinkaTrajectoryEspColorOthers, ColorRGBA(0.0f, 0.5f, 0.0f));
	}
	FinishMalinkaSection(Column, s_ScrollState, TrajectorySection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t TrailSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Trail"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTrail, Localize("Enable"), &g_Config.m_MalinkaTrail, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaTrail)
	{
		static std::array<const char *, 5> s_aTrailTypes = {Localize("Smoke"), Localize("Bullet"), Localize("Powerup"), Localize("Sparkle"), "Tater"};
		static CUi::SDropDownState s_TrailTypeState;
		static CScrollRegion s_TrailTypeScroll;
		DoMalinkaDropDown(pMenus, Column, Localize("Type"), g_Config.m_MalinkaTrailType, s_aTrailTypes, s_TrailTypeState, s_TrailTypeScroll);
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTrailColorRainbow, Localize("Rainbow"), &g_Config.m_MalinkaTrailColorRainbow, &Column, gs_MalinkaLineSize);
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTrailLowTaper, Localize("Low Taper"), &g_Config.m_MalinkaTrailLowTaper, &Column, gs_MalinkaLineSize);
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTrailFade, Localize("Fade"), &g_Config.m_MalinkaTrailFade, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaTrailWidth, Localize("Width"), 1, 64);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaTrailLength, Localize("Length"), 1, 128);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaTrailAlpha, Localize("Alpha"), 0, 100);
	}
	FinishMalinkaSection(Column, s_ScrollState, TrailSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t CamSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Cam"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaSmoothCam, Localize("Smooth Cam"), &g_Config.m_MalinkaSmoothCam, &Column, gs_MalinkaLineSize);
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaSuperDyncam, Localize("Super Smooth Cam"), &g_Config.m_MalinkaSuperDyncam, &Column, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaSmoothCam || g_Config.m_MalinkaSuperDyncam)
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaSmoothCamSmoothness, Localize("Smoothness"), 1, 100);
	FinishMalinkaSection(Column, s_ScrollState, CamSection);

	EndMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);
}

void RenderMalinkaAvoid(CMenus *pMenus, CUIRect MainView)
{
	static std::array<const char *, 4> s_aAvoidModes = {Localize("Basic"), Localize("Legit"), Localize("Blatant"), Localize("Fent Bot")};
	static CUi::SDropDownState s_AvoidModeState;
	static CScrollRegion s_AvoidModeScroll;
	DoMalinkaDropDown(pMenus, MainView, "", g_Config.m_MalinkaAvoidMode, s_aAvoidModes, s_AvoidModeState, s_AvoidModeScroll, 0.0f);
	MainView.HSplitTop(gs_MalinkaMargin + gs_MalinkaMarginSmall, nullptr, &MainView);

	static CMalinkaScrollState s_ScrollState;
	CUIRect LeftView, RightView;
	BeginMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);

	CUIRect Column = LeftView;
	const size_t MainSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Main"));
	static CButtonContainer s_StartStopButton;
	const bool IsFent = g_Config.m_MalinkaAvoidMode == 3;
	const char *pToggleLabel = IsFent ? (g_Config.m_MalinkaAvoidfreeze ? "Overdose" : "Inject Fent") : (g_Config.m_MalinkaAvoidfreeze ? "Stop" : "Start");
	const ColorRGBA ToggleColor = g_Config.m_MalinkaAvoidfreeze ? ColorRGBA(0.8f, 0.1f, 0.1f, 0.85f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f);
	if(DoMalinkaButton(pMenus, Column, s_StartStopButton, pToggleLabel, ToggleColor))
		g_Config.m_MalinkaAvoidfreeze ^= 1;
	Column.HSplitTop(gs_MalinkaMarginSmall, nullptr, &Column);

	if(g_Config.m_MalinkaAvoidMode == 1)
	{
		static const SBoolEntry s_aChecks[] = {
			{Localize("Player Prediction"), &g_Config.m_MalinkaAvoidLegitPlayerPrediction},
			{Localize("Afk Protection"), &g_Config.m_MalinkaAvoidLegitAfkProtection},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aChecks, std::size(s_aChecks));
		if(g_Config.m_MalinkaAvoidLegitAfkProtection)
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitAfkTime, Localize("Seconds"), 1, 120);
	}
	else if(g_Config.m_MalinkaAvoidMode == 2)
	{
		static const SBoolEntry s_aChecks[] = {
			{Localize("Player Prediction"), &g_Config.m_MalinkaAvoidBlatantPlayerPrediction},
			{Localize("NSIF (No Safe Input Found)"), &g_Config.m_MalinkaAvoidBlatantNsif},
			{Localize("Afk Protection"), &g_Config.m_MalinkaAvoidBlatantAfkProtection},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aChecks, std::size(s_aChecks));
		if(g_Config.m_MalinkaAvoidBlatantAfkProtection)
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantAfkTime, Localize("Seconds"), 1, 120);
	}
	else if(g_Config.m_MalinkaAvoidMode == 3)
	{
		static const SBoolEntry s_aChecks[] = {
			{Localize("Player Prediction"), &g_Config.m_MalinkaAvoidFentPlayerPrediction},
			{Localize("Unfreeze"), &g_Config.m_MalinkaAvoidFentUnfreeze},
			{Localize("Advanced Settings"), &g_Config.m_MalinkaAvoidFentAdvancedSettings},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aChecks, std::size(s_aChecks));
		if(g_Config.m_MalinkaAvoidFentUnfreeze)
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidFentUnfreezeRadius, Localize("Radius"), 1, 20);
		if(g_Config.m_MalinkaAvoidFentAdvancedSettings)
		{
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidFentTicks, Localize("Fent Ticks"), 10, 10000);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidFentTweakerInputs, Localize("Tweaker Inputs"), 1, 5000);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidFentTweakerTicks, Localize("Tweaker Ticks"), 1, 64);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidFentTweakerDosage, Localize("Tweaker Dosage"), 1, 1000);
		}
		else
		{
			static std::array<const char *, 3> s_aQuality = {Localize("Low"), Localize("Mid"), Localize("Max")};
			static CUi::SDropDownState s_QualityState;
			static CScrollRegion s_QualityScroll;
			DoMalinkaDropDown(pMenus, Column, Localize("Quality"), g_Config.m_MalinkaAvoidFentQualitySetting, s_aQuality, s_QualityState, s_QualityScroll);
		}
	}
	FinishMalinkaSection(Column, s_ScrollState, MainSection);

	if(g_Config.m_MalinkaAvoidMode == 1 || g_Config.m_MalinkaAvoidMode == 2)
	{
		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
		const size_t SettingsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Settings"));
		if(g_Config.m_MalinkaAvoidMode == 1)
		{
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitCheckTicks, Localize("Check Ticks"), 1, 60);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidLegitHook, Localize("Hook"), &g_Config.m_MalinkaAvoidLegitHook, &Column, gs_MalinkaLineSize);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidLegitDirection, Localize("Direction"), &g_Config.m_MalinkaAvoidLegitDirection, &Column, gs_MalinkaLineSize);
		}
		else if(g_Config.m_MalinkaAvoidMode == 2)
		{
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantCheckTicks, Localize("Check Ticks"), 1, 60);
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantKickInTicks, Localize("Kick in Ticks"), 1, 60);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantHook, Localize("Hook"), &g_Config.m_MalinkaAvoidBlatantHook, &Column, gs_MalinkaLineSize);
			int DirectionEnabled = g_Config.m_MalinkaAvoidBlatantDirection && g_Config.m_MalinkaAvoidBlatantCanSetDir;
			if(DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantDirection, Localize("Direction"), &DirectionEnabled, &Column, gs_MalinkaLineSize))
				g_Config.m_MalinkaAvoidBlatantDirection = g_Config.m_MalinkaAvoidBlatantCanSetDir = DirectionEnabled;
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantTrackPoint, Localize("Track Point"), &g_Config.m_MalinkaAvoidBlatantTrackPoint, &Column, gs_MalinkaLineSize);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantSafeAimTracking, Localize("Safe Aim Tracking"), &g_Config.m_MalinkaAvoidBlatantSafeAimTracking, &Column, gs_MalinkaLineSize);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidTileAutoDrag, Localize("Auto Drag"), &g_Config.m_MalinkaAvoidTileAutoDrag, &Column, gs_MalinkaLineSize);
		}
		FinishMalinkaSection(Column, s_ScrollState, SettingsSection);
	}

	Column = RightView;
	if(g_Config.m_MalinkaAvoidMode == 1)
	{
		const size_t TilesSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Tiles"));
		static const SBoolEntry s_aTiles[] = {
			{Localize("Teles"), &g_Config.m_MalinkaAvoidLegitTeles},
			{Localize("Death"), &g_Config.m_MalinkaAvoidLegitDeath},
			{Localize("Unfreeze"), &g_Config.m_MalinkaAvoidLegitUnfreeze},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aTiles, std::size(s_aTiles));
		if(g_Config.m_MalinkaAvoidLegitUnfreeze)
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitUnfreezeTicks, Localize("Ticks"), 1, 30);
		FinishMalinkaSection(Column, s_ScrollState, TilesSection);

		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
		const size_t PrioritySection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Priority"));
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitQuality, Localize("Quality"), 1, 1000, "", Localize("Number of full-horizon simulations. High Quality or Check Ticks can stall frames; lower both if needed. Existing presets are not changed."));
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitRandomness, Localize("Randomness"), 0, 64);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitDirectionPriority, Localize("Direction Priority"), 0, 1000);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitHookPriority, Localize("Hook Priority"), 0, 1000);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidLegitLifePriority, Localize("Life Priority"), 0, 1000);
		FinishMalinkaSection(Column, s_ScrollState, PrioritySection);
	}
	else if(g_Config.m_MalinkaAvoidMode == 2)
	{
		const size_t TilesSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Tiles"));
		static const SBoolEntry s_aTiles[] = {
			{Localize("Teles"), &g_Config.m_MalinkaAvoidBlatantTeles},
			{Localize("Death"), &g_Config.m_MalinkaAvoidBlatantDeath},
			{Localize("Unfreeze"), &g_Config.m_MalinkaAvoidBlatantUnfreeze},
		};
		DoMalinkaCheckBoxes(pMenus, Column, s_aTiles, std::size(s_aTiles));
		if(g_Config.m_MalinkaAvoidBlatantUnfreeze)
			DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantUnfreezeTicks, Localize("Ticks"), 1, 30);
		FinishMalinkaSection(Column, s_ScrollState, TilesSection);

		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
		const size_t AimSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Aimbot"));
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantAimbot, Localize("Enable"), &g_Config.m_MalinkaAvoidBlatantAimbot, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantAimbotSegments, Localize("Segments"), 1, 180);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantAimbotFov, Localize("FOV"), 1, 360);
		static std::array<const char *, 2> s_aAimModes = {Localize("Auto Aim"), Localize("Aim Assist")};
		static CUi::SDropDownState s_AimModeState;
		static CScrollRegion s_AimModeScroll;
		DoMalinkaDropDown(pMenus, Column, Localize("Mode"), g_Config.m_MalinkaAvoidBlatantAimbotMode, s_aAimModes, s_AimModeState, s_AimModeScroll);
		FinishMalinkaSection(Column, s_ScrollState, AimSection);

		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
		const size_t VisualsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Visuals"));
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantVisualTrackPoint, Localize("Track Point"), &g_Config.m_MalinkaAvoidBlatantVisualTrackPoint, &Column, gs_MalinkaLineSize);
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantVisualAimbot, Localize("Aimbot"), &g_Config.m_MalinkaAvoidBlatantVisualAimbot, &Column, gs_MalinkaLineSize);
		FinishMalinkaSection(Column, s_ScrollState, VisualsSection);

		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
		const size_t FreezeResetSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Freeze Reset"));
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantFreezeReset, Localize("Freeze Reset"), &g_Config.m_MalinkaAvoidBlatantFreezeReset, &Column, gs_MalinkaLineSize);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantFreezeResetWait, Localize("Freeze Reset Wait"), 0, 10);
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAvoidBlatantFreezeResetFriendDist, Localize("Freeze Reset Friend Radius"), 0, 30);
		FinishMalinkaSection(Column, s_ScrollState, FreezeResetSection);

		Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
		const size_t GoresSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Gores"));
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidBlatantOnlyHorizontally, Localize("Avoid Freeze Only Horizontally"), &g_Config.m_MalinkaAvoidBlatantOnlyHorizontally, &Column, gs_MalinkaLineSize);
		FinishMalinkaSection(Column, s_ScrollState, GoresSection);
	}
	else if(g_Config.m_MalinkaAvoidMode == 3)
	{
		const size_t VisualsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Visuals"));
		if(g_Config.m_MalinkaAvoidMode == 3)
		{
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidFentRenderPath, Localize("Render Path"), &g_Config.m_MalinkaAvoidFentRenderPath, &Column, gs_MalinkaLineSize);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidFentRenderPathfinding, Localize("Render Pathfinding"), &g_Config.m_MalinkaAvoidFentRenderPathfinding, &Column, gs_MalinkaLineSize);
			DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaAvoidFentSpectateScan, Localize("Spectate Scan"), &g_Config.m_MalinkaAvoidFentSpectateScan, &Column, gs_MalinkaLineSize);
			static CButtonContainer s_SaveFentReplayButton;
			Column.HSplitTop(gs_MalinkaMarginSmall, nullptr, &Column);
			if(DoMalinkaButton(pMenus, Column, s_SaveFentReplayButton, "Save Replay"))
				pMenus->MalinkaFentSaveReplay();
		}
		FinishMalinkaSection(Column, s_ScrollState, VisualsSection);

	}

	EndMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);
}

void RenderMalinkaTas(CMenus *pMenus, CUIRect MainView)
{
	static CMalinkaScrollState s_ScrollState;
	CUIRect LeftView, RightView;
	BeginMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);

	auto DoMalinkaDoubleButtonRow = [&](CUIRect &View, CButtonContainer &LeftId, const char *pLeftLabel, CButtonContainer &RightId, const char *pRightLabel, bool *pLeftPressed = nullptr, bool *pRightPressed = nullptr, ColorRGBA LeftColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), ColorRGBA RightColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f)) {
		CUIRect Row, LeftButton, RightButton;
		View.HSplitTop(gs_MalinkaLineSize, &Row, &View);
		Row.VSplitMid(&LeftButton, &RightButton, 5.0f);
		if(pLeftPressed != nullptr)
			*pLeftPressed = pMenus->DoButtonLineSize_MenuHelper(&LeftId, pLeftLabel, 0, &LeftButton, gs_MalinkaLineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, LeftColor) != 0;
		else
			pMenus->DoButtonLineSize_MenuHelper(&LeftId, pLeftLabel, 0, &LeftButton, gs_MalinkaLineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, LeftColor);
		if(pRightPressed != nullptr)
			*pRightPressed = pMenus->DoButtonLineSize_MenuHelper(&RightId, pRightLabel, 0, &RightButton, gs_MalinkaLineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, RightColor) != 0;
		else
			pMenus->DoButtonLineSize_MenuHelper(&RightId, pRightLabel, 0, &RightButton, gs_MalinkaLineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, RightColor);
	};

	CUIRect Column = LeftView;
	const size_t MainSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Main"));
	DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaTasTps, "TPS", 1, 50);
	static CButtonContainer s_PauseButton;
	static CButtonContainer s_PlaybackButton;
	static CButtonContainer s_RecordButton;
	static CButtonContainer s_ClearReplayButton;
	Column.HSplitTop(gs_MalinkaMarginSmall, nullptr, &Column);
	bool PausePressed = false;
	bool PlaybackPressed = false;
	DoMalinkaDoubleButtonRow(Column, s_PauseButton, g_Config.m_MalinkaTasPause ? "Resume" : "Pause", s_PlaybackButton, g_Config.m_MalinkaLoadReplay ? "Stop Playback" : "Playback Replay", &PausePressed, &PlaybackPressed, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), g_Config.m_MalinkaLoadReplay ? ColorRGBA(0.8f, 0.1f, 0.1f, 0.85f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f));
	if(PausePressed)
		g_Config.m_MalinkaTasPause ^= 1;
	if(PlaybackPressed)
		g_Config.m_MalinkaLoadReplay ^= 1;
	Column.HSplitTop(gs_MalinkaMarginSmall + 1.0f, nullptr, &Column);
	bool RecordPressed = false;
	bool ClearPressed = false;
	DoMalinkaDoubleButtonRow(Column, s_RecordButton, g_Config.m_MalinkaRecordReplay ? "Stop Recording" : "Record Replay", s_ClearReplayButton, "Clear Replay", &RecordPressed, &ClearPressed, g_Config.m_MalinkaRecordReplay ? ColorRGBA(0.8f, 0.1f, 0.1f, 0.85f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f));
	if(RecordPressed)
		g_Config.m_MalinkaRecordReplay ^= 1;
	if(ClearPressed)
		g_Config.m_MalinkaTasRespawn ^= 1;
	FinishMalinkaSection(Column, s_ScrollState, MainSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t SettingsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Settings"));
	static const SBoolEntry s_aTasChecks[] = {
		{Localize("Enable Sound"), &g_Config.m_MalinkaTasUseSound},
		{Localize("Enable Effects"), &g_Config.m_MalinkaTasShowEffects},
		{Localize("Show Real Aim"), &g_Config.m_MalinkaTasShowAim},
		{Localize("Predict Players"), &g_Config.m_MalinkaTasPlayerPrediction},
		{Localize("Auto Replay"), &g_Config.m_MalinkaTasAutoReplay},
		{Localize("Auto Save Replay"), &g_Config.m_MalinkaAutoSaveReplay},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aTasChecks, std::size(s_aTasChecks));
	if(g_Config.m_MalinkaAutoSaveReplay)
		DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaAutoSaveReplayInterval, Localize("Interval"), 1, 300, "s");
	FinishMalinkaSection(Column, s_ScrollState, SettingsSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t HotkeysSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Hotkeys"));
	static CButtonContainer s_RecordReader;
	static CButtonContainer s_RecordClear;
	static CButtonContainer s_LoadReader;
	static CButtonContainer s_LoadClear;
	static CButtonContainer s_ClearReader;
	static CButtonContainer s_ClearClear;
	static CButtonContainer s_PauseReader;
	static CButtonContainer s_PauseClear;
	static CButtonContainer s_RewindReader;
	static CButtonContainer s_RewindClear;
	static CButtonContainer s_ForwardReader;
	static CButtonContainer s_ForwardClear;
	pMenus->DoLine_KeyReader(Column, s_RecordReader, s_RecordClear, "Record", "toggle malinka_recordreplay 1 0");
	pMenus->DoLine_KeyReader(Column, s_LoadReader, s_LoadClear, "Load", "toggle malinka_loadreplay 1 0");
	pMenus->DoLine_KeyReader(Column, s_ClearReader, s_ClearClear, "Clear", "+toggle malinka_tasrespawn 1 0");
	pMenus->DoLine_KeyReader(Column, s_PauseReader, s_PauseClear, "Pause", "toggle malinka_taspause 0 1");
	pMenus->DoLine_KeyReader(Column, s_RewindReader, s_RewindClear, "Rewind", "+toggle malinka_tasrewind 1 0");
	pMenus->DoLine_KeyReader(Column, s_ForwardReader, s_ForwardClear, "Forward", "+toggle malinka_tasforward 1 0");
	FinishMalinkaSection(Column, s_ScrollState, HotkeysSection);

	Column = RightView;
	const size_t ReplaySection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Replay"));
	static CLineInput s_ReplayNameInput(g_Config.m_MalinkaTasReplayName, sizeof(g_Config.m_MalinkaTasReplayName));
	static CButtonContainer s_SaveReplayInline;
	static CButtonContainer s_ReloadReplayInline;
	static CUi::SDropDownState s_SavedReplayState;
	static CScrollRegion s_SavedReplayScroll;
	static int s_SelectedReplay = 0;
	static int s_ReplayRefreshNonce = 0;
	CUIRect ReplayNameRow, ReplayNameBox, ReplaySaveButton, ReplayReloadButton;
	Column.HSplitTop(gs_MalinkaLineSize, &ReplayNameRow, &Column);
	ReplayNameRow.VSplitRight(70.0f, &ReplayNameRow, &ReplayReloadButton);
	ReplayNameRow.VSplitRight(5.0f, &ReplayNameRow, nullptr);
	ReplayNameRow.VSplitRight(90.0f, &ReplayNameRow, &ReplaySaveButton);
	ReplayNameBox = ReplayNameRow;
	pMenus->MenuUi()->DoClearableEditBox(&s_ReplayNameInput, &ReplayNameBox, gs_MalinkaFontSize, IGraphics::CORNER_ALL);
	const bool SaveInlinePressed = pMenus->DoButtonLineSize_MenuHelper(&s_SaveReplayInline, "Save", 0, &ReplaySaveButton, gs_MalinkaLineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.45f, 0.45f, 0.1f, 0.85f)) != 0;
	const bool ReloadInlinePressed = pMenus->DoButtonLineSize_MenuHelper(&s_ReloadReplayInline, "Reload", 0, &ReplayReloadButton, gs_MalinkaLineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f)) != 0;
	if(SaveInlinePressed)
		pMenus->MalinkaTasSaveReplay();
	if(ReloadInlinePressed)
	{
		++s_ReplayRefreshNonce;
		s_SavedReplayState = CUi::SDropDownState();
		s_SelectedReplay = 0;
	}
	Column.HSplitTop(gs_MalinkaMarginSmall, nullptr, &Column);
	static std::vector<std::string> vReplayNameStorage;
	static int s_ListedNonce = -1;
	static int64_t s_NextReplayScan = 0;
	if(s_ListedNonce != s_ReplayRefreshNonce || time_get() >= s_NextReplayScan)
	{
		vReplayNameStorage = Malinka::Replay::List(pMenus->MenuStorage());
		s_ListedNonce = s_ReplayRefreshNonce;
		s_NextReplayScan = time_get() + 5 * time_freq();
	}
	if(vReplayNameStorage.empty())
		vReplayNameStorage.push_back("No replays found");
	std::vector<const char *> vReplayNames;
	vReplayNames.reserve(vReplayNameStorage.size());
	for(const std::string &Name : vReplayNameStorage)
		vReplayNames.push_back(Name.c_str());
	s_SelectedReplay = std::clamp(s_SelectedReplay, 0, (int)vReplayNames.size() - 1);
	s_SavedReplayState.m_SelectionPopupContext.m_pScrollRegion = &s_SavedReplayScroll;
	CUIRect ReplayDropDownRow;
	Column.HSplitTop(gs_MalinkaLineSize, &ReplayDropDownRow, &Column);
	const int NewSelectedReplay = pMenus->MenuUi()->DoDropDown(&ReplayDropDownRow, s_SelectedReplay, vReplayNames.empty() ? nullptr : vReplayNames.data(), vReplayNames.size(), s_SavedReplayState);
	if(NewSelectedReplay != s_SelectedReplay)
	{
		s_SelectedReplay = NewSelectedReplay;
		if(vReplayNameStorage[s_SelectedReplay] != "No replays found")
		{
			const std::string &ReplayStem = vReplayNameStorage[s_SelectedReplay];
			str_copy(g_Config.m_MalinkaTasReplayName, ReplayStem.c_str(), sizeof(g_Config.m_MalinkaTasReplayName));
			pMenus->MalinkaTasLoadSelectedReplay();
		}
	}
	// Explicit load is needed for an unchanged/first selection (including a
	// one-file list): dropdown change callbacks alone cannot load index zero.
	static CButtonContainer s_LoadSelectedReplay;
	if(DoMalinkaButton(pMenus, Column, s_LoadSelectedReplay, "Load Selected Replay") && vReplayNameStorage[s_SelectedReplay] != "No replays found")
	{
		str_copy(g_Config.m_MalinkaTasReplayName, vReplayNameStorage[s_SelectedReplay].c_str(), sizeof(g_Config.m_MalinkaTasReplayName));
		pMenus->MalinkaTasLoadSelectedReplay();
	}
	Column.HSplitTop(gs_MalinkaMarginSmall, nullptr, &Column);
	static CButtonContainer s_ValidateReplay;
	static CButtonContainer s_ReplayTime;
	bool ValidatePressed = false;
	bool ReplayTimePressed = false;
	DoMalinkaDoubleButtonRow(Column, s_ValidateReplay, "Validate Replay", s_ReplayTime, "Get Replay Time", &ValidatePressed, &ReplayTimePressed);
	if(ValidatePressed)
		pMenus->MalinkaTasValidateReplay();
	if(ReplayTimePressed)
		pMenus->MalinkaTasReportReplayTime();
	Column.HSplitTop(gs_MalinkaMarginSmall + 1.0f, nullptr, &Column);
	CUIRect ImportInfo;
	Column.HSplitTop(gs_MalinkaLineSize, &ImportInfo, &Column);
	pMenus->MenuUi()->DoLabel(&ImportInfo, pMenus->MalinkaTasImportPending() ? "PREVIEW ONLY: imported KRX positions are not validated." : "KRX sample import: validate before saving a conversion.", 11.0f, TEXTALIGN_ML);
	Column.HSplitTop(gs_MalinkaLineSize, &ImportInfo, &Column);
	pMenus->MenuUi()->DoLabel(&ImportInfo, "Use the same map/start; save conversions outside vault/.", 11.0f, TEXTALIGN_ML);
	FinishMalinkaSection(Column, s_ScrollState, ReplaySection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t VaultSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Replay Vault"));
	const SBoolEntry VaultAutoSync[] = {{Localize("Replay vault auto sync (GitHub)"), &g_Config.m_MalinkaTasReplayVaultAutoSync}};
	DoMalinkaCheckBoxes(pMenus, Column, VaultAutoSync, std::size(VaultAutoSync));
	static CButtonContainer s_VaultSync;
	if(DoMalinkaButton(pMenus, Column, s_VaultSync, pMenus->MalinkaTasVaultBusy() ? "Cancel Sync" : "Sync Now"))
		pMenus->MalinkaTasSyncNow();
	CUIRect VaultInfo;
	Column.HSplitTop(gs_MalinkaLineSize, &VaultInfo, &Column);
	pMenus->MenuUi()->DoLabel(&VaultInfo, pMenus->MalinkaTasVaultStatus(), 10.0f, TEXTALIGN_ML);
	pMenus->MalinkaTooltip(&s_VaultSync, &VaultInfo, pMenus->MalinkaTasVaultStatus());
	FinishMalinkaSection(Column, s_ScrollState, VaultSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t ToolsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Tools"));
	DoMalinkaSlider(pMenus, Column, &g_Config.m_MalinkaTasTickControlTicks, Localize("Ticks"), 1, 128);
	static const SBoolEntry s_aToolChecks[] = {
		{Localize("Auto Rewind"), &g_Config.m_MalinkaTasAutoRewind},
		{Localize("Auto Forward"), &g_Config.m_MalinkaTasAutoForward},
		{Localize("Auto Pause"), &g_Config.m_MalinkaTickControlPause},
		{Localize("Step"), &g_Config.m_MalinkaTickControlStep},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aToolChecks, std::size(s_aToolChecks));
	FinishMalinkaSection(Column, s_ScrollState, ToolsSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t FakeAimSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Fake Aim"));
	static std::array<const char *, 4> s_aTasFakeAimModes = {
		Localize("Robot Aim"),
		Localize("Spinbot"),
		Localize("Random"),
		Localize("Smooth"),
	};
	static CUi::SDropDownState s_TasFakeAimState;
	static CScrollRegion s_TasFakeAimScroll;
	DoMalinkaDropDown(pMenus, Column, Localize("Mode"), g_Config.m_MalinkaTasFakeAimMode, s_aTasFakeAimModes, s_TasFakeAimState, s_TasFakeAimScroll);
	Column.HSplitTop(gs_MalinkaMarginSmall, nullptr, &Column);
	static CButtonContainer s_AddTasFakeAimButton;
	if(!MalinkaUnavailable(pMenus, Column, "Add fake aim", &g_Config.m_MalinkaTasFakeAim) && DoMalinkaButton(pMenus, Column, s_AddTasFakeAimButton, "Add fake aim"))
		g_Config.m_MalinkaTasFakeAim = 1;
	FinishMalinkaSection(Column, s_ScrollState, FakeAimSection);

	Column.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &Column);
	const size_t VisualsSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Visuals"));
	static const SBoolEntry s_aTasVisualChecks[] = {
		{Localize("Draw start/end pos"), &g_Config.m_MalinkaTasDrawStartEndPos},
		{Localize("Draw replay path"), &g_Config.m_MalinkaTasDrawPath},
		{Localize("Draw prediction path"), &g_Config.m_MalinkaTasDrawPredictionPath},
	};
	DoMalinkaCheckBoxes(pMenus, Column, s_aTasVisualChecks, std::size(s_aTasVisualChecks));
	static std::array<const char *, 2> s_aTasPathModes = {Localize("Dotted"), Localize("Full Line")};
	static CUi::SDropDownState s_TasPathModeState;
	static CScrollRegion s_TasPathModeScroll;
	static CUi::SDropDownState s_TasPredictionPathModeState;
	static CScrollRegion s_TasPredictionPathModeScroll;
	static CButtonContainer s_TasPathColorReset;
	static CButtonContainer s_TasPredictionLocalReset;
	static CButtonContainer s_TasPredictionFrozenReset;
	static CButtonContainer s_TasPredictionOtherReset;
	if(g_Config.m_MalinkaTasDrawPath)
	{
		DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaTasDrawPathSegmented, Localize("Segmented"), &g_Config.m_MalinkaTasDrawPathSegmented, &Column, gs_MalinkaLineSize);
		DoMalinkaDropDown(pMenus, Column, Localize("Mode"), g_Config.m_MalinkaTasDrawPathMode, s_aTasPathModes, s_TasPathModeState, s_TasPathModeScroll);
		DoMalinkaColorPicker(pMenus, Column, s_TasPathColorReset, Localize("Color"), &g_Config.m_MalinkaTasDrawPathColor, ColorRGBA(0.0f, 1.0f, 0.0f));
	}
	if(g_Config.m_MalinkaTasDrawPredictionPath)
	{
		DoMalinkaDropDown(pMenus, Column, Localize("Prediction Mode"), g_Config.m_MalinkaTasDrawPredictionPathMode, s_aTasPathModes, s_TasPredictionPathModeState, s_TasPredictionPathModeScroll);
		DoMalinkaColorPicker(pMenus, Column, s_TasPredictionLocalReset, Localize("Local Color"), &g_Config.m_MalinkaTasDrawPredictionPathColor, ColorRGBA(0.0f, 1.0f, 0.0f));
		DoMalinkaColorPicker(pMenus, Column, s_TasPredictionFrozenReset, Localize("Frozen Color"), &g_Config.m_MalinkaTasDrawPredictionPathColorFrozen, ColorRGBA(0.0f, 1.0f, 1.0f));
		DoMalinkaColorPicker(pMenus, Column, s_TasPredictionOtherReset, Localize("Others Color"), &g_Config.m_MalinkaTasDrawPredictionPathColorOthers, ColorRGBA(0.0f, 0.5f, 0.0f));
	}
	FinishMalinkaSection(Column, s_ScrollState, VisualsSection);

	EndMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);
}

void RenderMalinkaSettings(CMenus *pMenus, CUIRect MainView)
{
	static CMalinkaScrollState s_ScrollState;
	CUIRect LeftView, RightView;
	BeginMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);

	CUIRect Column = LeftView;
	const size_t HotkeysSection = BeginMalinkaSection(pMenus, Column, s_ScrollState, Localize("Hotkeys"));
	static CButtonContainer s_AimbotReader;
	static CButtonContainer s_AimbotClear;
	static CButtonContainer s_EmoteReader;
	static CButtonContainer s_EmoteClear;
	static CButtonContainer s_HookSpamReader;
	static CButtonContainer s_HookSpamClear;
	static CButtonContainer s_AvoidReader;
	static CButtonContainer s_AvoidClear;
	static CButtonContainer s_AutoEdgeReader;
	static CButtonContainer s_AutoEdgeClear;
	static CButtonContainer s_UnfreezeReader;
	static CButtonContainer s_UnfreezeClear;
	pMenus->DoLine_KeyReader(Column, s_AimbotReader, s_AimbotClear, "Aimbot", "toggle malinka_aimbot 1 0");
	pMenus->DoLine_KeyReader(Column, s_EmoteReader, s_EmoteClear, "Emote Spam", "toggle malinka_emotespam 1 0");
	pMenus->DoLine_KeyReader(Column, s_HookSpamReader, s_HookSpamClear, "Hook Spam", "toggle malinka_hookspam 1 0");
	pMenus->DoLine_KeyReader(Column, s_AvoidReader, s_AvoidClear, "Avoid Freeze", "toggle malinka_avoidfreeze 1 0");
	pMenus->DoLine_KeyReader(Column, s_AutoEdgeReader, s_AutoEdgeClear, "Auto Edge", "+toggle malinka_autoedge 1 0");
	pMenus->DoLine_KeyReader(Column, s_UnfreezeReader, s_UnfreezeClear, "Laser Unfreeze Bot", "toggle malinka_unfreezebot 1 0");
	FinishMalinkaSection(Column, s_ScrollState, HotkeysSection);

	CUIRect RightColumn = RightView;
	const size_t SettingsSection = BeginMalinkaSection(pMenus, RightColumn, s_ScrollState, Localize("Settings"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaPredictionTeleports, Localize("Teleport Prediction"), &g_Config.m_MalinkaPredictionTeleports, &RightColumn, gs_MalinkaLineSize);
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaPredictionAdvancedSettings, Localize("Advanced Settings"), &g_Config.m_MalinkaPredictionAdvancedSettings, &RightColumn, gs_MalinkaLineSize);
	DoMalinkaSlider(pMenus, RightColumn, &g_Config.m_ClPredictionMargin, Localize("Prediction Margin"), 0, 400, "ms");
	DoMalinkaSlider(pMenus, RightColumn, &g_Config.m_MalinkaBalanceBotOffset, Localize("Balance Bot Offset"), 0, 20);
	DoMalinkaSlider(pMenus, RightColumn, &g_Config.m_MalinkaHookNearestCollisionFov, Localize("Hook Nearest Fov"), 1, 360);
	if(g_Config.m_MalinkaPredictionAdvancedSettings)
	{
		static const SBoolEntry s_aPredictionChecks[] = {
			{Localize("Death Tile Prediction"), &g_Config.m_MalinkaPredictionDeathTile},
			{Localize("Move Restrictions Prediction"), &g_Config.m_MalinkaPredictionMoveRestriction},
			{Localize("Player Loop (Collision)"), &g_Config.m_MalinkaPredictionPlayers},
			{Localize("Heart Tile Prediction"), &g_Config.m_MalinkaPredictionHeart},
		};
		DoMalinkaCheckBoxes(pMenus, RightColumn, s_aPredictionChecks, std::size(s_aPredictionChecks));
	}
	FinishMalinkaSection(RightColumn, s_ScrollState, SettingsSection);

	RightColumn.HSplitTop(gs_MalinkaSectionSpacing, nullptr, &RightColumn);
	const size_t RpcSection = BeginMalinkaSection(pMenus, RightColumn, s_ScrollState, Localize("Discord RPC"));
	DoMalinkaCheckBox(pMenus, &g_Config.m_MalinkaDiscordRpc, Localize("Enable"), &g_Config.m_MalinkaDiscordRpc, &RightColumn, gs_MalinkaLineSize);
	if(g_Config.m_MalinkaDiscordRpc)
	{
		static std::array<const char *, 4> s_aRpcModes = {"KRX", "DDNet", "TClient", Localize("Custom")};
		static CUi::SDropDownState s_RpcModeState;
		static CScrollRegion s_RpcModeScroll;
		DoMalinkaDropDown(pMenus, RightColumn, Localize("Mode"), g_Config.m_MalinkaDiscordRpcMode, s_aRpcModes, s_RpcModeState, s_RpcModeScroll);
		if(g_Config.m_MalinkaDiscordRpcMode == 3)
		{
			static CLineInput s_CustomRpcInput(g_Config.m_MalinkaDiscordRpcIdCustom, sizeof(g_Config.m_MalinkaDiscordRpcIdCustom));
			DoMalinkaEditBox(pMenus, RightColumn, &s_CustomRpcInput, "Custom RPC ID", "", g_Config.m_MalinkaDiscordRpcIdCustom, sizeof(g_Config.m_MalinkaDiscordRpcIdCustom));
		}
	}
	FinishMalinkaSection(RightColumn, s_ScrollState, RpcSection);

	EndMalinkaScrollColumns(MainView, s_ScrollState, LeftView, RightView);
}
}

bool CMenus::MalinkaTasVaultBusy() const
{
	return GameClient()->m_MalinkaReplayVault.Busy();
}

const char *CMenus::MalinkaTasVaultStatus() const
{
	return GameClient()->m_MalinkaReplayVault.Status();
}

bool CMenus::MalinkaTasImportPending() const
{
	return GameClient()->m_MalinkaTas.ImportedReplayPending();
}

void CMenus::MalinkaTasSaveReplay()
{
	GameClient()->m_MalinkaTas.SaveReplay();
}

void CMenus::MalinkaTasLoadSelectedReplay()
{
	GameClient()->m_MalinkaTas.LoadSelectedReplay();
}

void CMenus::MalinkaTasValidateReplay()
{
	GameClient()->m_MalinkaTas.ValidateReplay();
}

void CMenus::MalinkaTasReportReplayTime()
{
	GameClient()->m_MalinkaTas.ReportReplayTime();
}

void CMenus::MalinkaTasSyncNow()
{
	GameClient()->m_MalinkaTas.SyncNow();
}

void CMenus::MalinkaFentSaveReplay()
{
	GameClient()->m_MalinkaFent.SaveCurrentReplay();
}

void CMenus::HideMalinka()
{
	if(g_Config.m_UiSettingsPage == SETTINGS_MALINKA)
		g_Config.m_UiSettingsPage = SETTINGS_GENERAL;
}
