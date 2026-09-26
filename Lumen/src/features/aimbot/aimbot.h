#pragma once
#include <cache/cache.h>
#include <sdk/math/math.h>

namespace aimbot
{
	inline cache::entity_t player{};

	void run();
	void camera_aimbot();

	// shared by triggerbot / esp overlays
	bool entity_passes_filters(const cache::entity_t& entity);
	bool get_part_screen(const cache::entity_t& entity, const char* part_name,
		const math::matrix4& view, const math::vector2& dims,
		math::vector2& out_screen, math::vector3* out_world = nullptr);
}
