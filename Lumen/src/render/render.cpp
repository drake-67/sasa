#define IMGUI_DEFINE_MATH_OPERATORS
#include <render/render.h>

#include <dwmapi.h>
#include <cstdio>
#include <chrono>
#include <thread>
#include <mutex>

#include "assets/verdana.h"

#include <globals.h>
#include <cache/cache.h>
#include <features/esp/esp.h>
#include <features/settings.h>
#include <features/config/config.h>
#include <features/legit/legit.h>
#include "keybind.h"
#include "sdk/math/math.h"
#include <game/game.h>
#include <logger/logger.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
    {
        return true;
    }

    switch (msg)
    {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
        {
            return 0;
        }
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

render_t::render_t()
{
    detail = std::make_unique<detail_t>();
}

render_t::~render_t()
{
    destroy_imgui();
    destroy_window();
    destroy_device();
}

bool render_t::create_window()
{
    detail->window_class.cbSize = sizeof(detail->window_class);
    detail->window_class.style = CS_CLASSDC;
    detail->window_class.lpszClassName = "SASA_Overlay";
    detail->window_class.hInstance = GetModuleHandleA(0);
    detail->window_class.lpfnWndProc = wnd_proc;

    RegisterClassExA(&detail->window_class);

    detail->window = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        detail->window_class.lpszClassName,
        "SASA",
        WS_POPUP,
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        0,
        0,
        detail->window_class.hInstance,
        0
    );

    if (!detail->window)
    {
        return false;
    }

    SetLayeredWindowAttributes(detail->window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

    RECT client_area{};
    RECT window_area{};

    GetClientRect(detail->window, &client_area);
    GetWindowRect(detail->window, &window_area);

    POINT diff{};
    ClientToScreen(detail->window, &diff);

    MARGINS margins
    {
        window_area.left + (diff.x - window_area.left),
        window_area.top + (diff.y - window_area.top),
        window_area.right,
        window_area.bottom,
    };

    DwmExtendFrameIntoClientArea(detail->window, &margins);

    ShowWindow(detail->window, SW_SHOW);
    UpdateWindow(detail->window);

    return true;
}

bool render_t::create_device()
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};

    swap_chain_desc.BufferCount = 1;

    swap_chain_desc.BufferDesc.Width = 0;
    swap_chain_desc.BufferDesc.Height = 0;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    swap_chain_desc.OutputWindow = detail->window;

    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Windowed = 1;

    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    swap_chain_desc.SampleDesc.Count = 2;
    swap_chain_desc.SampleDesc.Quality = 0;

    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    D3D_FEATURE_LEVEL feature_level;
    D3D_FEATURE_LEVEL feature_level_list[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_level_list,
        2,
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &detail->swap_chain,
        &detail->device,
        &feature_level,
        &detail->device_context
    );

    if (result == DXGI_ERROR_UNSUPPORTED)
    {
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            feature_level_list,
            2,
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            &detail->swap_chain,
            &detail->device,
            &feature_level,
            &detail->device_context
        );
    }

    if (result != S_OK)
    {
        MessageBoxA(nullptr, "This software can not run on your computer.", "Critical Problem", MB_ICONERROR | MB_OK);
    }

    ID3D11Texture2D* back_buffer{ nullptr };
    detail->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));

    if (back_buffer)
    {
        detail->device->CreateRenderTargetView(back_buffer, nullptr, &detail->render_target_view);
        back_buffer->Release();

        return true;
    }

    return false;
}

bool render_t::create_imgui()
{
    using namespace ImGui;
    CreateContext();
    StyleColorsDark();

	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;

    float verdana_regular_size = 13.f;
    verdana_regular_size *= main_scale;

	const unsigned int freetype_flags = ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;
	io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
	io.Fonts->FontLoaderFlags = freetype_flags;

	ImFontConfig font_cfg;
	font_cfg.PixelSnapH = true;
	font_cfg.OversampleH = 2;
	font_cfg.OversampleV = 1;
	font_cfg.RasterizerMultiply = 1.05f;
	font_cfg.FontLoaderFlags = freetype_flags;

	ImFontConfig verdana_regular_cfg = font_cfg;
	verdana_regular_cfg.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF((void*)font_verdana_regular, sizeof(font_verdana_regular), verdana_regular_size, &verdana_regular_cfg);

    if (!ImGui_ImplWin32_Init(detail->window))
    {
        return false;
    }

    if (!detail->device || !detail->device_context)
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(detail->device, detail->device_context))
    {
        return false;
    }

    return true;
}

