#include "esp.h"

#include <cstdio>
#include <cmath>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <game/game.h>
#include <cache/cache.h>
#include <sdk/math/math.h>
#include <features/settings.h>
#include "logger/logger.h"
#include <wallcheck/wallcheck.h>
#include <features/aimbot/aimbot.h>

namespace esp
{
	static double hitmarker_until = 0.0;
	static std::unordered_map<std::uint64_t, float> last_health;

	static double now_seconds()
	{
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}

	__forceinline void outline(ImVec2& c1, ImVec2& c2, ImU32 col, float rounding = 0.f)
	{
		c1.x = std::round(c1.x); c1.y = std::round(c1.y);
		c2.x = std::round(c2.x); c2.y = std::round(c2.y);

		ImDrawList* draw = ImGui::GetBackgroundDrawList();

		ImRect rect_bb(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);

		draw->AddRect(rect_bb.Min, rect_bb.Max, IM_COL32(0, 0, 0, col >> 24), rounding);
		draw->AddRect({ rect_bb.Min.x - 2.f, rect_bb.Min.y - 2.f }, { rect_bb.Max.x + 2.f, rect_bb.Max.y + 2.f }, IM_COL32(0, 0, 0, col >> 24), rounding);
		draw->AddRect({ rect_bb.Min.x - 1.f, rect_bb.Min.y - 1.f }, { rect_bb.Max.x + 1.f, rect_bb.Max.y + 1.f }, col, rounding);
	}

	__forceinline void corner_box(ImVec2 c1, ImVec2 size, ImU32 col, float thickness = 1.5f, float len_ratio = 0.25f)
	{
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		float x = std::round(c1.x), y = std::round(c1.y);
		float w = size.x, h = size.y;
		float lx = w * len_ratio, ly = h * len_ratio;
		ImU32 black = IM_COL32(0, 0, 0, col >> 24);

		auto line = [&](ImVec2 a, ImVec2 b, ImU32 c) { draw->AddLine(a, b, c, thickness); };
		// outer black then inner color for each corner
		ImVec2 corners[4] = { {x, y}, {x + w, y}, {x, y + h}, {x + w, y + h} };
		// TL
		line({ x - 1, y - 1 }, { x + lx + 1, y - 1 }, black); line({ x - 1, y - 1 }, { x - 1, y + ly + 1 }, black);
		line({ x, y }, { x + lx, y }, col); line({ x, y }, { x, y + ly }, col);
		// TR
		line({ x + w - lx - 1, y - 1 }, { x + w + 1, y - 1 }, black); line({ x + w + 1, y - 1 }, { x + w + 1, y + ly + 1 }, black);
		line({ x + w - lx, y }, { x + w, y }, col); line({ x + w, y }, { x + w, y + ly }, col);
		// BL
		line({ x - 1, y + h + 1 }, { x + lx + 1, y + h + 1 }, black); line({ x - 1, y + h - ly - 1 }, { x - 1, y + h + 1 }, black);
		line({ x, y + h }, { x + lx, y + h }, col); line({ x, y + h - ly }, { x, y + h }, col);
		// BR
		line({ x + w - lx - 1, y + h + 1 }, { x + w + 1, y + h + 1 }, black); line({ x + w + 1, y + h - ly - 1 }, { x + w + 1, y + h + 1 }, black);
		line({ x + w - lx, y + h }, { x + w, y + h }, col); line({ x + w, y + h - ly }, { x + w, y + h }, col);
		(void)corners;
	}

	__forceinline void draw_text_outlined(ImDrawList* draw, ImFont* font, float font_size, ImVec2 pos, ImU32 col, const char* text_begin, const char* text_end = nullptr)
	{
		pos.x = std::round(pos.x);
		pos.y = std::round(pos.y);

		ImU32 outline_col = IM_COL32(0, 0, 0, 255);

		for (int x = -1; x <= 1; x++)
		{
			for (int y = -1; y <= 1; y++)
			{
				if (x == 0 && y == 0)
					continue;
				draw->AddText(font, font_size, ImVec2(pos.x + x, pos.y + y), outline_col, text_begin, text_end);
			}
		}

		draw->AddText(font, font_size, pos, col, text_begin, text_end);
	}

