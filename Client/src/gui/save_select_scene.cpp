#include "save_select_scene.h"
#include "application.h"
#include <imgui.h>

namespace yc::gui {

SaveSelectScene::SaveSelectScene(Application* application) :
    application(application) {
    refreshList();
}

void SaveSelectScene::refreshList() {
    saveNames = application->listSaves();
    if (selectedIndex >= static_cast<int>(saveNames.size())) {
        selectedIndex = -1;
    }
}

void SaveSelectScene::render() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 420.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;
    ImGui::Begin("Select Save", nullptr, flags);

    if (ImGui::Button("Refresh")) {
        refreshList();
    }

    ImGui::Separator();
    ImGui::Text("Saves:");

    ImGui::BeginChild("save_list", ImVec2(0.0f, 200.0f), true);
    for (int i = 0; i < static_cast<int>(saveNames.size()); ++i) {
        bool selected = (i == selectedIndex);
        if (ImGui::Selectable(saveNames[i].c_str(), selected)) {
            selectedIndex = i;
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::InputText("New Save Name", newSaveName, sizeof(newSaveName));

    const bool hasSelection = selectedIndex >= 0 && selectedIndex < static_cast<int>(saveNames.size());

    if (ImGui::Button("Load Selected") && hasSelection) {
        if (application->loadSave(saveNames[selectedIndex])) {
            application->setSaveSelectionActive(false);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Create New") && newSaveName[0] != '\0') {
        if (application->createNewSave(newSaveName)) {
            application->setSaveSelectionActive(false);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete Selected") && hasSelection) {
        if (application->deleteSave(saveNames[selectedIndex])) {
            selectedIndex = -1;
            refreshList();
        }
    }

    ImGui::End();
}

}