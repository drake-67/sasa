#include "auto_update.h"
#include "offsets.h"

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cctype>
#include <json/json.hpp>

#include <logger/logger.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
	std::string g_status = "not run";
	std::string g_live_version;

	std::string trim(const std::string& s)
	{
		size_t a = 0;
		while (a < s.size() && std::isspace((unsigned char)s[a])) a++;
		size_t b = s.size();
		while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
		return s.substr(a, b - a);
	}

	std::string http_get(const wchar_t* host, const wchar_t* path)
	{
		HINTERNET hSession = WinHttpOpen(L"SASA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession) throw std::runtime_error("WinHttpOpen failed");

		HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!hConnect) { WinHttpCloseHandle(hSession); throw std::runtime_error("WinHttpConnect failed"); }

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!hRequest) {
			WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
			throw std::runtime_error("WinHttpOpenRequest failed");
		}

		// short timeouts so a dead service never hangs startup
		WinHttpSetTimeouts(hRequest, 4000, 4000, 5000, 5000);

		BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		if (!ok || !WinHttpReceiveResponse(hRequest, nullptr)) {
			WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
			throw std::runtime_error("request failed");
		}

		DWORD status = 0;
		DWORD statusSize = sizeof(status);
		WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (status != 200) {
			WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
			throw std::runtime_error("HTTP " + std::to_string(status));
		}

		std::string body;
		DWORD available = 0;
		while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
			std::vector<char> buf(available);
			DWORD read = 0;
			if (!WinHttpReadData(hRequest, buf.data(), available, &read)) break;
			if (read == 0) break;
			body.append(buf.data(), read);
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return body;
	}

	uintptr_t get_off(const json& data, const char* cls, const char* field)
	{
		auto it = data.find("Offsets");
		if (it == data.end() || !it->is_object()) return 0;
		auto c = it->find(cls);
		if (c == it->end() || !c->is_object()) return 0;
		auto f = c->find(field);
		if (f == c->end()) return 0;
		try { return f->get<uintptr_t>(); }
		catch (...) { return 0; }
	}

#define APPLY(cls, field, target) \
	do { uintptr_t v = get_off(data, #cls, #field); if (v) Offsets::cls::field = v; } while (0)

	void apply_json(const json& data)
	{
		// version string
		try {
			if (data.contains("Roblox Version") && data["Roblox Version"].is_string())
				Offsets::ClientVersion = data["Roblox Version"].get<std::string>();
			else if (!g_live_version.empty())
				Offsets::ClientVersion = g_live_version;
		}
		catch (...) {}

		// everything SASA actually reads. missing keys keep compiled fallback.
		APPLY(FakeDataModel, Pointer, x);
		APPLY(FakeDataModel, RealDataModel, x);
		APPLY(VisualEngine, Pointer, x);
		APPLY(VisualEngine, ViewMatrix, x);
		APPLY(VisualEngine, Dimensions, x);
		APPLY(VisualEngine, RenderView, x);

		APPLY(Instance, ChildrenStart, x);
		APPLY(Instance, ChildrenEnd, x);
		APPLY(Instance, ClassDescriptor, x);
		APPLY(Instance, Name, x);
		APPLY(Instance, NameContainer, x);
		APPLY(Instance, Parent, x);
		APPLY(ClassDescriptor, ClassName, x);

		APPLY(Player, LocalPlayer, x);
		APPLY(Player, ModelInstance, x);
		APPLY(Player, Team, x);
		APPLY(Player, DisplayName, x);
		APPLY(Player, UserId, x);

		APPLY(Humanoid, Health, x);
		APPLY(Humanoid, MaxHealth, x);
		APPLY(Humanoid, Walkspeed, x);
		APPLY(Humanoid, WalkspeedCheck, x);
		APPLY(Humanoid, JumpPower, x);
		APPLY(Humanoid, HipHeight, x);
		APPLY(Humanoid, RigType, x);
		APPLY(Humanoid, HumanoidState, x);
		APPLY(Humanoid, HumanoidStateID, x);

		APPLY(BasePart, Primitive, x);
		APPLY(Primitive, Position, x);
		APPLY(Primitive, Rotation, x);
		APPLY(Primitive, Size, x);

		APPLY(Camera, Position, x);
		APPLY(Camera, Rotation, x);
		APPLY(Camera, FieldOfView, x);
		APPLY(Camera, ViewportSize, x);

		APPLY(DataModel, Workspace, x);
		APPLY(DataModel, GameId, x);
		APPLY(DataModel, PlaceId, x);
		APPLY(DataModel, CreatorId, x);
		APPLY(DataModel, ServerIP, x);
		APPLY(Workspace, CurrentCamera, x);

		// wallcheck extras (best-effort, names vary by dumper)
		APPLY(TaskScheduler, Pointer, x);
		APPLY(EngineWallCheck, StepWorldRva, x);
		APPLY(EngineWallCheck, RealWrapperRva, x);
		APPLY(WorldRoot, RaycastBoundFnRva, x);
		APPLY(WorldRoot, RaycastDescriptorRva, x);
	}
}

const std::string& offsets_auto::status() { return g_status; }
const std::string& offsets_auto::live_version() { return g_live_version; }

bool offsets_auto::ensure_latest()
{
	const fs::path versionFile = "SASA_offsets.version";
	const fs::path offsetsFile = "SASA_offsets.json";

	try {
		std::string live;
		try {
			live = trim(http_get(L"offsets.imtheo.lol", L"/roblox/version"));
		}
		catch (const std::exception& e) {
			// offline: try cache, else keep compiled
			std::ifstream f(offsetsFile);
			if (f) {
				try {
					json cached = json::parse(f);
					apply_json(cached);
					g_status = std::string("offline, used cache (") + e.what() + ")";
					return true;
				}
				catch (...) {}
			}
			g_status = std::string("offline, compiled fallback (") + e.what() + ")";
			return false;
		}

		g_live_version = live;

		std::string cachedVersion;
		{ std::ifstream f(versionFile); if (f) std::getline(f, cachedVersion); cachedVersion = trim(cachedVersion); }

		json data;
		bool need_download = (live != cachedVersion) || !fs::exists(offsetsFile);
		if (need_download) {
			std::string body = http_get(L"offsets.imtheo.lol", L"/offsets.json");
			// validate before overwriting
			data = json::parse(body);
			if (!data.contains("Offsets")) throw std::runtime_error("bad offsets.json shape");
			{ std::ofstream f(offsetsFile, std::ios::trunc | std::ios::binary); f << body; }
			{ std::ofstream f(versionFile, std::ios::trunc); f << live; }
			g_status = "downloaded fresh for " + live;
		}
		else {
			std::ifstream f(offsetsFile);
			if (!f) throw std::runtime_error("cache missing");
			data = json::parse(f);
			g_status = "up to date (" + live + ")";
		}

		apply_json(data);
		logger->log<INFO>("offsets: {}", g_status);
		return true;
	}
	catch (const std::exception& e) {
		g_status = std::string("failed, compiled fallback (") + e.what() + ")";
		logger->log<WARN>("offsets auto-update failed: {}, using compiled", e.what());
		return false;
	}
}
