#pragma once
#include <windows.h>
#include <string>
#include <chrono>
#include <imgui/imgui.h>

// click-to-bind key widget. returns true while waiting.
// 300ms grace after clicking so the opening left-click never binds itself.
inline bool key_bind(const char* label, int* key)
{
	static int* waiting_for = nullptr;
	static auto wait_start = std::chrono::steady_clock::now();

	ImGui::PushID(label);
	ImGui::TextUnformatted(label);
	ImGui::SameLine(140.f);

	char buf[48]{};
	if (waiting_for == key) {
		std::snprintf(buf, sizeof(buf), "[...]");
		auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - wait_start).count();

		if (waited > 300) {
			if (GetAsyncKeyState(VK_ESCAPE) & 1) { waiting_for = nullptr; }
			else {
				for (int vk = 0x01; vk <= 0xFE; vk++) {
					if (vk == VK_LBUTTON) continue; // left-click confirms UI, never binds
					if (GetAsyncKeyState(vk) & 1) { *key = vk; waiting_for = nullptr; break; }
				}
			}
		}
		if (waiting_for == nullptr) { /* bound or cancelled */ }
	}
	else {
		if (*key >= VK_XBUTTON1 && *key <= VK_XBUTTON2)
			std::snprintf(buf, sizeof(buf), "%s", *key == VK_XBUTTON1 ? "Mouse4" : "Mouse5");
		else if (*key == 0)
			std::snprintf(buf, sizeof(buf), "None");
		else {
			UINT sc = MapVirtualKeyA((UINT)*key, MAPVK_VK_TO_VSC);
			char name[32]{};
			if (sc && GetKeyNameTextA((sc << 16), name, sizeof(name)))
				std::snprintf(buf, sizeof(buf), "%s", name);
			else
				std::snprintf(buf, sizeof(buf), "0x%02X", *key & 0xFF);
		}
	}

	if (ImGui::Button(buf, ImVec2(110, 0))) {
		waiting_for = key;
		wait_start = std::chrono::steady_clock::now();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("X")) { *key = 0; if (waiting_for == key) waiting_for = nullptr; }
	ImGui::PopID();
	return waiting_for == key;
}
