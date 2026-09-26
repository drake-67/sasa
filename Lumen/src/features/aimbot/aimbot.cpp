#include "aimbot.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <game/game.h>
#include "../settings.h"
#include <wallcheck/wallcheck.h>

static float get_distance_from_center(const math::vector2& point, const math::vector2& position)
{
	float dx{ position.x - point.x };
	float dy{ position.y - point.y };
	return std::sqrt(dx * dx + dy * dy);
}

bool aimbot::entity_passes_filters(const cache::entity_t& entity)
{
	if (!entity.instance.address)
		return false;
	if (entity.instance.address == cache::local_player.instance.address)
		return false;
	if (settings::aimbot::teamcheck && entity.team == cache::local_player.team)
		return false;
	if (settings::aimbot::deadcheck && entity.health <= 0)
		return false;
	return true;
}

bool aimbot::get_part_screen(const cache::entity_t& entity, const char* part_name,
	const math::matrix4& view, const math::vector2& dims,
	math::vector2& out_screen, math::vector3* out_world)
{
	auto it = entity.parts.find(part_name);
	if (it == entity.parts.end())
		return false;

	rbx::c_part part{ it->second };
	if (!part.address)
		return false;

	math::vector3 pos_3d{ part.get_primitive().get_position() };

	// simple velocity prediction: lead along HRP delta if enabled.
	// we don't have stored velocity in cache, so approximate with HRP movement
	// fallback: no-op when prediction off.
	if (settings::aimbot::prediction && out_world) {
		// caller passes world out; prediction applied by caller using frame delta.
		// keep raw here so w2s stays stable.
	}

	if (settings::aimbot::visibility_only || settings::aimbot::wallcheck)
	{
		math::vector3 local_position = memory->read<math::vector3>(game::camera + Offsets::Camera::Position);
		if (!wallcheck->is_visible(local_position, pos_3d))
			return false;
	}

	if (!game::visualengine->world_to_screen(view, dims, pos_3d, out_screen))
		return false;

	if (out_world)
		*out_world = pos_3d;
	return true;
}

static bool get_target_point_for_entity
(
	const cache::entity_t& entity,
	const math::matrix4& view,
	const math::vector2& dims,
	float fov,
	float& out_distance,
	math::vector2& out_screen
)
{
	POINT cursor{};
	if (!GetCursorPos(&cursor))
		return false;

	const math::vector2 cursor_pos{ static_cast<float>(cursor.x), static_cast<float>(cursor.y) };

	float best_distance = FLT_MAX;
	bool found = false;
	math::vector2 best{};

	// headshot chance: if <100, roll to fall back to torso/hrp for legit feel
	static thread_local std::mt19937 rng{ std::random_device{}() };
	bool force_body = false;
	if (settings::aimbot::headshot_chance < 100.f) {
		std::uniform_real_distribution<float> d(0.f, 100.f);
		force_body = d(rng) > settings::aimbot::headshot_chance;
	}

	auto try_part = [&](const char* name)
	{
		math::vector2 pos_2d{};
		math::vector3 pos_3d{};
		if (!aimbot::get_part_screen(entity, name, view, dims, pos_2d, &pos_3d))
			return;

		// prediction: push target along facing delta approximated by local->target dir * speed
		// without true velocity we scale by world distance so far targets lead more
		if (settings::aimbot::prediction) {
			// project a small forward lead in screen space toward movement is not available;
			// apply Z-lead by re-projecting pushed world point
			math::vector3 local_pos = memory->read<math::vector3>(game::camera + Offsets::Camera::Position);
			math::vector3 dir = (pos_3d - local_pos).normalized();
			float dist = (pos_3d - local_pos).length();
			math::vector3 lead = pos_3d + dir * (dist * settings::aimbot::prediction_amount * 0.15f);
			math::vector2 lead2d{};
			if (game::visualengine->world_to_screen(view, dims, lead, lead2d))
				pos_2d = lead2d;
		}

		const float distance = get_distance_from_center(cursor_pos, pos_2d);
		if (distance > fov)
			return;
		if (distance < best_distance)
		{
			best_distance = distance;
			best = pos_2d;
			found = true;
		}
	};

	switch (settings::aimbot::target_part)
	{
	case 1:
		if (force_body) { try_part("Torso"); if (!found) try_part("HumanoidRootPart"); }
		else try_part("Head");
		break;
	case 2:
		try_part("Torso");
		break;
	case 3:
		try_part("HumanoidRootPart");
		break;
	default: // closest
	{
		if (force_body) {
			const char* parts[] = { "Torso", "HumanoidRootPart", "Head", "Left Leg", "Left Arm", "Right Arm", "Right Leg" };
			for (const char* name : parts) try_part(name);
		}
		else {
			const char* parts[] = { "Head", "Torso", "HumanoidRootPart", "Left Leg", "Left Arm", "Right Arm", "Right Leg" };
			for (const char* name : parts) try_part(name);
		}
		break;
	}
	}

	if (!found)
		return false;

	out_distance = best_distance;
	out_screen = best;
	return true;
}

