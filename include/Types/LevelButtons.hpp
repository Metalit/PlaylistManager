#pragma once

#include "custom-types/shared/coroutine.hpp"

#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/UI/Button.hpp"

#include "HMUI/ModalView.hpp"

#include "TMPro/TextMeshProUGUI.hpp"

#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/CustomBeatmapLevelPack.hpp"
#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"

namespace PlaylistCore {
    class Playlist;
    class CustomListSource;
}

namespace PlaylistManager {

    class ButtonsContainer {

        private:
        UnityEngine::GameObject *layoutObject, *movementButtonsContainer;
        UnityEngine::UI::Button *saveCoverButton, *playlistAddButton, *playlistRemoveButton;
        HMUI::ModalView *playlistAddModal, *infoModal, *removeModal;
        PlaylistCore::CustomListSource *playlistCovers;
        GlobalNamespace::StandardLevelDetailView *levelDetailView;
        GlobalNamespace::LevelCollectionTableView *levelListTableView;
        GlobalNamespace::IPreviewBeatmapLevel* currentLevel;
        PlaylistCore::Playlist* currentPlaylist = nullptr;

        std::vector<PlaylistCore::Playlist*> loadedPlaylists;
        
        bool deleteSongOnRemoval = false;

        bool hasConstructed = false, visibleOnFinish = false, inPlaylistOnFinish = false, isWIPOnFinish = false;

        custom_types::Helpers::Coroutine initCoroutine();

        void saveCoverButtonPressed();
        void addToPlaylistButtonPressed();
        void removeFromPlaylistButtonPressed();
        void playlistSelected(int listCellIdx);
        void scrollListLeftButtonPressed();
        void scrollListRightButtonPressed();
        void confirmRemovalButtonPressed();
        void cancelRemovalButtonPressed();
        void deleteSongToggleToggled(bool enabled);
        void moveUpButtonPressed();
        void moveDownButtonPressed();

        public:
        static ButtonsContainer* buttonsInstance;

        void Init(GlobalNamespace::StandardLevelDetailView* levelDetailView);
        void SetVisible(bool visible, bool inPlaylist, bool WIP);
        void SetLevel(GlobalNamespace::IPreviewBeatmapLevel* level);
        void SetPlaylist(PlaylistCore::Playlist* playlist);
        void RefreshPlaylists();
        void RefreshHighlightedDifficulties();
        void Destroy();
    };
}