	static ImU32 col4(float c[4], float alpha_mul = 1.f)
	{
		return IM_COL32((int)(c[0] * 255.f), (int)(c[1] * 255.f), (int)(c[2] * 255.f), (int)(c[3] * alpha_mul * 255.f));
	}

	static bool part_screen(const cache::entity_t& e, const char* name, const math::matrix4& view, const math::vector2& dims, math::vector2& out)
	{
		auto it = e.parts.find(name);
		if (it == e.parts.end() || !it->second.address) return false;
		math::vector3 w = it->second.get_primitive().get_position();
		return game::visualengine->world_to_screen(view, dims, w, out);
	}

	static void draw_skeleton(const cache::entity_t& e, const math::matrix4& view, const math::vector2& dims, ImU32 col)
	{
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		math::vector2 head, torso, larm, rarm, lleg, rleg, hrp;
		bool has_head = part_screen(e, "Head", view, dims, head);
		bool has_torso = part_screen(e, "Torso", view, dims, torso);
		if (!has_torso) has_torso = part_screen(e, "UpperTorso", view, dims, torso);
		if (!has_torso) has_torso = part_screen(e, "HumanoidRootPart", view, dims, torso);
		bool has_hrp = part_screen(e, "HumanoidRootPart", view, dims, hrp);
		bool has_la = part_screen(e, "Left Arm", view, dims, larm) || part_screen(e, "LeftArm", view, dims, larm);
		bool has_ra = part_screen(e, "Right Arm", view, dims, rarm) || part_screen(e, "RightArm", view, dims, rarm);
		bool has_ll = part_screen(e, "Left Leg", view, dims, lleg) || part_screen(e, "LeftLeg", view, dims, lleg);
		bool has_rl = part_screen(e, "Right Leg", view, dims, rleg) || part_screen(e, "RightLeg", view, dims, rleg);
		if (!has_torso) return;
		auto line = [&](math::vector2 a, math::vector2 b) { draw->AddLine({ a.x, a.y }, { b.x, b.y }, col, 1.5f); };
		if (has_head) line(head, torso);
		if (has_la) line(torso, larm);
		if (has_ra) line(torso, rarm);
		if (has_hrp) line(torso, hrp);
		else if (has_ll || has_rl) { /* torso to legs fallback */ }
		if (has_ll && has_hrp) line(hrp, lleg);
		else if (has_ll) line(torso, lleg);
		if (has_rl && has_hrp) line(hrp, rleg);
		else if (has_rl) line(torso, rleg);
	}
}

void esp::draw_hitmarker_tick()
{
	hitmarker_until = now_seconds() + settings::visuals::hitmarker_time;
}

void esp::draw_fov_circle()
{
	if (!settings::aimbot::draw_fov || settings::aimbot::fov <= 0.f) return;
	POINT c{};
	if (!GetCursorPos(&c)) return;
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	ImVec2 center{ (float)c.x, (float)c.y };
	if (settings::aimbot::fov_filled)
		draw->AddCircleFilled(center, settings::aimbot::fov, IM_COL32(255,255,255, (int)(settings::aimbot::fov_fill_alpha * 255.f)));
	draw->AddCircle(center, settings::aimbot::fov, esp::col4(settings::aimbot::fov_colour), 64, 1.5f);
}

void esp::draw_crosshair()
{
	if (!settings::visuals::crosshair) return;
	POINT c{};
	if (!GetCursorPos(&c)) return;
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	ImU32 col = esp::col4(settings::visuals::crosshair_colour);
	float g = settings::visuals::crosshair_gap, s = settings::visuals::crosshair_size;
	float x = (float)c.x, y = (float)c.y;
	draw->AddLine({ x - g - s, y }, { x - g, y }, col, 1.5f);
	draw->AddLine({ x + g, y }, { x + g + s, y }, col, 1.5f);
	draw->AddLine({ x, y - g - s }, { x, y - g }, col, 1.5f);
	draw->AddLine({ x, y + g }, { x, y + g + s }, col, 1.5f);
}

