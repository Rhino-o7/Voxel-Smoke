#include "game_scene.h"
#include "imgui.h"
#include "application.h"
#include <string>

namespace yc::gui {

GameScene::GameScene(Player* player, yc::GameManager* gameManager) :
    player(player),
    gameManager(gameManager) {
}

void GameScene::render() {
    ImGuiIO& io = ImGui::GetIO();
    // ImGui::SetWindowFontScale(2);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::SetNextWindowBgAlpha(0.2);
    // move debug window to top-right (10px margin), pivot (1,0) anchors the window's right-top corner
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::Begin("debug", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
    ImGui::Text("FPS: %.0f", 1.0f / yc::Application::GetDeltaTime());
    if (gameManager && gameManager->getWorld()) {
        const auto cullingStats = gameManager->getWorld()->getCullingStats();
        ImGui::Text("Chunks Loaded/Visible/Culled:\n %zu / %zu / %zu", cullingStats.loadedChunks, cullingStats.visibleChunks, cullingStats.culledChunks);
    }

    auto postion = player->getCamera()->getPosition();
    ImGui::Text("Position: %.0f %.0f %.0f", postion.x, postion.y, postion.z);

    // Hover pollution at selected block
    if (player->isSelectingBlock()) {
        const yc::world::BlockPos b = player->getSelectingBlock();
        const double c = gameManager ? gameManager->getPollutionAtBlock(b) : 0.0;
        ImGui::Text("Pollution: %.6f", c);
    } else {
        ImGui::Text("Pollution: (none)");
    }

    ImGui::End();

    ImGui::SetNextWindowBgAlpha(0.3);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 50), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("inventory", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);

    auto inventory = player->getInventory();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 1));

    for (int slot = 0; slot < inventory.size(); ++slot) {
        ImVec2 size = { 40, 40 };
        GLuint id = Resource::BlockIcons[inventory[slot]].getId();

        if (slot == player->getCurrentSlot()) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.72f, 0.72f, 0.72f, 1));
            ImGui::SetNextWindowBgAlpha(0.3);
        } else {
            ImGui::SetNextWindowBgAlpha(0);
        }

        ImGui::BeginChildFrame(slot + 12345, ImVec2(56, 56), ImGuiWindowFlags_NoScrollbar);

        if (slot == player->getCurrentSlot()) {
            size = { 48, 48 };
            auto cursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cursor.x - 4, cursor.y - 4));
        }

        ImGui::Image((void*)(intptr_t)id, size);
        ImGui::EndChildFrame();
        ImGui::SameLine();

        if (slot == player->getCurrentSlot()) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 110), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.4);
    ImGui::Begin("inventory_header", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::Text(world::GetBlockName(player->getCurrentBlockType()).c_str());

    ImGui::End();

    if (gameManager && player->getCurrentBlockType() == yc::world::BlockType::CHIMNEY) {
        const auto& chimney = gameManager->getChimneySettings();

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 155), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.4);
        ImGui::Begin("chimney_settings", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);

        ImGui::Text("Height: %d", chimney.height);
        ImGui::Text("Radius: %d", chimney.radius);
        ImGui::Text("Exit Vel: %.1f", chimney.exitVelocity);
        ImGui::Separator();
        ImGui::Text("Edit from menu (1)");

        ImGui::End();
    }

    ImGui::PopStyleVar();
}

}