void render_t::destroy_device()
{
	if (detail->render_target_view) detail->render_target_view->Release();
	if (detail->swap_chain) detail->swap_chain->Release();
	if (detail->device_context) detail->device_context->Release();
	if (detail->device) detail->device->Release();
}

void render_t::destroy_window()
{
    DestroyWindow(detail->window);
    UnregisterClassA(detail->window_class.lpszClassName, detail->window_class.hInstance);
}

void render_t::destroy_imgui()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void render_t::start_render()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

	if (settings::visuals::streamproof)
	{
		SetWindowDisplayAffinity(detail->window, WDA_EXCLUDEFROMCAPTURE);
	}
	else
	{
		SetWindowDisplayAffinity(detail->window, WDA_NONE);
	}

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // SASA accent
    ImGuiStyle& st = ImGui::GetStyle();
    ImVec4 acc{ settings::menu_accent[0], settings::menu_accent[1], settings::menu_accent[2], 1.f };
    st.Colors[ImGuiCol_Button] = ImVec4(acc.x * 0.6f, acc.y * 0.6f, acc.z * 0.6f, 1.f);
    st.Colors[ImGuiCol_ButtonHovered] = acc;
    st.Colors[ImGuiCol_ButtonActive] = acc;
    st.Colors[ImGuiCol_CheckMark] = acc;
    st.Colors[ImGuiCol_SliderGrab] = acc;
    st.Colors[ImGuiCol_SliderGrabActive] = acc;
    st.Colors[ImGuiCol_Header] = ImVec4(acc.x * 0.5f, acc.y * 0.5f, acc.z * 0.5f, 1.f);
    st.Colors[ImGuiCol_HeaderHovered] = acc;
    st.Colors[ImGuiCol_HeaderActive] = acc;

    if ((settings::menu_key != 0) && (GetAsyncKeyState(settings::menu_key) & 1))
    {
        running = !running;

        if (running)
        {
            // menu open: capture mouse (no TRANSPARENT) so widgets are clickable
            SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED);
        }
        else
        {
            // menu closed: click-through overlay
            SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
        }
        SetWindowPos(detail->window, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    // fps limiter
    static auto last_frame = std::chrono::steady_clock::now();
    if (settings::overlay_fps_limit > 0.f) {
        auto target = std::chrono::microseconds((int)(1000000.f / settings::overlay_fps_limit));
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_frame;
        if (elapsed < target)
            std::this_thread::sleep_for(target - elapsed);
    }

    // watermark + fps
    {
        auto now = std::chrono::steady_clock::now();
        static auto fps_last = now;
        static int fps_frames = 0;
        static int fps_value = 0;
        fps_frames++;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_last).count() >= 500) {
            fps_value = (int)(fps_frames * 1000 / max(1, (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_last).count()));
            fps_frames = 0;
            fps_last = now;
        }
        last_frame = std::chrono::steady_clock::now();
        detail_fps = fps_value;
    }

    if (settings::show_fps || settings::show_entity_count) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        char buf[128]{};
        size_t ent = 0;
        { std::lock_guard<std::mutex> l(cache::mtx); ent = cache::players.size(); }
        if (settings::show_fps && settings::show_entity_count)
            std::snprintf(buf, sizeof(buf), "SASA | %d fps | %llu players", detail_fps, (unsigned long long)ent);
        else if (settings::show_fps)
            std::snprintf(buf, sizeof(buf), "SASA | %d fps", detail_fps);
        else
            std::snprintf(buf, sizeof(buf), "SASA | %llu players", (unsigned long long)ent);
        fg->AddText({ 12, 10 }, IM_COL32(255,255,255,220), buf);
    }
}

void render_t::end_render()
{
    ImGui::Render();

    float clear_color[4]{ 0, 0, 0, 0 };
    detail->device_context->OMSetRenderTargets(1, &detail->render_target_view, nullptr);
    detail->device_context->ClearRenderTargetView(detail->render_target_view, clear_color);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    detail->swap_chain->Present(1, 0); // vsync: caps overlay to refresh rate, saves CPU/GPU
}

