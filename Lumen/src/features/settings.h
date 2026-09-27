#pragma once
#include <imgui/imgui.h>
#include <windows.h>
#include <string>

namespace settings
{
	// SASA global
	inline float menu_accent[4] = { 0.65f, 0.30f, 1.0f, 1.0f }; // purple
	inline int menu_key = VK_HOME;
	inline bool show_fps = true;
	inline bool show_entity_count = true;
	inline float overlay_fps_limit = 60.f; // 0 = unlimited (uncapped costs CPU)

	namespace aimbot
	{
		inline bool enabled = false;
		inline int aim_key = VK_XBUTTON2; // configurable
		inline bool require_key = true;   // hold-to-aim vs always-on

		inline int target_part = 0; // 0 closest, 1 head, 2 torso, 3 hrp
		inline int priority = 0;    // 0 closest-to-crosshair, 1 lowest-health, 2 closest-world

		inline bool sticky_target = true;
		inline float max_stick_distance = 150.f; // px, how far sticky can drift
		inline std::uint64_t locked_target = 0;  // internal

		inline float fov = 120.f;
		inline bool draw_fov = true;
		inline float fov_colour[4] = { 1.f, 1.f, 1.f, 0.55f };
		inline bool fov_filled = false;
		inline float fov_fill_alpha = 0.08f;

		inline float smooth_x = 10.f;
		inline float smooth_y = 10.f;
		inline int smooth_curve = 0; // 0 linear, 1 ease-out, 2 ease-in-out, 3 humanized
		inline float humanize = 0.35f; // 0-1 jitter amount for humanized curve
		inline float deadzone = 3.f;
		inline float headshot_chance = 100.f; // 0-100, legit feel: % of time force head

		inline bool prediction = false;
		inline float prediction_amount = 0.13f; // seconds of velocity lead (uses HRP velocity if available)
		inline bool visibility_only = false; // only aim at wallcheck-visible

		inline bool teamcheck = false;
		inline bool deadcheck = true;
		inline bool wallcheck = false;

		namespace camera {
			inline bool enabled = false;
			inline bool smoothing_enabled = true;
			inline float smoothing_value = 8.f; // 0.1-50
		}
	}

	namespace triggerbot
	{
		inline bool enabled = false;
		inline int key = VK_XBUTTON1;
		inline bool require_key = true;
		inline float delay_ms = 80.f;
		inline float fov_radius = 12.f; // px around crosshair
		inline bool teamcheck = true;
		inline bool deadcheck = true;
		inline bool wallcheck = true;
	}

	namespace visuals
	{
		inline bool streamproof = true;

		// box styles: 0 full, 1 corner, 2 3d-ish (uses 2d bounds + corner)
		inline bool box = true;
		inline int box_style = 1;
		inline float colour[4] = { 0.65f, 0.30f, 1.0f, 1.0f };
		inline bool box_fill = false;
		inline float box_fill_alpha = 0.15f;
		inline bool team_colour_override = false;

		inline bool username = true;
		inline float username_colour[4] = { 1.f, 1.f, 1.f, 1.f };
		inline bool distance = false;
		inline float distance_colour[4] = { 0.7f, 0.7f, 0.7f, 1.f };
		inline bool healthbar = true;
		inline float healthbar_colour[4] = { 0.f, 1.f, 0.f, 1.f };
		inline bool health_text = false;

		inline bool skeleton = false;
		inline float skeleton_colour[4] = { 1.f, 1.f, 1.f, 1.f };
		inline bool snapline = false;
		inline int snapline_origin = 0; // 0 bottom, 1 top, 2 center, 3 crosshair
		inline float snapline_colour[4] = { 1.f, 1.f, 1.f, 0.6f };
		inline bool head_dot = false;
		inline float head_dot_colour[4] = { 1.f, 0.2f, 0.2f, 1.f };
		inline float head_dot_size = 4.f;

		inline bool offscreen_arrows = false;
		inline float arrow_colour[4] = { 1.f, 1.f, 1.f, 0.9f };
		inline float arrow_size = 12.f;
		inline float arrow_distance = 120.f; // px from center

		inline bool crosshair = false;
		inline float crosshair_colour[4] = { 1.f, 1.f, 1.f, 0.8f };
		inline float crosshair_size = 8.f;
		inline float crosshair_gap = 4.f;

		inline bool hitmarker = false;
		inline float hitmarker_colour[4] = { 1.f, 1.f, 1.f, 1.f };
		inline float hitmarker_size = 8.f;
		inline float hitmarker_time = 0.4f; // seconds shown

		inline bool radar = false;
		inline float radar_size = 150.f;
		inline float radar_range = 150.f; // studs
		inline float radar_zoom = 1.f;
		inline float radar_bg[4] = { 0.05f, 0.05f, 0.08f, 0.85f };

		inline bool teamcheck = false;
		inline bool deadcheck = true;
		inline bool wallcheck = false;

		inline bool distance_culling = false;
		inline float max_render_distance = 500.f;

		inline bool debug_wallcheck = false;
		inline float debug_wallcheck_max_length = 75.f;
	}

	namespace legit
	{
		inline bool enabled = false;
		inline float walkspeed = 16.f;
		inline float jumppower = 50.f;
		inline float hipheight = 0.f;
		inline bool apply_continuous = true;

		inline bool fly_enabled = false;
		inline int fly_key = VK_SPACE;
		inline float fly_speed = 50.f;
	}

	namespace config
	{
		inline char name[64] = "default";
		inline std::string last_status;
	}
}
