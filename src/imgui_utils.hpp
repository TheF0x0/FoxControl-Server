/**
 * @author F0x0
 * @since 05/04/2023
 */

#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace fox::imgui {
    inline auto push_disabled() noexcept -> void {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5F);
    }

    inline auto pop_disabled() noexcept -> void {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
}