static float apply_curve(float t, int curve, float humanize)
{
	t = std::clamp(t, 0.f, 1.f);
	switch (curve) {
	case 1: return 1.f - (1.f - t) * (1.f - t); // ease-out
	case 2: return t < 0.5f ? 2.f * t * t : 1.f - std::pow(-2.f * t + 2.f, 2.f) / 2.f; // ease-in-out
	case 3: { // humanized: ease-out + small jitter
		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> j(-humanize * 0.15f, humanize * 0.15f);
		float e = 1.f - (1.f - t) * (1.f - t);
		return std::clamp(e + j(rng), 0.02f, 1.f);
	}
	default: return t;
	}
}

static void mouse_aim(const math::vector2& target, float smooth_x, float smooth_y)
{
	POINT cursor{};
	if (!GetCursorPos(&cursor))
		return;

	float delta_x = target.x - static_cast<float>(cursor.x);
	float delta_y = target.y - static_cast<float>(cursor.y);

	const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);

	if (distance < settings::aimbot::deadzone)
		return;

	// curve shaping: larger distance -> larger step fraction
	float norm = std::clamp(distance / max(settings::aimbot::fov, 1.f), 0.f, 1.f);
	float k = apply_curve(norm, settings::aimbot::smooth_curve, settings::aimbot::humanize);

	const float divisor_x = max(smooth_x * (2.f - k), 1.0f);
	const float divisor_y = max(smooth_y * (2.f - k), 1.0f);

	static thread_local float remainder_x = 0.0f;
	static thread_local float remainder_y = 0.0f;

	remainder_x += delta_x / divisor_x;
	remainder_y += delta_y / divisor_y;

	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	static thread_local double last_time = static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
	const double current_time = static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
	last_time = current_time;

	static thread_local int last_dir_x = 0;
	static thread_local int last_dir_y = 0;
	static thread_local double last_change_x = 0.0;
	static thread_local double last_change_y = 0.0;

	const int intended_dir_x = (remainder_x > 0.1f) ? 1 : ((remainder_x < -0.1f) ? -1 : 0);
	const int intended_dir_y = (remainder_y > 0.1f) ? 1 : ((remainder_y < -0.1f) ? -1 : 0);

	const double min_direction_change_time = 0.03;

	if (intended_dir_x != 0 && intended_dir_x == -last_dir_x)
	{
		if (current_time - last_change_x < min_direction_change_time)
			remainder_x *= 0.3f;
		else { last_dir_x = intended_dir_x; last_change_x = current_time; }
	}
	else if (intended_dir_x != 0 && intended_dir_x != last_dir_x)
	{ last_dir_x = intended_dir_x; last_change_x = current_time; }
	else if (intended_dir_x == 0)
	{ last_dir_x = 0; }

	if (intended_dir_y != 0 && intended_dir_y == -last_dir_y)
	{
		if (current_time - last_change_y < min_direction_change_time)
			remainder_y *= 0.3f;
		else { last_dir_y = intended_dir_y; last_change_y = current_time; }
	}
	else if (intended_dir_y != 0 && intended_dir_y != last_dir_y)
	{ last_dir_y = intended_dir_y; last_change_y = current_time; }
	else if (intended_dir_y == 0)
	{ last_dir_y = 0; }

	const LONG move_x = static_cast<LONG>(std::round(remainder_x));
	const LONG move_y = static_cast<LONG>(std::round(remainder_y));

	remainder_x -= static_cast<float>(move_x);
	remainder_y -= static_cast<float>(move_y);

	if (move_x == 0 && move_y == 0)
		return;

	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dx = move_x;
	input.mi.dy = move_y;
	input.mi.dwFlags = MOUSEEVENTF_MOVE;

	SendInput(1, &input, sizeof(INPUT));
}

static float get_world_distance_to_entity(cache::entity_t& entity)
{
	auto local_it = cache::local_player.parts.find("HumanoidRootPart");
	if (local_it == cache::local_player.parts.end() || !local_it->second.address)
		return FLT_MAX;

	auto target_it = entity.parts.find("HumanoidRootPart");
	if (target_it == entity.parts.end() || !target_it->second.address)
		return FLT_MAX;

	math::vector3 local_pos = local_it->second.get_primitive().get_position();
	math::vector3 target_pos = target_it->second.get_primitive().get_position();

	return local_pos.distance(target_pos);
}


