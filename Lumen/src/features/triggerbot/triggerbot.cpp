#include "triggerbot.h"
#include <chrono>
#include <thread>
#include <windows.h>
#include <cache/cache.h>
#include <game/game.h>
#include <features/settings.h>
#include <features/aimbot/aimbot.h>
#include <wallcheck/wallcheck.h>

// R6 ("Torso") and R15 ("UpperTorso"/"LowerTorso") naming
static const char* trigger_parts[] = {
	"Head", "Torso", "UpperTorso", "LowerTorso", "HumanoidRootPart"
};

void triggerbot::run()
{
	using namespace std::chrono_literals;
	auto last_shot = std::chrono::steady_clock::now() - std::chrono::hours(1);

	for (;;)
	{
		if (!settings::triggerbot::enabled) { std::this_thread::sleep_for(10ms); continue; }

		bool key_down = !settings::triggerbot::require_key || (GetAsyncKeyState(settings::triggerbot::key) & 0x8000);
		if (!key_down) { std::this_thread::sleep_for(5ms); continue; }

		if (!game::visualengine || !game::visualengine->address) { std::this_thread::sleep_for(10ms); continue; }

		POINT cursor{};
		if (!GetCursorPos(&cursor)) { std::this_thread::sleep_for(5ms); continue; }
		math::vector2 center{ (float)cursor.x, (float)cursor.y };

		math::matrix4 view = game::visualengine->get_viewmatrix();
		math::vector2 dims = game::visualengine->get_dimensions();

		std::vector<cache::entity_t> snap;
		{ std::lock_guard<std::mutex> l(cache::mtx); snap = cache::players; }

		bool found = false;
		for (auto& e : snap) {
			if (!e.instance.address) continue;
			if (e.instance.address == cache::local_player.instance.address) continue;
			if (settings::triggerbot::teamcheck && e.team == cache::local_player.team) continue;
			if (settings::triggerbot::deadcheck && e.health <= 0) continue;

			for (auto* pn : trigger_parts) {
				auto it = e.parts.find(pn);
				if (it == e.parts.end() || !it->second.address) continue;
				math::vector3 w = it->second.get_primitive().get_position();
				if (settings::triggerbot::wallcheck) {
					if (!game::camera) continue; // camera not cached yet
					math::vector3 lp = memory->read<math::vector3>(game::camera + Offsets::Camera::Position);
					if (!wallcheck->is_visible(lp, w)) continue;
				}
				math::vector2 s{};
				if (!game::visualengine->world_to_screen(view, dims, w, s)) continue;
				float dx = s.x - center.x, dy = s.y - center.y;
				if (std::sqrt(dx*dx + dy*dy) <= settings::triggerbot::fov_radius) { found = true; break; }
			}
			if (found) break;
		}

		auto now = std::chrono::steady_clock::now();
		auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shot).count();
		if (found && since >= settings::triggerbot::delay_ms) {
			INPUT down{};
			down.type = INPUT_MOUSE;
			down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			SendInput(1, &down, sizeof(down));
			std::this_thread::sleep_for(20ms); // hold so the click registers in-game
			INPUT up{};
			up.type = INPUT_MOUSE;
			up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
			SendInput(1, &up, sizeof(up));
			last_shot = std::chrono::steady_clock::now();
		}

		std::this_thread::sleep_for(5ms);
	}
}
