#include "save_select_scene.h"
#include "application.h"
#include <imgui.h>
#include <random>
#include <algorithm>

namespace {
int32_t GenerateRandomSeed() {
    static std::random_device rd;
    static std::mt19937 rng(rd());
    static std::uniform_int_distribution<int32_t> dist;
    return dist(rng);
}
}

namespace yc::gui {

SaveSelectScene::SaveSelectScene(Application* application) :
    application(application) {
    randomSeed = GenerateRandomSeed();
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
    const float uiScale = std::clamp(application ? application->getSettings().ui.scale : 1.0f, 0.75f, 2.0f);

    const ImVec2 windowSize(
        std::min(io.DisplaySize.x * 0.9f, 480.0f * uiScale),
        std::min(io.DisplaySize.y * 0.9f, 420.0f * uiScale));

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;
    ImGui::Begin("Select Save", nullptr, flags);

    const bool showConnectUi = application->shouldShowServerConnectInSaveSelect();
    if (showConnectUi) {
        ImGui::InputText("Server", serverAddress, sizeof(serverAddress));
        ImGui::SameLine();
        if (ImGui::Button("Connect")) {
            connectError.clear();
            if (!application->connectToServer(serverAddress, connectError)) {
                if (connectError.empty()) {
                    connectError = "Failed to connect";
                }
            } else {
                refreshList();
            }
        }

        ImGui::Text("Status: %s", application->isServerConnected() ? "Connected" : "Disconnected");
        if (!connectError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", connectError.c_str());
        }

        ImGui::Separator();
    }

    if (!application->isServerConnected()) {
        ImGui::Text("Server is disconnected.");
        if (!showConnectUi) {
            ImGui::Text("Reconnect from main menu by disconnecting to title.");
        }
        ImGui::End();
        return;
    }

    if (!actionError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", actionError.c_str());
    }

    if (ImGui::Button("Refresh")) {
        refreshList();
    }

    ImGui::Separator();
    ImGui::Text("Saves:");

    ImGui::BeginChild("save_list", ImVec2(0.0f, 200.0f * uiScale), true);
    for (int i = 0; i < static_cast<int>(saveNames.size()); ++i) {
        bool selected = (i == selectedIndex);
        if (ImGui::Selectable(saveNames[i].c_str(), selected)) {
            selectedIndex = i;
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::InputText("New Save Name", newSaveName, sizeof(newSaveName));

    ImGui::Checkbox("Use Random Seed", &useRandomSeed);
    if (useRandomSeed) {
        ImGui::Text("Seed: %d", randomSeed);
        ImGui::SameLine();
        if (ImGui::Button("Reroll")) {
            randomSeed = GenerateRandomSeed();
        }
    } else {
        ImGui::InputInt("Seed", &manualSeed);
    }

    const bool hasSelection = selectedIndex >= 0 && selectedIndex < static_cast<int>(saveNames.size());

    if (ImGui::Button("Load Selected")) {
        if (!hasSelection) {
            actionError = "Select a save first.";
        } else if (application->loadSave(saveNames[selectedIndex])) {
            actionError.clear();
            application->setSaveSelectionActive(false);
        } else {
            actionError = "Load failed. Verify server save data compatibility.";
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Create New")) {
        if (newSaveName[0] == '\0') {
            actionError = "Enter a save name first.";
        } else {
            const int32_t seed = useRandomSeed ? randomSeed : manualSeed;
            if (application->createNewSave(newSaveName, seed)) {
                actionError.clear();
                newSaveName[0] = '\0';
                refreshList();
                application->setSaveSelectionActive(false);
            } else {
                actionError = "Create failed. Save may already exist or server rejected request.";
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete Selected")) {
        if (!hasSelection) {
            actionError = "Select a save first.";
        } else if (application->deleteSave(saveNames[selectedIndex])) {
            actionError.clear();
            selectedIndex = -1;
            refreshList();
        } else {
            actionError = "Delete failed.";
        }
    }

    ImGui::End();
}

}