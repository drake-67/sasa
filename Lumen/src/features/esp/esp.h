#pragma once
#include <mutex>
#include <sdk/math/math.h>

namespace esp
{
	void run();
	void draw_fov_circle();
	void draw_crosshair();
	void draw_radar(const math::matrix4& view, const math::vector2& dims);
	void draw_hitmarker_tick(); // call when target dies / health drops
}
