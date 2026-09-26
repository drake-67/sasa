#include "config.h"
#include <fstream>
#include <filesystem>
#include <json/json.hpp>

using nlohmann::json;

#define S(v) j[#v] = v
#define L(v) if (j.contains(#v)) j[#v].get_to(v)

static void arr_to_j(json& j, const char* k, float v[4]) { j[k] = { v[0], v[1], v[2], v[3] }; }
static void j_to_arr(const json& j, const char* k, float v[4]) {
	if (!j.contains(k) || !j[k].is_array() || j[k].size() < 4) return;
	for (int i = 0; i < 4; i++) j[k][i].get_to(v[i]);
}

std::string config::file_path(const char* name)
{
	std::string safe{ name };
	if (safe.empty()) safe = "default";
	for (char& c : safe) {
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			c = '_';
	}
	return "SASA_" + safe + ".json";
}

bool config::save(const char* name)
{
	try {
		json j;
		// global
		arr_to_j(j, "menu_accent", settings::menu_accent);
		S(settings::menu_key); S(settings::show_fps); S(settings::show_entity_count); S(settings::overlay_fps_limit);

		// aimbot
		S(settings::aimbot::enabled); S(settings::aimbot::aim_key); S(settings::aimbot::require_key);
		S(settings::aimbot::target_part); S(settings::aimbot::priority);
		S(settings::aimbot::sticky_target); S(settings::aimbot::max_stick_distance); S(settings::aimbot::fov);
		S(settings::aimbot::draw_fov); arr_to_j(j, "aimbot_fov_colour", settings::aimbot::fov_colour);
		S(settings::aimbot::fov_filled); S(settings::aimbot::fov_fill_alpha);
		S(settings::aimbot::smooth_x); S(settings::aimbot::smooth_y); S(settings::aimbot::smooth_curve);
		S(settings::aimbot::humanize); S(settings::aimbot::deadzone); S(settings::aimbot::headshot_chance);
		S(settings::aimbot::prediction); S(settings::aimbot::prediction_amount); S(settings::aimbot::visibility_only);
		S(settings::aimbot::teamcheck); S(settings::aimbot::deadcheck); S(settings::aimbot::wallcheck);
		S(settings::aimbot::camera::enabled); S(settings::aimbot::camera::smoothing_enabled); S(settings::aimbot::camera::smoothing_value);

		// triggerbot
		S(settings::triggerbot::enabled); S(settings::triggerbot::key); S(settings::triggerbot::require_key);
		S(settings::triggerbot::delay_ms); S(settings::triggerbot::fov_radius);
		S(settings::triggerbot::teamcheck); S(settings::triggerbot::deadcheck); S(settings::triggerbot::wallcheck);

		// visuals
		S(settings::visuals::streamproof);
		S(settings::visuals::box); S(settings::visuals::box_style); arr_to_j(j, "visuals_colour", settings::visuals::colour);
		S(settings::visuals::box_fill); S(settings::visuals::box_fill_alpha); S(settings::visuals::team_colour_override);
		S(settings::visuals::username); arr_to_j(j, "visuals_username_colour", settings::visuals::username_colour);
		S(settings::visuals::distance); arr_to_j(j, "visuals_distance_colour", settings::visuals::distance_colour);
		S(settings::visuals::healthbar); arr_to_j(j, "visuals_healthbar_colour", settings::visuals::healthbar_colour);
		S(settings::visuals::health_text);
		S(settings::visuals::skeleton); arr_to_j(j, "visuals_skeleton_colour", settings::visuals::skeleton_colour);
		S(settings::visuals::snapline); S(settings::visuals::snapline_origin); arr_to_j(j, "visuals_snapline_colour", settings::visuals::snapline_colour);
		S(settings::visuals::head_dot); arr_to_j(j, "visuals_head_dot_colour", settings::visuals::head_dot_colour); S(settings::visuals::head_dot_size);
		S(settings::visuals::offscreen_arrows); arr_to_j(j, "visuals_arrow_colour", settings::visuals::arrow_colour);
		S(settings::visuals::arrow_size); S(settings::visuals::arrow_distance);
		S(settings::visuals::crosshair); arr_to_j(j, "visuals_crosshair_colour", settings::visuals::crosshair_colour);
		S(settings::visuals::crosshair_size); S(settings::visuals::crosshair_gap);
		S(settings::visuals::hitmarker); arr_to_j(j, "visuals_hitmarker_colour", settings::visuals::hitmarker_colour);
		S(settings::visuals::hitmarker_size); S(settings::visuals::hitmarker_time);
		S(settings::visuals::radar); S(settings::visuals::radar_size); S(settings::visuals::radar_range); S(settings::visuals::radar_zoom);
		arr_to_j(j, "visuals_radar_bg", settings::visuals::radar_bg);
		S(settings::visuals::teamcheck); S(settings::visuals::deadcheck); S(settings::visuals::wallcheck);
		S(settings::visuals::debug_wallcheck); S(settings::visuals::debug_wallcheck_max_length);

		// legit
		S(settings::legit::enabled); S(settings::legit::walkspeed); S(settings::legit::jumppower);
		S(settings::legit::hipheight); S(settings::legit::apply_continuous);
		S(settings::legit::fly_enabled); S(settings::legit::fly_key); S(settings::legit::fly_speed);

		std::ofstream f(file_path(name));
		if (!f) { settings::config::last_status = "failed to open file for write"; return false; }
		f << j.dump(2);
		settings::config::last_status = "saved " + file_path(name);
		return true;
	}
	catch (...) { settings::config::last_status = "save exception"; return false; }
}