void render_t::render_menu()
{
    static std::int32_t tab = 0;

    ImGui::SetNextWindowSize({ 520, 600 }, ImGuiCond_Once);
    ImGui::Begin("SASA");

    const char* tabs[] = { "Aim", "Visual", "Trigger", "Legit", "Config" };
    for (int i = 0; i < 5; i++) {
        if (i > 0) ImGui::SameLine();
        if (ImGui::Button(tabs[i], ImVec2(90, 0)))
            tab = i;
    }

    ImGui::Separator();

    switch (tab)
    {
    case 0:
    {
        ImGui::Text("Aimbot");
        ImGui::Checkbox("Enable aimbot", &settings::aimbot::enabled);
        ImGui::Checkbox("Hold-to-aim (else always-on)", &settings::aimbot::require_key);
        key_bind("Aim key", &settings::aimbot::aim_key);

        static constexpr const char* const parts = "Closest\0Head\0Torso\0HumanoidRootPart\0";
        ImGui::Combo("Target part", &settings::aimbot::target_part, parts);
        static constexpr const char* const prio = "Crosshair\0Lowest HP\0Closest world\0";
        ImGui::Combo("Priority", &settings::aimbot::priority, prio);

        ImGui::Checkbox("Sticky target", &settings::aimbot::sticky_target);
        if (settings::aimbot::sticky_target)
            ImGui::SliderFloat("Stick radius px", &settings::aimbot::max_stick_distance, 20.f, 500.f, "%.0f");

        ImGui::Separator();
        ImGui::Text("FOV");
        ImGui::SliderFloat("FOV size", &settings::aimbot::fov, 10.f, 1000.f, "%.0f");
        ImGui::Checkbox("Draw FOV", &settings::aimbot::draw_fov);
        ImGui::ColorEdit4("FOV colour", settings::aimbot::fov_colour, ImGuiColorEditFlags_NoInputs);
        ImGui::Checkbox("FOV filled", &settings::aimbot::fov_filled);
        if (settings::aimbot::fov_filled)
            ImGui::SliderFloat("Fill alpha", &settings::aimbot::fov_fill_alpha, 0.f, 0.5f, "%.2f");

        ImGui::Separator();
        ImGui::Text("Smoothing");
        ImGui::SliderFloat("Smooth x", &settings::aimbot::smooth_x, 1.f, 100.f, "%.1f");
        ImGui::SliderFloat("Smooth y", &settings::aimbot::smooth_y, 1.f, 100.f, "%.1f");
        static constexpr const char* const curves = "Linear\0Ease-out\0Ease-in-out\0Humanized\0";
        ImGui::Combo("Curve", &settings::aimbot::smooth_curve, curves);
        if (settings::aimbot::smooth_curve == 3)
            ImGui::SliderFloat("Humanize", &settings::aimbot::humanize, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Deadzone px", &settings::aimbot::deadzone, 0.f, 20.f, "%.1f");
        ImGui::SliderFloat("Headshot %", &settings::aimbot::headshot_chance, 0.f, 100.f, "%.0f%%");

        ImGui::Separator();
        ImGui::Checkbox("Prediction", &settings::aimbot::prediction);
        if (settings::aimbot::prediction)
            ImGui::SliderFloat("Lead", &settings::aimbot::prediction_amount, 0.f, 0.5f, "%.3f");
        ImGui::Checkbox("Visible only", &settings::aimbot::visibility_only);

        ImGui::Separator();
        ImGui::Text("Camera aimbot");
        ImGui::Checkbox("Enable camera aimbot", &settings::aimbot::camera::enabled);
        ImGui::Checkbox("Camera smoothing", &settings::aimbot::camera::smoothing_enabled);
        if (settings::aimbot::camera::smoothing_enabled)
            ImGui::SliderFloat("Camera smooth", &settings::aimbot::camera::smoothing_value, 0.1f, 50.f, "%.1f");

        ImGui::Separator();
        ImGui::Text("Checks");
        ImGui::Checkbox("Team check##aim", &settings::aimbot::teamcheck);
        ImGui::Checkbox("Dead check##aim", &settings::aimbot::deadcheck);
        ImGui::Checkbox("Wall check##aim", &settings::aimbot::wallcheck);
        break;
    }
    case 1:
    {
        ImGui::Text("ESP");
        ImGui::Checkbox("Draw box", &settings::visuals::box);
        if (settings::visuals::box) {
            static constexpr const char* const styles = "Full\0Corner\0";
            ImGui::Combo("Box style", &settings::visuals::box_style, styles);
            ImGui::Checkbox("Box fill", &settings::visuals::box_fill);
            if (settings::visuals::box_fill)
                ImGui::SliderFloat("Fill alpha", &settings::visuals::box_fill_alpha, 0.f, 0.6f, "%.2f");
        }
        ImGui::ColorEdit4("Box colour", settings::visuals::colour, ImGuiColorEditFlags_NoInputs);
        ImGui::Checkbox("Team colour override", &settings::visuals::team_colour_override);

        ImGui::Checkbox("Username", &settings::visuals::username);
        ImGui::ColorEdit4("Username colour", settings::visuals::username_colour, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Distance", &settings::visuals::distance);
        ImGui::ColorEdit4("Distance colour", settings::visuals::distance_colour, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Healthbar", &settings::visuals::healthbar);
        ImGui::ColorEdit4("Healthbar colour", settings::visuals::healthbar_colour, ImGuiColorEditFlags_NoInputs);
        ImGui::Checkbox("Health text", &settings::visuals::health_text);

        ImGui::Separator();
        ImGui::Checkbox("Skeleton", &settings::visuals::skeleton);
        ImGui::ColorEdit4("Skeleton colour", settings::visuals::skeleton_colour, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Snapline", &settings::visuals::snapline);
        if (settings::visuals::snapline) {
            static constexpr const char* const origins = "Bottom\0Top\0Center\0Crosshair\0";
            ImGui::Combo("Origin", &settings::visuals::snapline_origin, origins);
            ImGui::ColorEdit4("Snapline colour", settings::visuals::snapline_colour, ImGuiColorEditFlags_NoInputs);
        }

        ImGui::Checkbox("Head dot", &settings::visuals::head_dot);
        if (settings::visuals::head_dot) {
            ImGui::ColorEdit4("Dot colour", settings::visuals::head_dot_colour, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Dot size", &settings::visuals::head_dot_size, 1.f, 12.f, "%.1f");
        }

        ImGui::Separator();
        ImGui::Checkbox("Offscreen arrows", &settings::visuals::offscreen_arrows);
        if (settings::visuals::offscreen_arrows) {
            ImGui::ColorEdit4("Arrow colour", settings::visuals::arrow_colour, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Arrow size", &settings::visuals::arrow_size, 6.f, 30.f, "%.0f");
            ImGui::SliderFloat("Arrow distance", &settings::visuals::arrow_distance, 40.f, 400.f, "%.0f");
        }

        ImGui::Checkbox("Crosshair", &settings::visuals::crosshair);
        if (settings::visuals::crosshair) {
            ImGui::ColorEdit4("Crosshair colour", settings::visuals::crosshair_colour, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Crosshair size", &settings::visuals::crosshair_size, 2.f, 30.f, "%.0f");
            ImGui::SliderFloat("Crosshair gap", &settings::visuals::crosshair_gap, 0.f, 20.f, "%.0f");
        }

        ImGui::Checkbox("Hitmarker", &settings::visuals::hitmarker);
        if (settings::visuals::hitmarker) {
            ImGui::ColorEdit4("Hitmarker colour", settings::visuals::hitmarker_colour, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Hitmarker size", &settings::visuals::hitmarker_size, 4.f, 24.f, "%.0f");
            ImGui::SliderFloat("Hitmarker time", &settings::visuals::hitmarker_time, 0.1f, 2.f, "%.1fs");
        }

        ImGui::Separator();
        ImGui::Checkbox("Radar", &settings::visuals::radar);
        if (settings::visuals::radar) {
            ImGui::SliderFloat("Radar size", &settings::visuals::radar_size, 80.f, 400.f, "%.0f");
            ImGui::SliderFloat("Radar range", &settings::visuals::radar_range, 20.f, 500.f, "%.0f");
            ImGui::SliderFloat("Radar zoom", &settings::visuals::radar_zoom, 0.2f, 4.f, "%.1f");
            ImGui::ColorEdit4("Radar bg", settings::visuals::radar_bg, ImGuiColorEditFlags_NoInputs);
        }

        ImGui::Separator();
        ImGui::Text("Checks");
        ImGui::Checkbox("Team check##vis", &settings::visuals::teamcheck);
        ImGui::Checkbox("Dead check##vis", &settings::visuals::deadcheck);
        ImGui::Checkbox("Wall dim##vis", &settings::visuals::wallcheck);

        ImGui::Separator();
        ImGui::Text("Misc");
        ImGui::Checkbox("Streamproof", &settings::visuals::streamproof);
        ImGui::Checkbox("Debug wallcheck", &settings::visuals::debug_wallcheck);
        ImGui::SliderFloat("Debug max length", &settings::visuals::debug_wallcheck_max_length, 25.f, 1000.f, "%.0f");
        break;
    }
    case 2:
    {
        ImGui::Text("Triggerbot");
        ImGui::Checkbox("Enable", &settings::triggerbot::enabled);
        ImGui::Checkbox("Hold-to-fire", &settings::triggerbot::require_key);
        key_bind("Trigger key", &settings::triggerbot::key);
        ImGui::SliderFloat("Delay ms", &settings::triggerbot::delay_ms, 0.f, 500.f, "%.0f");
        ImGui::SliderFloat("Radius px", &settings::triggerbot::fov_radius, 1.f, 60.f, "%.0f");
        ImGui::Checkbox("Team check##trig", &settings::triggerbot::teamcheck);
        ImGui::Checkbox("Dead check##trig", &settings::triggerbot::deadcheck);
        ImGui::Checkbox("Wall check##trig", &settings::triggerbot::wallcheck);
        ImGui::Separator();
        ImGui::TextWrapped("Aim at a player so their head/torso is inside the radius, then hold the trigger key. Untick Hold-to-fire to shoot whenever anything crosses the crosshair.");
        break;
    }
    case 3:
    {
        ImGui::Text("Legit / Movement");
        ImGui::Checkbox("Enable", &settings::legit::enabled);
        ImGui::SliderFloat("Walkspeed", &settings::legit::walkspeed, 8.f, 200.f, "%.0f");
        ImGui::SliderFloat("Jump power", &settings::legit::jumppower, 10.f, 300.f, "%.0f");
        ImGui::SliderFloat("Hip height", &settings::legit::hipheight, 0.f, 20.f, "%.1f");
        ImGui::Checkbox("Apply continuous", &settings::legit::apply_continuous);
        if (ImGui::Button("Apply once")) legit::apply_once();

        ImGui::Separator();
        ImGui::Checkbox("Fly (hold key + WASD)", &settings::legit::fly_enabled);
        key_bind("Fly key", &settings::legit::fly_key);
        ImGui::SliderFloat("Fly speed", &settings::legit::fly_speed, 10.f, 300.f, "%.0f");
        ImGui::TextWrapped("Fly: hold fly key, steer with WASD + Space/Ctrl.");
        break;
    }
    case 4:
    {
        ImGui::Text("SASA Config");
        ImGui::ColorEdit4("Accent", settings::menu_accent, ImGuiColorEditFlags_NoInputs);
        key_bind("Menu key", &settings::menu_key);
        ImGui::Checkbox("Show FPS", &settings::show_fps);
        ImGui::Checkbox("Show players", &settings::show_entity_count);
        ImGui::SliderFloat("FPS limit (0=off)", &settings::overlay_fps_limit, 0.f, 240.f, "%.0f");

        ImGui::Separator();
        ImGui::InputText("Name", settings::config::name, sizeof(settings::config::name));
        if (ImGui::Button("Save")) config::save(settings::config::name);
        ImGui::SameLine();
        if (ImGui::Button("Load")) config::load(settings::config::name);
        if (!settings::config::last_status.empty())
            ImGui::TextWrapped("%s", settings::config::last_status.c_str());
        break;
    }
    default:
        break;
    }

    ImGui::Separator();
    ImGui::TextDisabled("HOME toggles menu | click a key box, wait, then press the new key (ESC cancels)");

    ImGui::End();
}

void render_t::render_visuals()
{
    esp::run();
}