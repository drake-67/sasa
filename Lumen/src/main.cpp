#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <thread>
#include <chrono>

#include <globals.h>
#include <sdk/sdk.h>
#include <game/game.h>
#include <cache/cache.h>
#include <logger/logger.h>
#include <render/render.h>
#include <scanner/rescan.h>
#include <wallcheck/wallcheck.h>
#include <features/aimbot/aimbot.h>
#include <features/triggerbot/triggerbot.h>
#include <features/legit/legit.h>
#include <features/config/config.h>
#include <sdk/offsets/auto_update.h>

std::int32_t main()
{
	using namespace std::chrono_literals;

	SetConsoleTitleA(("SASA | " + Offsets::ClientVersion).c_str());
	ShowWindow(GetConsoleWindow(), SW_SHOW);

	logger->log<INFO>("SASA external starting...");

	offsets_auto::ensure_latest();
	logger->log<INFO>("offsets status: {}", offsets_auto::status());
	SetConsoleTitleA(("SASA | " + Offsets::ClientVersion).c_str());

	if (memory->find_process_id(BINARY_NAME) == 0)
	{
		logger->log<ERR>("roblox process not found");
		std::this_thread::sleep_for(5s);
		std::exit(0);
	}

	logger->log<INFO>("found {} process, id {}", BINARY_NAME, memory->m_process_id);

	if (!memory->attach_to_process(BINARY_NAME))
	{
		logger->log<ERR>("unable to attach to the process");
		std::this_thread::sleep_for(5s);
		std::exit(0);
	}

	logger->log<INFO>("attached to {}, handle id {}", BINARY_NAME, memory->m_process_handle);

	if (memory->find_module_address(BINARY_NAME) == 0)
	{
		logger->log<ERR>("unable to find module address");
		std::this_thread::sleep_for(5s);
		std::exit(0);
	}

	logger->log<INFO>("found {} module @ {:x}", BINARY_NAME, memory->m_base_address);

	game::datamodel = { rbx::c_datamodel::get() };
	game::visualengine = { rbx::c_visualengine::get() };

	wallcheck->cache_workspace();

	if (game::datamodel->address == 0)
	{
		logger->log<WARN>("datamodel address is 0, will attempt rescans soon");
	}
	else
	{
		logger->log<INFO>("found datamodel @ {:x}", game::datamodel->address);
	}

	if (game::visualengine->address == 0)
	{
		logger->log<WARN>("visualengine address is 0, will attempt rescans soon");
	}
	else
	{
		logger->log<INFO>("found visualengine @ {:x}", game::visualengine->address);
	}

	// auto-load default config if present (non-fatal)
	config::load("default");

	logger->log<INFO>("all initial setup complete, hiding console...");

	std::this_thread::sleep_for(1s);
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	std::thread(rescan::rescan_game).detach();
	std::thread(rescan::rescan_process).detach();

	std::thread(cache::run).detach();
	std::thread(aimbot::run).detach();
	std::thread(triggerbot::run).detach();
	std::thread(legit::run).detach();

	{ /* setup overlay */
		if (!render->create_window())
		{
			ShowWindow(GetConsoleWindow(), SW_SHOW);
			logger->log<ERR>("failed to create window handle");
			std::this_thread::sleep_for(5s);
			std::exit(0);
		}

		if (!render->create_device())
		{
			ShowWindow(GetConsoleWindow(), SW_SHOW);
			logger->log<ERR>("failed to create directx device");
			std::this_thread::sleep_for(5s);
			std::exit(0);
		}
		if (!render->create_imgui())
		{
			ShowWindow(GetConsoleWindow(), SW_SHOW);
			logger->log<ERR>("failed to create gui context");
			std::this_thread::sleep_for(5s);
			std::exit(0);
		}
	}

	render->sync_interaction(); // menu starts open: capture mouse from frame one

	while (true)
	{
		render->start_render();

		render->render_visuals();

		if (render->running)
		{
			render->render_menu();
		}

		render->end_render();
	}
}
