// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_MALINKA_VISUALS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_MALINKA_VISUALS_H

#include <engine/graphics.h>
#include <engine/warning.h>

#include <game/client/component.h>

#include <deque>
#include <string>
#include <vector>

class CProjectileData;

class CMalinkaVisuals : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

	void PushNotification(const SWarning &Warning);
	void RenderBulletLine(const CProjectileData &Projectile, float Time, float Curvature, float Speed, float Alpha);

private:
	struct SNotification
	{
		char m_aTitle[128];
		char m_aMessage[256];
		double m_StartTime;
		double m_Duration;
	};

	std::deque<SNotification> m_vNotifications;
	bool m_MalinkaThemeActive = false;
	unsigned m_StoredUiColor = 0;

	void UpdateMenuTheme();
	void RenderWorldVisuals();
	void RenderTileOutlines();
	void RenderAimbotOverlay();
	void RenderAimLines();
	void RenderTrajectory();
	void RenderAutoEdge();
	void RenderSelfHitEsp();
	void RenderFeaturesHud(float Width, float Height);
	void RenderFrozenHud(float Width, float Height);
	void RenderWatermark(float Width, float Height);
	void RenderNotifications(float Width, float Height);
};

#endif
