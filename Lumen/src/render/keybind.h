#pragma once
#include <windows.h>
#include <string>
#include <chrono>
#include <imgui/imgui.h>

// Robust click-to-bind widget.
// Uses down-state edge detection (not the flaky GetAsyncKeyState transition
// bit), a grace period so the opening click never binds itself, and ESC to
// cancel. Left mouse is never bindable (it drives the UI itself).
// Returns true while this widget is waiting for a key.
namespace keybind_detail
{
	struct state_t
	{
		ImGuiID waiting = 0;
		std::chrono::steady_clock::time_point since{};
		bool prev_down[256] = {};
		bool primed = false;
	};

	inline state_t& state()
	{
		static state_t s;
		return s;
	}

	inline void key_name(int vk, char* out, size_t out_len)
	{
		if (vk == 0) { std::snprintf(out, out_len, "None"); return; }
		if (vk == VK_XBUTTON1) { std::snprintf(out, out_len, "Mouse4"); return; }
		if (vk == VK_XBUTTON2) { std::snprintf(out, out_len, "Mouse5"); return; }
		if (vk == VK_MBUTTON) { std::snprintf(out, out_len, "Mouse3"); return; }
		UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
		char name[32]{};
		if (sc && GetKeyNameTextA((sc << 16), name, sizeof(name)))
			std::snprintf(out, out_len, "%s", name);
		else
			std::snprintf(out, out_len, "0x%02X", vk & 0xFF);
	}
}

inline bool key_bind(const char* label, int* key)
{
	using namespace keybind_detail;
	state_t& st = state();

	ImGui::PushID(label);
	const ImGuiID id = ImGui::GetID("##bind");

	ImGui::TextUnformatted(label);
	ImGui::SameLine(150.f);

	char buf[48]{};
	const bool waiting = (st.waiting == id && st.waiting != 0);

	if (waiting)
	{
		// blink while listening
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		std::snprintf(buf, sizeof(buf), (ms / 400) % 2 ? "[ ... ]" : "[ press ]");

		auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - st.since).count();

		bool down_now[256] = {};
		for (int vk = 1; vk <= 254; vk++)
			down_now[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;

		if (st.primed && waited > 250)
		{
			// ESC cancels
			if (down_now[VK_ESCAPE] && !st.prev_down[VK_ESCAPE])
			{
				st.waiting = 0;
			}
			else
			{
				for (int vk = 1; vk <= 254; vk++)
				{
					if (vk == VK_LBUTTON) continue; // UI click, never binds
					if (vk == VK_ESCAPE) continue;
					if (down_now[vk] && !st.prev_down[vk])
					{
						*key = vk;
						st.waiting = 0;
						break;
					}
				}
			}
		}

		for (int vk = 1; vk <= 254; vk++)
			st.prev_down[vk] = down_now[vk];
		st.primed = true;
	}
	else
	{
		key_name(*key, buf, sizeof(buf));
	}

	if (ImGui::Button(buf, ImVec2(120, 0)))
	{
		st.waiting = id;
		st.since = std::chrono::steady_clock::now();
		st.primed = false;
		for (int vk = 1; vk <= 254; vk++)
			st.prev_down[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("X")) { *key = 0; if (st.waiting == id) st.waiting = 0; }

	ImGui::PopID();
	return waiting;
}

// One-shot edge detector for hotkeys polled once per frame.
// Returns true only on the press edge (no auto-repeat while held).
inline bool hotkey_pressed(int vk, bool& prev_down)
{
	if (vk == 0) { prev_down = false; return false; }
	const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
	const bool edge = down && !prev_down;
	prev_down = down;
	return edge;
}