void esp::draw_radar(const math::matrix4& view, const math::vector2& dims)
{
	if (!settings::visuals::radar) return;
	(void)view; (void)dims;
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	float size = settings::visuals::radar_size;
	ImVec2 pos{ 20, 60 };
	ImVec2 center{ pos.x + size * 0.5f, pos.y + size * 0.5f };
	draw->AddRectFilled(pos, { pos.x + size, pos.y + size }, esp::col4(settings::visuals::radar_bg));
	draw->AddRect(pos, { pos.x + size, pos.y + size }, IM_COL32(255,255,255,60));
	draw->AddLine({ center.x - size*0.5f, center.y }, { center.x + size*0.5f, center.y }, IM_COL32(255,255,255,30));
	draw->AddLine({ center.x, center.y - size*0.5f }, { center.x, center.y + size*0.5f }, IM_COL32(255,255,255,30));

	auto local_it = cache::local_player.parts.find("HumanoidRootPart");
	if (local_it == cache::local_player.parts.end() || !local_it->second.address) return;
	math::vector3 local_pos = local_it->second.get_primitive().get_position();

	std::vector<cache::entity_t> snap;
	{ std::lock_guard<std::mutex> l(cache::mtx); snap = cache::players; }

	float range = max(settings::visuals::radar_range, 1.f) * max(settings::visuals::radar_zoom, 0.1f);
	for (auto& e : snap) {
		if (e.instance.address == cache::local_player.instance.address) continue;
		if (settings::visuals::teamcheck && e.team == cache::local_player.team) continue;
		if (settings::visuals::deadcheck && e.health <= 0) continue;
		auto it = e.parts.find("HumanoidRootPart");
		if (it == e.parts.end() || !it->second.address) continue;
		math::vector3 p = it->second.get_primitive().get_position();
		math::vector3 d = p - local_pos;
		float dx = d.x / range * (size * 0.5f);
		float dy = d.z / range * (size * 0.5f);
		float len = std::sqrt(dx*dx + dy*dy);
		float maxr = size * 0.5f - 4.f;
		if (len > maxr) { dx *= maxr / len; dy *= maxr / len; }
		ImU32 c = settings::visuals::team_colour_override && e.team == cache::local_player.team
			? IM_COL32(80, 160, 255, 255) : esp::col4(settings::visuals::colour);
		draw->AddCircleFilled({ center.x + dx, center.y + dy }, 3.f, c);
	}
	// local player wedge
	draw->AddCircleFilled(center, 3.f, IM_COL32(255,255,255,255));
}

