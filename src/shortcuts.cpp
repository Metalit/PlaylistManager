#include "shortcuts.hpp"

#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsViewController.hpp"
#include "System/Collections/Generic/IReadOnlyCollection_1.hpp"
#include "System/Collections/Generic/IReadOnlyList_1.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "assets.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/Components/ScrollView.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "customtypes/mainmenu.hpp"
#include "manager.hpp"
#include "playlistcore/shared/PlaylistCore.hpp"
#include "utils.hpp"

namespace Shortcuts {
    bool levelShortuctsInitialized = false;
    std::vector<PlaylistCore::Playlist*> playlists;
    GlobalNamespace::StandardLevelDetailViewController* levelDetailController = nullptr;
    BSML::ScrollView* scrollView;
    UnityEngine::UI::VerticalLayoutGroup* playlistLayout = {};
    std::vector<std::pair<HMUI::ImageView*, HMUI::HoverHint*>> playlistCells = {};
    UnityEngine::UI::Button* addButton = nullptr;

    bool packShortuctsInitialized = false;
    GlobalNamespace::LevelPackDetailViewController* packDetailController = nullptr;
    UnityEngine::UI::Button* editButton = nullptr;

    bool createShortuctsInitialized = false;
    UnityEngine::RectTransform* createButtonCanvas = nullptr;

    void SetupLevelShortcuts(GlobalNamespace::StandardLevelDetailViewController* levelDetail) {
        PlaylistManager::MainMenu::GetInstance(); // ensures invalidate will be called when needed
        levelDetailController = levelDetail;
        auto canvas = BSML::Lite::CreateCanvas()->GetComponent<UnityEngine::RectTransform*>();
        canvas->SetParent(levelDetail->_standardLevelDetailView->transform, false);
        canvas->localScale = {1, 1, 1};
        canvas->sizeDelta = {40, 60};
        canvas->anchoredPosition = {52, 0};
        canvas->GetComponent<UnityEngine::Canvas*>()->overrideSorting = true;
        auto scrollContent = BSML::Lite::CreateScrollView(canvas);
        scrollContent->AddComponent<UnityEngine::UI::LayoutElement*>()->minWidth = 6;
        playlistLayout = scrollContent->GetComponent<UnityEngine::UI::VerticalLayoutGroup*>();
        playlistLayout->childControlHeight = false;
        playlistLayout->childForceExpandHeight = false;
        playlistLayout->spacing = 2;
        scrollView = canvas->GetChild(0)->GetComponent<BSML::ScrollView*>();
        auto scrollViewRect = scrollView->GetComponent<UnityEngine::RectTransform*>();
        scrollViewRect->anchoredPosition = {-10, 10};
        scrollViewRect->sizeDelta = {-25, -18};
        addButton = BSML::Lite::CreateUIButton(canvas, "+", "ActionButton", []() {
            if (levelDetailController)
                Manager::PresentAddShortcut(levelDetailController->beatmapLevel);
        });
        addButton->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 0;
        BSML::Lite::AddHoverHint(addButton, "Add song to playlist");
        levelShortuctsInitialized = true;
    }

    std::pair<HMUI::ImageView*, HMUI::HoverHint*> CreatePlaylistCell(int index) {
        auto playlist = playlists[index];
        auto image = BSML::Lite::CreateClickableImage(playlistLayout, PlaylistCore::GetCoverImage(playlist), [index]() {
            if (index < playlists.size())
                Manager::PresentEditShortcut(playlists[index]);
        });
        image->_skew = 0.18;
        image->preserveAspect = true;
        image->rectTransform->sizeDelta = {6, 6};
        image->material = Utils::GetCurvedCornersMaterial();
        auto hover = BSML::Lite::AddHoverHint(image, playlist->name);
        return {image, hover};
    }

    void RefreshPlaylistList() {
        if (!levelShortuctsInitialized)
            return;
        playlists = Utils::GetPlaylistsWithSong(levelDetailController->beatmapLevel);
        for (int i = 0; i < playlists.size() || i < playlistCells.size(); i++) {
            if (i >= playlists.size())
                playlistCells[i].first->gameObject->active = false;
            else if (i >= playlistCells.size())
                playlistCells.emplace_back(CreatePlaylistCell(i));
            else {
                playlistCells[i].first->gameObject->active = true;
                playlistCells[i].first->sprite = PlaylistCore::GetCoverImage(playlists[i]);
                playlistCells[i].second->text = playlists[i]->name;
            }
        }
        scrollView->ScrollTo(0, false);
        BSML::MainThreadScheduler::ScheduleNextFrame([]() {
            float height = playlistLayout->rectTransform->sizeDelta.y;
            float y = height == 0 ? -5 : std::max(-7 - height, (float) -43);
            addButton->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {6, y};
        });
    }

