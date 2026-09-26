#pragma once
#include <windows.h>
#include <string>
#include <imgui/imgui.h>

// click-to-bind key widget. returns true while waiting.
inline bool key_bind(const char* label, int* key)
{
	static int* waiting_for = nullptr;
	ImGui::PushID(label);
	ImGui::TextUnformatted(label);
	ImGui::SameLine(140.f);

	char buf[48]{};
	if (waiting_for == key) {
		std::snprintf(buf, sizeof(buf), "[...]");
		// poll keys 0x01..0xFE (skip mouse move noise lightly)
		for (int vk = 0x01; vk <= 0xFE; vk++) {
			if (vk == VK_LBUTTON && ImGui::GetIO().WantCaptureMouse) {
				// allow unbind with right click instead; skip left while clicking widget
				continue;
			}
			if (GetAsyncKeyState(vk) & 1) { *key = vk; waiting_for = nullptr; break; }
		}
		if (GetAsyncKeyState(VK_ESCAPE) & 1) { waiting_for = nullptr; }
	}
	else {
		if (*key >= VK_XBUTTON1 && *key <= VK_XBUTTON2)
			std::snprintf(buf, sizeof(buf), "%s", *key == VK_XBUTTON1 ? "Mouse4" : "Mouse5");
		else {
			UINT sc = MapVirtualKeyA((UINT)*key, MAPVK_VK_TO_VSC);
			char name[32]{};
			if (sc && GetKeyNameTextA((sc << 16), name, sizeof(name)))
				std::snprintf(buf, sizeof(buf), "%s", name);
			else
				std::snprintf(buf, sizeof(buf), "0x%02X", *key & 0xFF);
		}
	}

	if (ImGui::Button(buf, ImVec2(110, 0)))
		waiting_for = key;
	ImGui::SameLine();
	if (ImGui::SmallButton("X")) { *key = 0; if (waiting_for == key) waiting_for = nullptr; }
	ImGui::PopID();
	return waiting_for == key;
}
