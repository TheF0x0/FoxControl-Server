/**
 * @author F0x0
 * @since 04/04/2023
 */

#include <SDL.h>
#include <SDL_rwops.h>
#include <SDL_video.h>
#include <SDL_image.h>
#include <SDL_messagebox.h>
#include <SDL_clipboard.h>
#include <glad/gl.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "monitor.hpp"
#include "server.hpp"
#include "gateway.hpp"
#include "imgui_utils.hpp"

namespace fox {
    Monitor::Monitor(Server& server, Gateway& gateway) noexcept:
            _server(server),
            _gateway(gateway),
            _is_running(true),
            _is_close_requested(false),
            _is_mouse_down(false),
            _is_session_password_visible(false),
            _auto_power_state(true),
            _current_slider_speed(),
            _previous_slider_speed(),
            _speed_history(NUM_SPEED_HISTORY_ENTRIES),
            _speed_delta_history(NUM_SPEED_HISTORY_ENTRIES),
            _previous_speed(),
            _current_speed(),
            _current_mode(dto::Mode::DEFAULT),
            _device_log_buffer(),
            _device_log_auto_scroll(true),
            _device_log_prev_scroll_y(),
            _device_log_curr_scroll_y(),
            _gateway_log_buffer(),
            _gateway_log_auto_scroll(true),
            _gateway_log_prev_scroll_y(),
            _gateway_log_curr_scroll_y() {
        std::fill(_speed_history.begin(), _speed_history.end(), 0.0F);
        std::fill(_speed_delta_history.begin(), _speed_delta_history.end(), 0.0F);

        server.attach_monitor(this);
        gateway.attach_monitor(this);
    }