void esp::run()
{
	math::matrix4 view = game::visualengine->get_viewmatrix();
	math::vector2 dims = game::visualengine->get_dimensions();

	draw_fov_circle();
	draw_crosshair();
	draw_radar(view, dims);

	// hitmarker center
	if (settings::visuals::hitmarker && now_seconds() < hitmarker_until) {
		POINT c{};
		if (GetCursorPos(&c)) {
			ImDrawList* draw = ImGui::GetBackgroundDrawList();
			ImU32 col = esp::col4(settings::visuals::hitmarker_colour);
			float s = settings::visuals::hitmarker_size, g = 3.f;
			float x = (float)c.x, y = (float)c.y;
			draw->AddLine({ x - g - s, y - g - s }, { x - g, y - g }, col, 2.f);
			draw->AddLine({ x + g, y - g }, { x + g + s, y - g - s }, col, 2.f);
			draw->AddLine({ x - g - s, y + g + s }, { x - g, y + g }, col, 2.f);
			draw->AddLine({ x + g, y + g }, { x + g + s, y + g + s }, col, 2.f);
		}
	}

	static math::vector3 local_corners[8] =
	{
		{ -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 },
		{ -1, -1, 1 }, { 1, -1, 1 }, { -1, 1, 1 }, { 1, 1, 1 }
	};

	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	draw->Flags &= ImDrawListFlags_AntiAliasedLines;

	std::vector<cache::entity_t> players_snapshot;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		players_snapshot = cache::players;
	}

	// hitmarker health tracking
	if (settings::visuals::hitmarker) {
		for (auto& e : players_snapshot) {
			if (!e.instance.address) continue;
			auto it = last_health.find(e.instance.address);
			if (it != last_health.end() && e.health < it->second - 0.5f)
				draw_hitmarker_tick();
			last_health[e.instance.address] = e.health;
		}
	}

	// snapline origin
	POINT cursor{};
	GetCursorPos(&cursor);
	ImVec2 screen_origin{ 0, 0 };
	{
		int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
		switch (settings::visuals::snapline_origin) {
		case 0: screen_origin = { (float)sw * 0.5f, (float)sh }; break;
		case 1: screen_origin = { (float)sw * 0.5f, 0 }; break;
		case 2: screen_origin = { (float)sw * 0.5f, (float)sh * 0.5f }; break;
		default: screen_origin = { (float)cursor.x, (float)cursor.y }; break;
		}
	}

	for (auto& entity : players_snapshot)
	{
		if (entity.instance.address == cache::local_player.instance.address)
			continue;

		// distance culling
		if (settings::visuals::distance_culling) {
			auto local_it = cache::local_player.parts.find("HumanoidRootPart");
			if (local_it != cache::local_player.parts.end() && local_it->second.address) {
				auto target_it = entity.parts.find("HumanoidRootPart");
				if (target_it != entity.parts.end() && target_it->second.address) {
					float dist = local_it->second.get_primitive().get_position().distance(
						target_it->second.get_primitive().get_position());
					if (dist > settings::visuals::max_render_distance)
						continue;
				}
			}
		}

		bool valid = false;
		float left = FLT_MAX, top = FLT_MAX;
		float right = -FLT_MAX, bottom = -FLT_MAX;

		for (auto& pair : entity.parts)
		{
			if (!pair.second.address)
				continue;

			if (pair.first == "HumanoidRootPart")
				continue;

			rbx::c_primitive prim = pair.second.get_primitive();
			math::vector3 size = prim.get_size();
			math::vector3 pos = prim.get_position();
			math::matrix3 rot = prim.get_rotation();

			if (size.x == 0.f && size.y == 0.f && size.z == 0.f)
				continue;

			for (math::vector3& lc : local_corners)
			{
				math::vector3 world = pos + rot * math::vector3
				{
					lc.x * size.x * 0.5f,
					lc.y * size.y * 0.5f,
					lc.z * size.z * 0.5f
				};

				math::vector2 out;
				if (game::visualengine->world_to_screen(view, dims, world, out))
				{
					valid = true;
					left = min(left, out.x);
					top = min(top, out.y);
					right = max(right, out.x);
					bottom = max(bottom, out.y);
				}
			}
		}

		bool onscreen = valid && left < right && top < bottom;

		// offscreen arrows even when box invalid
		if (!onscreen && settings::visuals::offscreen_arrows) {
			auto it = entity.parts.find("HumanoidRootPart");
			if (it != entity.parts.end() && it->second.address) {
				math::vector3 w = it->second.get_primitive().get_position();
				math::vector2 s{};
				bool vis = game::visualengine->world_to_screen(view, dims, w, s);
				float cx = (float)GetSystemMetrics(SM_CXSCREEN) * 0.5f;
				float cy = (float)GetSystemMetrics(SM_CYSCREEN) * 0.5f;
				float dx, dy;
				if (vis) {
					dx = s.x - cx;
					dy = s.y - cy;
				} else {
					math::vector3 dir = (w - memory->read<math::vector3>(game::camera + Offsets::Camera::Position)).normalized();
					dx = dir.x;
					dy = dir.y;
				}
				float len = std::sqrt(dx*dx + dy*dy);
				if (len < 1.f) { dx = 0; dy = -1; len = 1; }
				dx /= len; dy /= len;
				float dist = settings::visuals::arrow_distance + 60.f;
				ImVec2 tip{ cx + dx * dist, cy + dy * dist };
				float sz = settings::visuals::arrow_size;
				float ang = std::atan2(dy, dx);
				ImVec2 p1{ tip.x + std::cos(ang + 2.5f) * sz, tip.y + std::sin(ang + 2.5f) * sz };
				ImVec2 p2{ tip.x + std::cos(ang - 2.5f) * sz, tip.y + std::sin(ang - 2.5f) * sz };
				draw->AddTriangleFilled(tip, p1, p2, esp::col4(settings::visuals::arrow_colour));
			}
			continue;
		}

		if (!onscreen)
			continue;

		ImVec2 c1(left, top);
		ImVec2 c2(right - left, bottom - top);

		if (settings::visuals::teamcheck)
		{
			if (entity.team == cache::local_player.team)
				continue;
		}

		if (settings::visuals::deadcheck)
		{
			if (entity.health <= 0)
				continue;
		}

		auto local_root_it = cache::local_player.parts.find("HumanoidRootPart");
		bool has_local_pos = local_root_it != cache::local_player.parts.end() && local_root_it->second.address;

		float transparency = 255.f;
		if (settings::visuals::wallcheck && has_local_pos)
		{
			math::vector3 local_position = local_root_it->second.get_primitive().get_position();
			math::vector3 target_position = entity.humanoid_root_part.get_primitive().get_position();

			wallcheck->is_visible(local_position, target_position) ? transparency = 255.f : transparency = 50.f;
		}

		ImU32 box_col = IM_COL32(
			(int)(settings::visuals::colour[0] * 255.f),
			(int)(settings::visuals::colour[1] * 255.f),
			(int)(settings::visuals::colour[2] * 255.f),
			(int)(settings::visuals::colour[3] * transparency));

		if (settings::visuals::team_colour_override && entity.team == cache::local_player.team)
			box_col = IM_COL32(80, 160, 255, (int)transparency);

		if (settings::visuals::box_fill) {
			draw->AddRectFilled({ c1.x, c1.y }, { c1.x + c2.x, c1.y + c2.y },
				IM_COL32(255,255,255, (int)(settings::visuals::box_fill_alpha * (transparency / 255.f) * 255.f)));
		}

		if (settings::visuals::box)
		{
			if (settings::visuals::box_style == 1)
				esp::corner_box(c1, c2, box_col);
			else
				esp::outline(c1, c2, box_col);
		}

		if (settings::visuals::skeleton)
			draw_skeleton(entity, view, dims, esp::col4(settings::visuals::skeleton_colour, transparency / 255.f));

		if (settings::visuals::snapline) {
			ImVec2 dst{ c1.x + c2.x * 0.5f, c1.y + c2.y };
			draw->AddLine(screen_origin, dst, esp::col4(settings::visuals::snapline_colour, transparency / 255.f), 1.2f);
		}

		if (settings::visuals::head_dot) {
			math::vector2 hs{};
			if (part_screen(entity, "Head", view, dims, hs))
				draw->AddCircleFilled({ hs.x, hs.y }, settings::visuals::head_dot_size, esp::col4(settings::visuals::head_dot_colour, transparency / 255.f));
		}

		if (settings::visuals::healthbar)
		{
			float health_percent = 0.f;
			if (entity.max_health > 0.f)
			{
				health_percent = entity.health / entity.max_health;
				if (health_percent < 0.f) health_percent = 0.f;
				if (health_percent > 1.f) health_percent = 1.f;
			}

			float box_left = std::round(c1.x);
			float box_top = std::round(c1.y);
			float box_bottom = std::round(c1.y + c2.y);

			draw->AddRectFilled(ImVec2(box_left - 6.f, box_top - 2.f), ImVec2(box_left - 3.f, box_bottom + 2.f), IM_COL32(0, 0, 0, 255));
			if (health_percent > 0.f)
			{
				draw->AddRectFilled(ImVec2(box_left - 5.f, (box_bottom + 1.f) - ((box_bottom - box_top + 2.f) * health_percent)), ImVec2(box_left - 4.f, box_bottom + 1.f), IM_COL32(settings::visuals::healthbar_colour[0] * 255.f, settings::visuals::healthbar_colour[1] * 255.f, settings::visuals::healthbar_colour[2] * 255.f, settings::visuals::healthbar_colour[3] * transparency));
			}

			if (settings::visuals::health_text) {
				char hp[16]{};
				std::snprintf(hp, sizeof(hp), "%d", (int)entity.health);
				ImFont* font = ImGui::GetFont();
				draw->AddText(font, ImGui::GetFontSize(), { box_left - 6.f, box_bottom + 3.f }, IM_COL32(255,255,255, (int)transparency), hp);
			}
		}

		if (settings::visuals::username)
		{
			ImFont* font = ImGui::GetFont();
			float font_size = ImGui::GetFontSize();
			float text_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, entity.name.c_str()).x;

			esp::draw_text_outlined(draw, font, font_size,
				ImVec2(std::round(c1.x + (c2.x * 0.5f) - (text_width * 0.5f)), std::round(c1.y) - 2.f - 2.f - 1.f - font_size),
				IM_COL32(settings::visuals::username_colour[0] * 255.f, settings::visuals::username_colour[1] * 255.f, settings::visuals::username_colour[2] * 255.f, settings::visuals::username_colour[3] * transparency),
				entity.name.c_str());
		}

		if (settings::visuals::distance)
		{
			auto target_it = entity.parts.find("HumanoidRootPart");

			if (has_local_pos && target_it != entity.parts.end() && target_it->second.address)
			{
				char distance_str[32];
				std::snprintf(distance_str, sizeof(distance_str), "%.0f", local_root_it->second.get_primitive().get_position().distance(target_it->second.get_primitive().get_position()));

				ImFont* font = ImGui::GetFont();
				float font_size = ImGui::GetFontSize();
				float text_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, distance_str).x;

				esp::draw_text_outlined(draw, font, font_size,
					ImVec2(std::round(c1.x + (c2.x * 0.5f) - (text_width * 0.5f)), std::round(c1.y + c2.y) + 2.f + 2.f + 1.f),
					IM_COL32(settings::visuals::distance_colour[0] * 255.f, settings::visuals::distance_colour[1] * 255.f, settings::visuals::distance_colour[2] * 255.f, settings::visuals::distance_colour[3] * transparency),
					distance_str);
			}
		}

		if (settings::visuals::debug_wallcheck)
		{
			math::vector3 localpos = cache::local_player.parts["HumanoidRootPart"].get_primitive().get_position();

			for (auto& obb : wallcheck->get_obstacles()) {
				float distance = (obb.center - localpos).length();
				if (distance > settings::visuals::debug_wallcheck_max_length) {
					continue;
				}

				math::vector3 hx = obb.axes[0] * obb.half_size.x;
				math::vector3 hy = obb.axes[1] * obb.half_size.y;
				math::vector3 hz = obb.axes[2] * obb.half_size.z;

				math::vector3 corners[8] = {
					obb.center + hx + hy + hz,
					obb.center - hx + hy + hz,
					obb.center + hx - hy + hz,
					obb.center - hx - hy + hz,
					obb.center + hx + hy - hz,
					obb.center - hx + hy - hz,
					obb.center + hx - hy - hz,
					obb.center - hx - hy - hz
				};

				std::int32_t edges[12][2] = {
					{0,1},{1,3},{3,2},{2,0}, // bottom
					{4,5},{5,7},{7,6},{6,4}, // top
					{0,4},{1,5},{2,6},{3,7}  // sides
				};

				for (int i = 0; i < 12; i++) {
					math::vector2 p1{};
					math::vector2 p2{};

					if (!game::visualengine->world_to_screen(view, dims, corners[edges[i][0]], p1))
					{
						continue;
					}

					if (!game::visualengine->world_to_screen(view, dims, corners[edges[i][1]], p2))
					{
						continue;
					}

					if (p1.x != -1 && p1.y != -1 && p2.x != -1 && p2.y != -1) {
						ImGui::GetForegroundDrawList()->AddLine({ p1.x, p1.y }, { p2.x, p2.y }, IM_COL32(0, 255, 200, 255));
					}
				}
			}
		}
	}
}