    void RefreshLevelShortcuts(GlobalNamespace::StandardLevelDetailViewController* levelDetail) {
        if (!levelShortuctsInitialized)
            SetupLevelShortcuts(levelDetail);
        RefreshPlaylistList();
    }

    void SetupPackShortcuts(GlobalNamespace::LevelPackDetailViewController* packDetail) {
        PlaylistManager::MainMenu::GetInstance(); // ensures invalidate will be called when needed
        packDetailController = packDetail;
        editButton = BSML::Lite::CreateUIButton(packDetail->_detailWrapper, "", "ActionButton", {61, -4}, []() {
            if (packDetailController)
                Manager::PresentEditShortcut(packDetailController->_pack);
        });
        auto buttonLayout = editButton->GetComponent<UnityEngine::UI::LayoutElement*>();
        buttonLayout->preferredWidth = 7;
        buttonLayout->preferredHeight = 7;
        auto editIcon = BSML::Lite::CreateImage(editButton, PNG_SPRITE(edit));
        editIcon->preserveAspect = true;
        for (auto image : editButton->GetComponentsInChildren<HMUI::ImageView*>()) {
            image->_skew = 0;
            image->__Refresh();
        }
        packShortuctsInitialized = true;
    }

    void RefreshPackShortcuts(GlobalNamespace::LevelPackDetailViewController* packDetail) {
        if (!packShortuctsInitialized)
            SetupPackShortcuts(packDetail);
        editButton->gameObject->active = PlaylistCore::GetPlaylistWithPrefix(packDetail->_pack->packID) != nullptr;
    }

    void SetupCreateShortcuts(GlobalNamespace::AnnotatedBeatmapLevelCollectionsViewController* packsView) {
        PlaylistManager::MainMenu::GetInstance(); // ensures invalidate will be called when needed
        createButtonCanvas = BSML::Lite::CreateCanvas()->GetComponent<UnityEngine::RectTransform*>();
        createButtonCanvas->SetParent(packsView->transform, false);
        createButtonCanvas->localScale = {1, 1, 1};
        createButtonCanvas->sizeDelta = {10, 10};
        createButtonCanvas->GetComponent<UnityEngine::Canvas*>()->overrideSorting = true;
        auto button = BSML::Lite::CreateUIButton(createButtonCanvas, "+", "ActionButton", []() { Manager::PresentCreateShortcut(); });
        button->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 0;
        auto rect = button->GetComponent<UnityEngine::RectTransform*>();
        rect->anchorMin = {0, 0};
        rect->anchorMax = {1, 1};
        BSML::Lite::AddHoverHint(button, "Create new playlist");
        for (auto text : button->GetComponentsInChildren<TMPro::TextMeshProUGUI*>())
            text->fontStyle = TMPro::FontStyles::Normal;
        for (auto image : button->GetComponentsInChildren<HMUI::ImageView*>()) {
            image->_skew = 0;
            image->__Refresh();
        }
        createShortuctsInitialized = true;
    }

    void RefreshCreateShortcuts(GlobalNamespace::LevelFilteringNavigationController* filterController) {
        auto packsView = filterController->_annotatedBeatmapLevelCollectionsViewController;
        if (!createShortuctsInitialized)
            SetupCreateShortcuts(packsView);
        if (filterController->selectedLevelCategory == GlobalNamespace::SelectLevelCategoryViewController::LevelCategory::CustomSongs) {
            int count = packsView->_annotatedBeatmapLevelCollections->i___System__Collections__Generic__IReadOnlyCollection_1_T_()->Count;
            count = std::min(count, 6);
            createButtonCanvas->anchoredPosition = {(float) count * 15 - 39, 3};
            createButtonCanvas->gameObject->active = true;
        } else
            createButtonCanvas->gameObject->active = false;
    }

    void Invalidate() {
        levelShortuctsInitialized = false;
        playlists.clear();
        levelDetailController = nullptr;
        scrollView = nullptr;
        playlistLayout = nullptr;
        playlistCells.clear();
        addButton = nullptr;

        packShortuctsInitialized = false;
        packDetailController = nullptr;
        editButton = nullptr;

        createShortuctsInitialized = false;
        createButtonCanvas = nullptr;
    }
}
