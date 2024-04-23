#pragma once

#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "HMUI/IconSegmentedControl.hpp"
#include "HMUI/InputFieldView.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/SimpleTextDropdown.hpp"
#include "HMUI/TableView.hpp"
#include "UnityEngine/UI/Toggle.hpp"
#include "playlistcore/shared/PlaylistCore.hpp"

namespace Utils {
    bool CaseInsensitiveEquals(std::string a, std::string b);

    std::set<int> GetSelected(HMUI::TableView* tableView);

    StringW CharacteristicName(GlobalNamespace::BeatmapCharacteristicSO* characteristic);

    void AnimateModal(HMUI::ModalView* modal, bool out);

    void AddModalAnimations(HMUI::SimpleTextDropdown* dropdown, HMUI::ModalView* behindModal);

    std::vector<PlaylistCore::Playlist*> GetPlaylistsWithSong(GlobalNamespace::BeatmapLevel* level);

    int GetLevelIndex(PlaylistCore::Playlist* playlist, GlobalNamespace::BeatmapLevel* level);

    PlaylistCore::BPSong* GetLevelJson(PlaylistCore::Playlist* playlist, GlobalNamespace::BeatmapLevel* level);

    std::set<int> GetHighlightedDifficulties(PlaylistCore::BPSong const& song, GlobalNamespace::BeatmapCharacteristicSO* characteristic);

    bool IsDifficultyHighlighted(PlaylistCore::BPSong const& song, GlobalNamespace::BeatmapCharacteristicSO* characteristic, int difficulty);

    void SetDifficultyHighlighted(PlaylistCore::BPSong& song, std::string characteristicSerializedName, int difficulty, bool value);

    void SetupIcons(HMUI::IconSegmentedControl* iconControl);

    void DeselectAllCells(HMUI::IconSegmentedControl* iconControl);

    void SetCellInteractable(HMUI::IconSegmentedControl* iconControl, int idx, bool interactable);

    void InstantSetToggle(UnityEngine::UI::Toggle* toggle, bool value);

    HMUI::InputFieldView* CreateInput(
        TMPro::TextMeshProUGUI* text,
        HMUI::ImageView* caret,
        UnityEngine::UI::Button* clear,
        UnityEngine::GameObject* placeholder,
        UnityEngine::Vector2 keyboardOffset,
        std::function<void(StringW)> onInput,
        UnityEngine::Color textColor = {1, 1, 1, 1},
        UnityEngine::Color highlightColor = {0.6, 0.6, 0.6, 1}
    );

    std::shared_future<UnityEngine::Texture2D*> GenerateCoverImage(std::span<GlobalNamespace::BeatmapLevel*> levels);

    UnityEngine::Material* GetCurvedCornersMaterial();
}
