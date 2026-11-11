#ifndef GAME_CLIENT_COMPONENTS_MALINKA_COLORS_H
#define GAME_CLIENT_COMPONENTS_MALINKA_COLORS_H

#include <base/color.h>
#include <base/time.h>

#include <cmath>

namespace Malinka
{
inline ColorRGBA RenderColor(unsigned Packed, bool Rainbow, int Speed)
{
	if(Rainbow)
	{
		const double Seconds = static_cast<double>(time_get()) / time_freq();
		return color_cast<ColorRGBA>(ColorHSLA(static_cast<float>(std::fmod(Seconds * Speed / 100.0, 1.0)), 1.0f, 0.5f));
	}
	return color_cast<ColorRGBA>(ColorHSLA(Packed).UnclampLighting(ColorHSLA::DARKEST_LGT));
}
}

#endif
