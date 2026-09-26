#include "legit.h"
#include <chrono>
#include <thread>
#include <windows.h>
#include <cache/cache.h>
#include <game/game.h>
#include <features/settings.h>
#include <sdk/sdk.h>

static bool apply_to_local(const cache::entity_t& local)
{
	if (!local.instance.address) return false;
	if (!local.humanoid.address) return false;

	local.humanoid.set_walk_speed(settings::legit::walkspeed);
	local.humanoid.set_jump_power(settings::legit::jumppower);
	local.humanoid.set_hip_height(settings::legit::hipheight);
	return true;
}

void legit::apply_once()
{
	std::vector<cache::entity_t> snap;
	{ std::lock_guard<std::mutex> l(cache::mtx); snap = cache::players; }
	cache::entity_t local = cache::local_player;
	// local_player copy may be stale; find fresh by address
	for (auto& e : snap) {
		if (e.instance.address == local.instance.address) { local = e; break; }
	}
	apply_to_local(local);
}

void legit::run()
{
	using namespace std::chrono_literals;
	for (;;) {
		if (settings::legit::enabled && settings::legit::apply_continuous) {
			apply_once();
		}

		// simple fly: hold fly key, move HRP along camera facing with WASD + space/ctrl
		if (settings::legit::fly_enabled && (GetAsyncKeyState(settings::legit::fly_key) & 0x8000)) {
			auto it = cache::local_player.parts.find("HumanoidRootPart");
			if (it != cache::local_player.parts.end() && it->second.address) {
				rbx::c_primitive prim = it->second.get_primitive();
				math::vector3 pos = prim.get_position();
				// camera basis from game camera rotation
				math::matrix3 cam_rot = memory->read<math::matrix3>(game::camera + Offsets::Camera::Rotation);
				math::vector3 fwd = math::vector3(-cam_rot.m[0][2], -cam_rot.m[1][2], -cam_rot.m[2][2]).normalized();
				math::vector3 right = math::vector3(-cam_rot.m[0][0], -cam_rot.m[1][0], -cam_rot.m[2][0]).normalized();
				math::vector3 move{ 0,0,0 };
				if (GetAsyncKeyState('W') & 0x8000) move = move + fwd;
				if (GetAsyncKeyState('S') & 0x8000) move = move - fwd;
				if (GetAsyncKeyState('D') & 0x8000) move = move + right;
				if (GetAsyncKeyState('A') & 0x8000) move = move - right;
				if (GetAsyncKeyState(VK_SPACE) & 0x8000) move.y += 1.f;
				if (GetAsyncKeyState(VK_CONTROL) & 0x8000) move.y -= 1.f;
				if (move.length() > 0.01f) {
					move = move.normalized() * (settings::legit::fly_speed * 0.016f);
					prim.set_position(pos + move);
				}
			}
		}

		std::this_thread::sleep_for(16ms);
	}
}