    auto Monitor::run() noexcept -> kstd::Result<void> {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            return {std::unexpected("Could not initialize SDL subsystems")};
        }

        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        spdlog::info("Creating window");
        auto* window = SDL_CreateWindow(R"(🦊Control)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 800, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

        if (window == nullptr) {
            return {std::unexpected("Could not create monitor window")};
        }

        load_window_icon(window);

        if (SDL_GL_LoadLibrary(nullptr) != 0) {
            return {std::unexpected("Could not load OpenGL library")};
        }

        auto* gl_context = SDL_GL_CreateContext(window);

        if (gl_context == nullptr) {
            return {std::unexpected("Could not create OpenGL context")};
        }

        gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));

        auto* imgui_context = ImGui::CreateContext();

        if (imgui_context == nullptr) {
            return {std::unexpected("Could not create ImGui context")};
        }

        ImGui::SetCurrentContext(imgui_context);
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 330");

        SDL_GL_SetSwapInterval(1);
        SDL_ShowWindow(window);
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);

        while (_is_running) {
            SDL_Event event;

            while (SDL_PollEvent(&event) > 0) {
                handle_event(window, event);
            }

            if (_is_close_requested) {
                _is_running = false;
                _is_close_requested = false;
                spdlog::info("Requesting window close");
            }

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            render_window(window);
            SDL_GL_SwapWindow(window);
        }

        spdlog::info("Destroying window");

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext(imgui_context);

        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return {};
    }

    auto Monitor::load_window_icon(SDL_Window* window) noexcept -> void {
        auto* rw_ops = SDL_RWFromFile("icon.png", "r");
        auto* surface = IMG_LoadPNG_RW(rw_ops);
        SDL_SetWindowIcon(window, surface);
        SDL_FreeSurface(surface);
        SDL_FreeRW(rw_ops);
    }

    auto Monitor::render_window(SDL_Window* window) noexcept -> void {
        ImGui_ImplSDL2_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        kstd::i32 width = 0;
        kstd::i32 height = 0;
        SDL_GetWindowSize(window, &width, &height);
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({static_cast<kstd::f32>(width), static_cast<kstd::f32>(height)});

        update_data();
        populate_window();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ImGui::EndFrame();
    }

    auto Monitor::populate_window() noexcept -> void {
        if (ImGui::Begin("FoxControl", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
            populate_controls();

            ImGui::BeginTable("##console_container", 2, ImGuiTableFlags_Resizable);
            ImGui::TableSetupColumn("##device_log_column", ImGuiTableColumnFlags_NoHeaderLabel | ImGuiTableColumnFlags_WidthStretch, 0.5F);
            ImGui::TableSetupColumn("##gateway_log_column", ImGuiTableColumnFlags_NoHeaderLabel | ImGuiTableColumnFlags_WidthStretch, 0.5F);
            ImGui::TableNextColumn();
            populate_device_log();
            ImGui::TableNextColumn();
            populate_gateway_log();
            ImGui::TableNextRow();
            ImGui::EndTable();
        }

        ImGui::End();
    }

    auto Monitor::populate_controls() noexcept -> void {
        constexpr ImVec4 active_color{1.0F, 0.0F, 0.0F, 1.0F};
        constexpr ImVec4 inactive_color{0.0F, 1.0F, 0.0F, 1.0F};
        auto is_on = _server.is_on();

        ImGui::Text("Power");

        if (is_on) {
            imgui::push_disabled();
        }

        if (ImGui::Button("ON") && !is_on) {
            _current_slider_speed = 1;
            _server.set_is_on(true);
        }

        if (is_on) {
            imgui::pop_disabled();
        }

        ImGui::SameLine();

        if (!is_on) {
            imgui::push_disabled();
        }

        if (ImGui::Button("OFF") && is_on) {
            _current_slider_speed = 0;
            _server.set_is_on(false);
        }

        if (!is_on) {
            imgui::pop_disabled();
        }

        ImGui::SameLine();

        is_on = _server.is_on();
        const auto status_color = is_on ? active_color : inactive_color;
        const auto* status_text = is_on ? "Running" : "Idle";
        ImGui::TextColored(status_color, "%s", status_text);

        ImGui::Checkbox("Auto Power State", &_auto_power_state);

        ImGui::Separator();

        if (_is_session_password_visible) {
            const auto& current_password = _gateway.get_session_password();

            if (current_password != _session_display_password) {
                _session_display_password = current_password;
            }

            if (ImGui::Button("Hide")) {
                hide_session_password();
            }
        }
        else {
            const auto size = _gateway.get_session_password().size();

            if (size != _session_display_password.size()) {
                _session_display_password.resize(size);
                std::fill(_session_display_password.begin(), _session_display_password.end(), '*');
            }

            if (ImGui::Button("Show")) {
                show_session_password();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Copy")) {
            SDL_SetClipboardText(_gateway.get_session_password().c_str());
            spdlog::info("Copied session password to clipboard");
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset")) {
            _gateway.reset_session();
            spdlog::info("Requested new session");
        }

        ImGui::SameLine();
        ImGui::Text("Session Password: %s", _session_display_password.c_str());

        ImGui::Separator();
        ImGui::Text("Controls");

        const auto cannot_change_state = _server.get_actual_speed() != _server.get_target_speed();
        const auto cannot_change_mode = cannot_change_state || !is_on;
        const auto current_mode = _server.get_mode();
        const auto current_mode_name = get_mode_name(current_mode);

        if (cannot_change_mode) {
            imgui::push_disabled();
        }

        if (ImGui::BeginCombo("Mode", current_mode_name.c_str())) {
            for (size_t i = 0; i < NUM_MODES; i++) {
                const auto entry_mode = MODES[i];
                const auto mode_name = get_mode_name(entry_mode);

                if (ImGui::Selectable(mode_name.c_str(), entry_mode == current_mode)) {
                    // TODO: implement mode selection
                }
            }

            ImGui::EndCombo();
        }

        if (cannot_change_mode) {
            imgui::pop_disabled();
        }

        ImGui::Text("Target Speed: %d", _server.get_target_speed());
        ImGui::Text("Actual Speed: %d", _server.get_actual_speed());
        ImGui::PlotLines("Speed History", _speed_history.data(), NUM_SPEED_HISTORY_ENTRIES, 0, nullptr, static_cast<kstd::f32>(MIN_SPEED), static_cast<kstd::f32>(MAX_SPEED), {0, 120.0F});
        ImGui::PlotHistogram("Speed Delta", _speed_delta_history.data(), NUM_SPEED_HISTORY_ENTRIES, 0, nullptr, static_cast<kstd::f32>(MIN_SPEED), static_cast<kstd::f32>(MAX_SPEED), {0, 120.0F});

        if (cannot_change_state) {
            imgui::push_disabled();
        }

        ImGui::SliderInt("Speed", &_current_slider_speed, MIN_SPEED, MAX_SPEED);

        if (cannot_change_state) {
            imgui::pop_disabled();

        }

        update_speed_if_needed();
    }

    auto Monitor::populate_device_log() noexcept -> void {
        ImGui::Text("Device Log");

        if (ImGui::Button("Clear")) {
            clear_device_log();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Autoscroll", &_device_log_auto_scroll);

        ImGui::BeginChild("Device Log", {0, 0}, true, ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped("%s", get_device_log().c_str());

        if (_device_log_auto_scroll) {
            ImGui::SetScrollY(ImGui::GetScrollMaxY());
        }

        ImGui::EndChild();
    }

    auto Monitor::populate_gateway_log() noexcept -> void {
        ImGui::Text("Gateway Log");

        if (ImGui::Button("Clear")) {
            clear_gateway_log();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Autoscroll", &_device_log_auto_scroll);

        ImGui::BeginChild("Gateway Log", {0, 0}, true, ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped("%s", get_gateway_log().c_str());

        if (_gateway_log_auto_scroll) {
            ImGui::SetScrollY(ImGui::GetScrollMaxY());
        }

        ImGui::EndChild();
    }

    auto Monitor::handle_event(SDL_Window* window, const SDL_Event& event) noexcept -> void {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    _is_mouse_down = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    _is_mouse_down = false;
                }
                break;
            case SDL_QUIT:
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Wanna quit?", "If you want to quit the application, enter 'exit' into the system console", nullptr);
                break;
        }
    }

    auto Monitor::update_data() noexcept -> void {
        const auto target_speed = _server.get_target_speed();

        if (_current_slider_speed != target_speed) {
            _current_slider_speed = target_speed;
        }

        _previous_speed = _current_speed;
        _current_speed = _server.get_actual_speed();

        _speed_history.erase(_speed_history.begin());
        _speed_history.push_back(static_cast<kstd::f32>(_current_speed));

        _speed_delta_history.erase(_speed_delta_history.begin());
        _speed_delta_history.push_back(static_cast<kstd::f32>(_current_speed) - static_cast<kstd::f32>(_previous_speed));
    }

    auto Monitor::update_speed_if_needed() noexcept -> void {
        if (_is_mouse_down || _previous_slider_speed == _current_slider_speed) {
            return; // Only update when mouse is released
        }

        _previous_slider_speed = _current_slider_speed;

        if (_auto_power_state && !_server.is_on() && _current_slider_speed > 0) {
            _server.set_is_on(true);
        }

        if (_auto_power_state && _server.is_on() && _current_slider_speed == 0) {
            _server.set_is_on(false);
        }

        _server.set_speed(_current_slider_speed);
    }

    auto Monitor::get_device_log() noexcept -> std::string {
        std::scoped_lock lock(_device_log_mutex);
        std::string result;

        for (const auto& line: _device_log_buffer) {
            result += line + '\n';
        }

        return result;
    }

    auto Monitor::get_gateway_log() noexcept -> std::string {
        std::scoped_lock lock(_gateway_log_mutex);
        std::string result;

        for (const auto& line: _gateway_log_buffer) {
            result += line + '\n';
        }

        return result;
    }

    auto Monitor::show_session_password() noexcept -> void {
        if (_is_session_password_visible) {
            return;
        }

        _session_display_password = _gateway.get_session_password();
        _is_session_password_visible = true;
    }

    auto Monitor::hide_session_password() noexcept -> void {
        if (!_is_session_password_visible) {
            return;
        }

        const auto size = _gateway.get_session_password().size();
        _session_display_password.resize(size);
        std::fill(_session_display_password.begin(), _session_display_password.end(), '*');
        _is_session_password_visible = false;
    }
}