bool config::load(const char* name)
{
	try {
		std::ifstream f(file_path(name));
		if (!f) { settings::config::last_status = "file not found"; return false; }
		json j; f >> j;

		j_to_arr(j, "menu_accent", settings::menu_accent);
		L(settings::menu_key); L(settings::show_fps); L(settings::show_entity_count); L(settings::overlay_fps_limit);

		L(settings::aimbot::enabled); L(settings::aimbot::aim_key); L(settings::aimbot::require_key);
		L(settings::aimbot::target_part); L(settings::aimbot::priority);
		L(settings::aimbot::sticky_target); L(settings::aimbot::max_stick_distance); L(settings::aimbot::fov);
		L(settings::aimbot::draw_fov); j_to_arr(j, "aimbot_fov_colour", settings::aimbot::fov_colour);
		L(settings::aimbot::fov_filled); L(settings::aimbot::fov_fill_alpha);
		L(settings::aimbot::smooth_x); L(settings::aimbot::smooth_y); L(settings::aimbot::smooth_curve);
		L(settings::aimbot::humanize); L(settings::aimbot::deadzone); L(settings::aimbot::headshot_chance);
		L(settings::aimbot::prediction); L(settings::aimbot::prediction_amount); L(settings::aimbot::visibility_only);
		L(settings::aimbot::teamcheck); L(settings::aimbot::deadcheck); L(settings::aimbot::wallcheck);
		L(settings::aimbot::camera::enabled); L(settings::aimbot::camera::smoothing_enabled); L(settings::aimbot::camera::smoothing_value);

		L(settings::triggerbot::enabled); L(settings::triggerbot::key); L(settings::triggerbot::require_key);
		L(settings::triggerbot::delay_ms); L(settings::triggerbot::fov_radius);
		L(settings::triggerbot::teamcheck); L(settings::triggerbot::deadcheck); L(settings::triggerbot::wallcheck);

		L(settings::visuals::streamproof);
		L(settings::visuals::box); L(settings::visuals::box_style); j_to_arr(j, "visuals_colour", settings::visuals::colour);
		L(settings::visuals::box_fill); L(settings::visuals::box_fill_alpha); L(settings::visuals::team_colour_override);
		L(settings::visuals::username); j_to_arr(j, "visuals_username_colour", settings::visuals::username_colour);
		L(settings::visuals::distance); j_to_arr(j, "visuals_distance_colour", settings::visuals::distance_colour);
		L(settings::visuals::healthbar); j_to_arr(j, "visuals_healthbar_colour", settings::visuals::healthbar_colour);
		L(settings::visuals::health_text);
		L(settings::visuals::skeleton); j_to_arr(j, "visuals_skeleton_colour", settings::visuals::skeleton_colour);
		L(settings::visuals::snapline); L(settings::visuals::snapline_origin); j_to_arr(j, "visuals_snapline_colour", settings::visuals::snapline_colour);
		L(settings::visuals::head_dot); j_to_arr(j, "visuals_head_dot_colour", settings::visuals::head_dot_colour); L(settings::visuals::head_dot_size);
		L(settings::visuals::offscreen_arrows); j_to_arr(j, "visuals_arrow_colour", settings::visuals::arrow_colour);
		L(settings::visuals::arrow_size); L(settings::visuals::arrow_distance);
		L(settings::visuals::crosshair); j_to_arr(j, "visuals_crosshair_colour", settings::visuals::crosshair_colour);
		L(settings::visuals::crosshair_size); L(settings::visuals::crosshair_gap);
		L(settings::visuals::hitmarker); j_to_arr(j, "visuals_hitmarker_colour", settings::visuals::hitmarker_colour);
		L(settings::visuals::hitmarker_size); L(settings::visuals::hitmarker_time);
		L(settings::visuals::radar); L(settings::visuals::radar_size); L(settings::visuals::radar_range); L(settings::visuals::radar_zoom);
		j_to_arr(j, "visuals_radar_bg", settings::visuals::radar_bg);
		L(settings::visuals::teamcheck); L(settings::visuals::deadcheck); L(settings::visuals::wallcheck);
		L(settings::visuals::debug_wallcheck); L(settings::visuals::debug_wallcheck_max_length);

		L(settings::legit::enabled); L(settings::legit::walkspeed); L(settings::legit::jumppower);
		L(settings::legit::hipheight); L(settings::legit::apply_continuous);
		L(settings::legit::fly_enabled); L(settings::legit::fly_key); L(settings::legit::fly_speed);

		settings::aimbot::locked_target = 0;
		settings::config::last_status = "loaded " + file_path(name);
		return true;
	}
	catch (...) { settings::config::last_status = "load exception"; return false; }
}