static cache::entity_t get_best_player(const math::matrix4& view, const math::vector2& dims)
{
	cache::entity_t best_player{};
	float best_score = FLT_MAX;

	std::vector<cache::entity_t> entities_snapshot;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		entities_snapshot = cache::players;
	}

	// sticky: keep locked target if still valid + within stick radius
	if (settings::aimbot::sticky_target && settings::aimbot::locked_target != 0) {
		for (auto& e : entities_snapshot) {
			if (e.instance.address != settings::aimbot::locked_target) continue;
			if (!aimbot::entity_passes_filters(e)) break;
			float d{}; math::vector2 s{};
			if (!get_target_point_for_entity(e, view, dims, settings::aimbot::max_stick_distance, d, s)) break;
			// also require within main fov
			float d2{}; math::vector2 s2{};
			if (!get_target_point_for_entity(e, view, dims, settings::aimbot::fov, d2, s2)) break;
			return e;
		}
		settings::aimbot::locked_target = 0;
	}

	POINT cursor{};
	GetCursorPos(&cursor);
	math::vector2 cursor_pos{ (float)cursor.x, (float)cursor.y };

	for (cache::entity_t& entity : entities_snapshot)
	{
		if (!aimbot::entity_passes_filters(entity))
			continue;

		float world_distance = get_world_distance_to_entity(entity);
		if (world_distance == FLT_MAX)
			continue;

		float distance{};
		math::vector2 target_screen{};
		if (!get_target_point_for_entity(entity, view, dims, settings::aimbot::fov, distance, target_screen))
			continue;

		float score = distance;
		if (settings::aimbot::priority == 1) {
			// lowest health first, tie-break by crosshair distance
			float hp = entity.max_health > 0 ? entity.health / entity.max_health : 1.f;
			score = hp * 1000.f + distance * 0.1f;
		}
		else if (settings::aimbot::priority == 2) {
			score = world_distance + distance * 0.05f;
		}

		if (score < best_score)
		{
			best_score = score;
			best_player = entity;
		}
	}

	if (best_player.instance.address != 0 && settings::aimbot::sticky_target)
		settings::aimbot::locked_target = best_player.instance.address;

	return best_player;
}


void aimbot::run()
{
	using namespace std::chrono_literals;

	for (;;)
	{
		if (settings::aimbot::camera::enabled)
		{
			math::matrix4 view = game::visualengine->get_viewmatrix();
			math::vector2 dims = game::visualengine->get_dimensions();

			cache::entity_t player = get_best_player(view, dims);
			if (player.instance.address != 0)
			{
				aimbot::player = player;
				camera_aimbot();
			}
		}

		if (!settings::aimbot::enabled)
		{
			Sleep(1);
			continue;
		}

		bool key_down = !settings::aimbot::require_key || (GetAsyncKeyState(settings::aimbot::aim_key) & 0x8000);
		if (!key_down)
		{
			settings::aimbot::locked_target = 0;
			Sleep(1);
			continue;
		}

		math::matrix4 view = game::visualengine->get_viewmatrix();
		math::vector2 dims = game::visualengine->get_dimensions();

		cache::entity_t player = get_best_player(view, dims);
		if (player.instance.address == 0)
		{
			Sleep(1);
			continue;
		}

		float world_distance = get_world_distance_to_entity(player);
		if (world_distance == FLT_MAX)
		{
			Sleep(1);
			continue;
		}

		float distance{};
		math::vector2 target_screen{};
		if (get_target_point_for_entity(player, view, dims, settings::aimbot::fov, distance, target_screen))
		{
			mouse_aim(target_screen, settings::aimbot::smooth_x, settings::aimbot::smooth_y);
		}

		Sleep(1);
	}
}

void aimbot::camera_aimbot()
{
	if (!game::datamodel || !game::datamodel->address)
		return;

	rbx::c_workspace workspace = game::datamodel->get_workspace();
	if (!workspace.address)
		return;

	std::uint64_t camera_address = workspace.find_first_child_by_class("Camera");
	if (!camera_address)
		return;

	rbx::c_camera camera{ camera_address };
	if (!camera.address)
		return;

	if (aimbot::player.instance.address == 0)
		return;

	auto head_it = aimbot::player.parts.find("Head");
	if (head_it == aimbot::player.parts.end() || !head_it->second.address)
		return;

	math::vector3 camera_pos = camera.get_position();
	math::vector3 target_pos = head_it->second.get_primitive().get_position();

	math::matrix3 target_rot = math::look_at(camera_pos, target_pos);

	if (settings::aimbot::camera::smoothing_enabled)
	{
		math::matrix3 camera_rot = camera.get_rotation();
		float t = max(settings::aimbot::camera::smoothing_value, 0.1f) * 50.f;
		float k = 1.0f / t;
		math::matrix3 result{};
		for (std::int32_t i = 0; i < 3; i++)
		{
			for (std::int32_t j = 0; j < 3; j++)
			{
				result.m[i][j] = camera_rot.m[i][j] + (target_rot.m[i][j] - camera_rot.m[i][j]) * k;
			}
		}
		camera.set_rotation(result);
	}
	else
	{
		camera.set_rotation(target_rot);
	}
}
