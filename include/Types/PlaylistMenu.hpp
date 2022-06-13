#pragma once

#include "custom-types/shared/coroutine.hpp"

#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/UI/Button.hpp"

#include "HMUI/ImageView.hpp"
#include "HMUI/InputFieldView.hpp"
#include "HMUI/ModalView.hpp"

#include "TMPro/TextMeshProUGUI.hpp"

#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridView.hpp"

namespace PlaylistCore {
    class Playlist;
    class CustomListSource;
}

DECLARE_CLASS_CODEGEN(PlaylistManager, PlaylistMenu, UnityEngine::MonoBehaviour,

    private:
    UnityEngine::GameObject *buttonsContainer, *detailsContainer, *bootstrapContainer;
    HMUI::InputFieldView *playlistTitle, *playlistAuthor;
    TMPro::TextMeshProUGUI *playlistDescription, *descriptionTitle, *syncingText;
    UnityEngine::UI::Button *coverButton, *createButton, *cancelButton, *deleteButton, *syncButton, *downloadButton;
    HMUI::ImageView *coverImage, *packImage;
    HMUI::ModalView *confirmModal, *coverModal, *syncingModal;
    PlaylistCore::CustomListSource *list;

    int coverImageIndex;

    std::string currentTitle, currentAuthor;

    bool detailsVisible, inMovement, addingPlaylist, awaitingSync;
    bool hasConstructed, visibleOnFinish, customOnFinish;

    GlobalNamespace::AnnotatedBeatmapLevelCollectionsGridView* gameTableView;
    PlaylistCore::Playlist* playlist;

    custom_types::Helpers::Coroutine moveCoroutine(bool reversed);
    custom_types::Helpers::Coroutine refreshCoroutine();
    custom_types::Helpers::Coroutine syncCoroutine();

    custom_types::Helpers::Coroutine initCoroutine();

    void updateDetailsMode();
    void showDetails(bool visible);
    void refreshDetails();

    void infoButtonPressed();
    void syncButtonPressed();
    void downloadButtonPressed();
    void addButtonPressed();
    void moveRightButtonPressed();
    void moveLeftButtonPressed();
    void playlistTitleTyped(std::string newValue);
    void playlistAuthorTyped(std::string newValue);
    void coverButtonPressed();
    void deleteButtonPressed();
    void createButtonPressed();
    void cancelButtonPressed();
    void confirmDeleteButtonPressed();
    void cancelDeleteButtonPressed();
    void coverSelected(int listCellIdx);
    void scrollListLeftButtonPressed();
    void scrollListRightButtonPressed();

    public:
    static std::function<void()> nextCloseKeyboard;
    static PlaylistMenu* menuInstance;

    void Init(HMUI::ImageView* packImage);
    void SetPlaylist(PlaylistCore::Playlist* playlist);
    void RefreshCovers();

    void SetVisible(bool visible, bool custom = false);
    void ShowInfoMenu();
    void ShowCreateMenu();
    void Destroy();
)