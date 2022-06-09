#include "Main.hpp"
#include "Types/SongDownloaderAddon.hpp"
#include "Types/PlaylistMenu.hpp"
#include "Types/PlaylistFilters.hpp"
#include "Types/LevelButtons.hpp"
#include "Types/GridViewAddon.hpp"
#include "Types/Scroller.hpp"
#include "Types/Config.hpp"
#include "PlaylistManager.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "ResettableStaticPtr.hpp"

#include <chrono>

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "songloader/shared/API.hpp"

#include "questui/shared/QuestUI.hpp"

#include "GlobalNamespace/StandardLevelDetailViewController.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelPackDetailViewController.hpp"
#include "GlobalNamespace/LevelPackDetailViewController_ContentType.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsViewController.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridViewAnimator.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionCell.hpp"
#include "GlobalNamespace/PageControl.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "GlobalNamespace/StandardLevelInfoSaveData.hpp"
#include "GlobalNamespace/ISpriteAsyncLoader.hpp"
#include "GlobalNamespace/EnvironmentInfoSO.hpp"
#include "GlobalNamespace/PreviewDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"

#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Rect.hpp" // This needs to be included before RectTransform
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Events/UnityAction.hpp"
#include "UnityEngine/UI/Button_ButtonClickedEvent.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "HMUI/TableView.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/ViewController_AnimationType.hpp"
#include "HMUI/FlowCoordinator.hpp"
#include "HMUI/InputFieldView.hpp"
#include "Tweening/TimeTweeningManager.hpp"
#include "Tweening/Vector2Tween.hpp"
#include "Zenject/DiContainer.hpp"
#include "Zenject/StaticMemoryPool_7.hpp"
#include "System/Tuple_2.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"

using namespace GlobalNamespace;
using namespace PlaylistManager;

ModInfo modInfo;

// shared config data
PlaylistConfig playlistConfig;
Folder* currentFolder = nullptr;
int filterSelectionState = 0;
bool allowInMultiplayer = false;

Logger& getLogger() {
    static auto logger = new Logger(modInfo, LoggerOptions(false, true)); 
    return *logger;
}

std::string GetPlaylistsPath() {
    static std::string playlistsPath(getDataDir(modInfo) + "Playlists");
    return playlistsPath;
}

std::string GetConfigPath() {
    static std::string configPath = Configuration::getConfigFilePath(modInfo);
    return configPath;
}

std::string GetCoversPath() {
    static std::string coversPath(getDataDir(modInfo) + "Covers");
    return coversPath;
}

using TupleType = System::Tuple_2<int, int>;
// small fix for horizontal tables
MAKE_HOOK_MATCH(TableView_GetVisibleCellsIdRange, &HMUI::TableView::GetVisibleCellsIdRange,
        TupleType*, HMUI::TableView* self) {
    
    using namespace HMUI;
    UnityEngine::Rect rect = self->viewportTransform->get_rect();

    float heightWidth = (self->tableType == TableView::TableType::Vertical) ? rect.get_height() : rect.get_width();
    float position = (self->tableType == TableView::TableType::Vertical) ? self->scrollView->get_position() : -self->scrollView->get_position();

    int min = floor(position / self->cellSize + self->cellSize * 0.001f);
    if (min < 0) {
        min = 0;
    }

    int max = floor((position + heightWidth - self->cellSize * 0.001f) / self->cellSize);
    if (max > self->numberOfCells - 1) {
        max = self->numberOfCells - 1;
    }

    return TupleType::New_ctor(min, max);
}

// allow name and author changes to be made on keyboard close (assumes only one keyboard will be open at a time)
MAKE_HOOK_MATCH(InputFieldView_DeactivateKeyboard, &HMUI::InputFieldView::DeactivateKeyboard,
        void, HMUI::InputFieldView* self, HMUI::UIKeyboard* keyboard) {

    InputFieldView_DeactivateKeyboard(self, keyboard);

    if(PlaylistMenu::nextCloseKeyboard) {
        PlaylistMenu::nextCloseKeyboard();
        PlaylistMenu::nextCloseKeyboard = nullptr;
    }
}

// ensure input field clear buttons are updated on their first appearance
MAKE_HOOK_MATCH(InputFieldView_Awake, &HMUI::InputFieldView::Awake,
        void, HMUI::InputFieldView* self) {
    
    InputFieldView_Awake(self);

    self->UpdateClearButton();
}

// prevent download icon showing up on empty custom playlists unless configured to
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync, &AnnotatedBeatmapLevelCollectionCell::RefreshAvailabilityAsync,
        void, AnnotatedBeatmapLevelCollectionCell* self, AdditionalContentModel* contentModel) {
    
    AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync(self, contentModel);

    auto pack = il2cpp_utils::try_cast<IBeatmapLevelPack>(self->annotatedBeatmapLevelCollection);
    if(pack.has_value()) {
        auto playlist = GetPlaylistWithPrefix(pack.value()->get_packID());
        if(playlist)
            self->SetDownloadIconVisible(playlistConfig.DownloadIcon && PlaylistHasMissingSongs(playlist));
    }
}

// override header cell behavior and change no data prefab
MAKE_HOOK_MATCH(LevelCollectionViewController_SetData, &LevelCollectionViewController::SetData,
        void, LevelCollectionViewController* self, IBeatmapLevelCollection* beatmapLevelCollection, StringW headerText, UnityEngine::Sprite* headerSprite, bool sortLevels, UnityEngine::GameObject* noDataInfoPrefab) {
    
    // only check for null strings, not empty
    // string will be null for level collections but not level packs
    self->showHeader = (bool) headerText;
    // copy base game method implementation
    self->levelCollectionTableView->Init(headerText, headerSprite);
    self->levelCollectionTableView->showLevelPackHeader = self->showHeader;
    if(self->noDataInfoGO) {
        UnityEngine::Object::Destroy(self->noDataInfoGO);
        self->noDataInfoGO = nullptr;
    }
    // also override check for empty collection
    if(beatmapLevelCollection) {
        self->levelCollectionTableView->get_gameObject()->SetActive(true);
        self->levelCollectionTableView->SetData(beatmapLevelCollection->get_beatmapLevels(), self->playerDataModel->playerData->favoritesLevelIds, sortLevels);
        self->levelCollectionTableView->RefreshLevelsAvailability();
    } else {
        if(noDataInfoPrefab)
            self->noDataInfoGO = self->container->InstantiatePrefab(noDataInfoPrefab, self->noDataInfoContainer);
        // change no custom songs text if playlists exist
        // because if they do then the only way to get here with that specific no data indicator is to have no playlists filtered
        static ConstString message("No playlists are contained in the filtering options selected.");
        if(GetLoadedPlaylists().size() > 0 && noDataInfoPrefab == FindComponent<LevelFilteringNavigationController*>()->emptyCustomSongListInfoPrefab)
            self->noDataInfoGO->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_text(message);
        self->levelCollectionTableView->get_gameObject()->SetActive(false);
    }
    if(self->get_isInViewControllerHierarchy()) {
        if(self->showHeader)
            self->levelCollectionTableView->SelectLevelPackHeaderCell();
        else
            self->levelCollectionTableView->ClearSelection();
        self->songPreviewPlayer->CrossfadeToDefault();
    }
}

// make playlist selector only 5 playlists wide and add scrolling
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridView_OnEnable, &AnnotatedBeatmapLevelCollectionsGridView::OnEnable,
        void, AnnotatedBeatmapLevelCollectionsGridView* self) {

    if(playlistConfig.Management) {
        self->GetComponent<UnityEngine::RectTransform*>()->set_anchorMax({0.83, 1});
        self->pageControl->content->get_gameObject()->SetActive(false);
        auto content = self->animator->contentTransform;
        content->set_anchoredPosition({0, content->get_anchoredPosition().y});
    } else {
        self->GetComponent<UnityEngine::RectTransform*>()->set_anchorMax({1, 1});
        self->pageControl->content->get_gameObject()->SetActive(true);
        auto content = self->animator->contentTransform;
        content->set_anchoredPosition({1.5, content->get_anchoredPosition().y});
    }
    
    AnnotatedBeatmapLevelCollectionsGridView_OnEnable(self);

    if(!playlistConfig.Management)
        return;

    if(!self->GetComponent<Scroller*>())
        self->get_gameObject()->AddComponent<Scroller*>()->Init(self->animator->contentTransform);
}

// make the playlist opening animation work better with the playlist scroller
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen, &AnnotatedBeatmapLevelCollectionsGridViewAnimator::AnimateOpen,
        void, AnnotatedBeatmapLevelCollectionsGridViewAnimator* self, bool animated) {
    
    int rowCount = self->rowCount;
    int selectedRow = self->selectedRow;
    
    if(playlistConfig.Management) {
        // lock height to specific value
        self->rowCount = 5;
        self->selectedRow = 0;
    }

    AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen(self, animated);
    
    if(playlistConfig.Management) {
        // prevent modification of content transform as it overrides the scroll view
        Tweening::Vector2Tween::_get_Pool()->Despawn(self->contentPositionTween);
        self->contentPositionTween = nullptr;
    }
    
    self->rowCount = rowCount;
    self->selectedRow = selectedRow;
}

// ensure animator doesn't get stuck at the wrong position
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridViewAnimator_ScrollToRowIdxInstant, &AnnotatedBeatmapLevelCollectionsGridViewAnimator::ScrollToRowIdxInstant,
        void, AnnotatedBeatmapLevelCollectionsGridViewAnimator* self, int selectedRow) {

    // despawns tweens and force sets the viewport and anchored pos
    self->AnimateClose(selectedRow, false);

    AnnotatedBeatmapLevelCollectionsGridViewAnimator_ScrollToRowIdxInstant(self, selectedRow);
}

// when to set up the add playlist button
MAKE_HOOK_MATCH(LevelFilteringNavigationController_UpdateSecondChildControllerContent, &LevelFilteringNavigationController::UpdateSecondChildControllerContent,
        void, LevelFilteringNavigationController* self, SelectLevelCategoryViewController::LevelCategory levelCategory) {
    
    LevelFilteringNavigationController_UpdateSecondChildControllerContent(self, levelCategory);

    if(!playlistConfig.Management)
        return;
    
    if(!GridViewAddon::addonInstance) {
        GridViewAddon::addonInstance = new GridViewAddon();
        GridViewAddon::addonInstance->Init(self->annotatedBeatmapLevelCollectionsViewController);
    }
    GridViewAddon::addonInstance->SetVisible(levelCategory == SelectLevelCategoryViewController::LevelCategory::CustomSongs);
}

// when to show the playlist filters
MAKE_HOOK_MATCH(LevelFilteringNavigationController_DidActivate, &LevelFilteringNavigationController::DidActivate,
        void, LevelFilteringNavigationController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    
    LevelFilteringNavigationController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if(!playlistConfig.Management)
        return;

    if(!PlaylistFilters::filtersInstance) {
        PlaylistFilters::filtersInstance = new PlaylistFilters();
        PlaylistFilters::filtersInstance->Init();
    } else {
        PlaylistFilters::filtersInstance->SetVisible(true);
        PlaylistFilters::filtersInstance->UpdateTransform();
    }
}

// when to hide the playlist filters
MAKE_HOOK_MATCH(LevelFilteringNavigationController_DidDeactivate, &LevelFilteringNavigationController::DidDeactivate,
        void, LevelFilteringNavigationController* self, bool removedFromHierarchy, bool screenSystemDisabling) {
    
    LevelFilteringNavigationController_DidDeactivate(self, removedFromHierarchy, screenSystemDisabling);

    if(PlaylistFilters::filtersInstance)
        PlaylistFilters::filtersInstance->SetVisible(false);
}

// when to show the playlist menu
MAKE_HOOK_MATCH(LevelPackDetailViewController_ShowContent, &LevelPackDetailViewController::ShowContent,
        void, LevelPackDetailViewController* self, LevelPackDetailViewController::ContentType contentType, StringW errorText) {
    
    LevelPackDetailViewController_ShowContent(self, contentType, errorText);

    if(!playlistConfig.Management)
        return;

    static ConstString customPackName(CustomLevelPackPrefixID);

    if(!PlaylistMenu::menuInstance) {
        PlaylistMenu::menuInstance = self->get_gameObject()->AddComponent<PlaylistMenu*>();
        PlaylistMenu::menuInstance->Init(self->packImage);
    }

    if(contentType == LevelPackDetailViewController::ContentType::Owned && self->pack->get_packID()->Contains(customPackName)) {
        // find playlist json
        auto playlist = GetPlaylistWithPrefix(self->pack->get_packID());
        if(playlist) {
            PlaylistMenu::menuInstance->SetPlaylist(playlist);
            PlaylistMenu::menuInstance->SetVisible(true);
        } else
            PlaylistMenu::menuInstance->SetVisible(false, true);
    } else
        PlaylistMenu::menuInstance->SetVisible(false);

    // disable level buttons (hides modal if necessary)
    if(ButtonsContainer::buttonsInstance)
        ButtonsContainer::buttonsInstance->SetVisible(false, false, false);
}

// when to show the level buttons
MAKE_HOOK_MATCH(StandardLevelDetailViewController_ShowContent, &StandardLevelDetailViewController::ShowContent, 
        void, StandardLevelDetailViewController* self, StandardLevelDetailViewController::ContentType contentType, StringW errorText, float downloadingProgress, StringW downloadingText) {

    StandardLevelDetailViewController_ShowContent(self, contentType, errorText, downloadingProgress, downloadingText);

    if(!playlistConfig.Management || contentType != StandardLevelDetailViewController::ContentType::OwnedAndReady)
        return;
    
    if(!ButtonsContainer::buttonsInstance) {
        ButtonsContainer::buttonsInstance = new ButtonsContainer();
        ButtonsContainer::buttonsInstance->Init(self->standardLevelDetailView);
    }
    // note: pack is simply the first level pack it finds that contains the level, if selected from all songs etc.
    std::string id = self->pack ? self->pack->get_packID() : "";
    bool customPack = GetPlaylistWithPrefix(id) != nullptr;
    bool customSong = customPack || id == CustomLevelPackPrefixID "CustomLevels" || id == CustomLevelPackPrefixID "CustomWIPLevels";
    bool wip = IsWipLevel(self->previewBeatmapLevel);
    ButtonsContainer::buttonsInstance->SetVisible(customSong, customPack, wip);
    ButtonsContainer::buttonsInstance->SetLevel(self->previewBeatmapLevel);
    ButtonsContainer::buttonsInstance->SetPlaylist(GetPlaylistWithPrefix(id));
    ButtonsContainer::buttonsInstance->RefreshHighlightedDifficulties();
}

// hook to apply changes when deselecting a cell in a multi select
MAKE_HOOK_MATCH(TableView_HandleCellSelectionDidChange, &HMUI::TableView::HandleCellSelectionDidChange,
        void, HMUI::TableView* self, HMUI::SelectableCell* selectableCell, HMUI::SelectableCell::TransitionType transitionType, ::Il2CppObject* changeOwner) {
    
    if(self == PlaylistFilters::monitoredTable) {
        int cellIdx = ((HMUI::TableCell*) selectableCell)->get_idx();
        bool wasSelected = self->selectedCellIdxs->Contains(cellIdx);

        TableView_HandleCellSelectionDidChange(self, selectableCell, transitionType, changeOwner);

        if(!selectableCell->get_selected() && wasSelected)
            PlaylistFilters::deselectCallback(cellIdx);
    } else
        TableView_HandleCellSelectionDidChange(self, selectableCell, transitionType, changeOwner);
}

// throw away objects on a soft restart
MAKE_HOOK_MATCH(MenuTransitionsHelper_RestartGame, &MenuTransitionsHelper::RestartGame,
        void, MenuTransitionsHelper* self, System::Action_1<Zenject::DiContainer*>* finishCallback) {
    
    SettingsViewController::DestroyUI();
    
    ResettableStaticPtr::resetAll();

    ClearLoadedImages();

    hasLoaded = false;
    filterSelectionState = 0;

    MenuTransitionsHelper_RestartGame(self, finishCallback);
}

// override the main menu button to reload playlists
MAKE_HOOK_FIND_CLASS_INSTANCE(MainMenuModSettingsViewController_DidActivate, "QuestUI", "MainMenuModSettingsViewController", "DidActivate",
        void, HMUI::ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    
    MainMenuModSettingsViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if(!firstActivation)
        return;

    for(auto& button : self->GetComponentsInChildren<UnityEngine::UI::Button*>()) {
        auto text = button->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
        if(text->get_text() == "Reload Playlists") {
            // auto eventListener = button->get_onClick();
            // not sure why RemoveAllListeners is stripped, but this is what it does
            // actually the m_RuntimeCalls list has a problem with its generic so I guess we just make a new event
            // eventListener->m_Calls->m_RuntimeCalls->Clear();
            // eventListener->m_Calls->m_NeedsUpdate = true;
            button->set_onClick(UnityEngine::UI::Button::ButtonClickedEvent::New_ctor());
            button->get_onClick()->AddListener(il2cpp_utils::MakeDelegate<UnityEngine::Events::UnityAction*>((std::function<void()>) [] {
                ReloadPlaylists(true);
            }));
        }
    }
}

// add our playlist selection view controller to the song downloader menu
MAKE_HOOK_FIND_CLASS_INSTANCE(DownloadSongsFlowCoordinator_DidActivate, "SongDownloader", "DownloadSongsFlowCoordinator", "DidActivate",
        void, HMUI::FlowCoordinator* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    
    DownloadSongsFlowCoordinator_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if(firstActivation) {
        STATIC_AUTO(AddonViewController, SongDownloaderAddon::Create());
        self->providedRightScreenViewController = AddonViewController;
    }
}

// add a playlist callback to the song downloader buttons
MAKE_HOOK_FIND_CLASS_INSTANCE(DownloadSongsSearchViewController_DidActivate, "SongDownloader", "DownloadSongsSearchViewController", "DidActivate",
        void, HMUI::ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    
    DownloadSongsSearchViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if(!firstActivation)
        return;

    using namespace UnityEngine;
    
    // add listeners to search entry buttons
    auto entryList = self->GetComponentsInChildren<UI::VerticalLayoutGroup*>().First([](UI::VerticalLayoutGroup* x) {
        return x->get_name() == "QuestUIScrollViewContentContainer";
    })->get_transform();

    // offset by one because the prefab doesn't seem to be destroyed yet here
    int entryCount = entryList->GetChildCount() - 1;
    for(int i = 0; i < entryCount; i++) {
        // undo offset to skip first element
        auto downloadButton = entryList->GetChild(i + 1)->GetComponentInChildren<UI::Button*>();
        // get entry at index with some lovely pointer addition
        SearchEntryHack* entryArrStart = (SearchEntryHack*) (((char*) self) + sizeof(HMUI::ViewController));
        // capture button array start and index
        downloadButton->get_onClick()->AddListener(il2cpp_utils::MakeDelegate<Events::UnityAction*>((std::function<void()>) [entryArrStart, i] {
            auto& entry = *(entryArrStart + i);
            // get hash from entry
            std::string hash;
            if(entry.MapType == SearchEntryHack::MapType::BeatSaver)
                hash = entry.map.GetVersions().front().GetHash();
            else if(entry.MapType == SearchEntryHack::MapType::BeastSaber)
                hash = entry.BSsong.GetHash();
            else if(entry.MapType == SearchEntryHack::MapType::ScoreSaber)
                hash = entry.SSsong.GetId();
            // get json object from playlist
            auto playlist = SongDownloaderAddon::SelectedPlaylist;
            if(!playlist)
                return;
            auto& json = playlist->playlistJSON;
            // add a blank song
            json.Songs.emplace_back(BPSong());
            // set info
            auto& songJson = *(json.Songs.end() - 1);
            songJson.Hash = hash;
            // write to file
            WriteToFile(playlist->path, json);
            // have changes be updated
            MarkPlaylistForReload(playlist);
        }));
    }
}

// override to prevent crashes due to opening with a null level pack
#define COMBINE(delegate1, selfMethodName, ...) delegate1 = (__VA_ARGS__) System::Delegate::Combine(delegate1, System::Delegate::CreateDelegate(csTypeOf(__VA_ARGS__), self, #selfMethodName));
MAKE_HOOK_MATCH(LevelCollectionNavigationController_DidActivate, &LevelCollectionNavigationController::DidActivate,
        void, LevelCollectionNavigationController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    if(addedToHierarchy) {
        COMBINE(self->levelCollectionViewController->didSelectLevelEvent, HandleLevelCollectionViewControllerDidSelectLevel, System::Action_2<LevelCollectionViewController*, IPreviewBeatmapLevel*>*);
        COMBINE(self->levelCollectionViewController->didSelectHeaderEvent, HandleLevelCollectionViewControllerDidSelectPack, System::Action_1<LevelCollectionViewController*>*);
        COMBINE(self->levelDetailViewController->didPressActionButtonEvent, HandleLevelDetailViewControllerDidPressActionButton, System::Action_1<StandardLevelDetailViewController*>*);
        COMBINE(self->levelDetailViewController->didPressPracticeButtonEvent, HandleLevelDetailViewControllerDidPressPracticeButton, System::Action_2<StandardLevelDetailViewController*, IBeatmapLevel*>*);
        COMBINE(self->levelDetailViewController->didChangeDifficultyBeatmapEvent, HandleLevelDetailViewControllerDidChangeDifficultyBeatmap, System::Action_2<StandardLevelDetailViewController*, IDifficultyBeatmap*>*);
        COMBINE(self->levelDetailViewController->didChangeContentEvent, HandleLevelDetailViewControllerDidPresentContent, System::Action_2<StandardLevelDetailViewController*, StandardLevelDetailViewController::ContentType>*);
        COMBINE(self->levelDetailViewController->didPressOpenLevelPackButtonEvent, HandleLevelDetailViewControllerDidPressOpenLevelPackButton, System::Action_2<StandardLevelDetailViewController*, IBeatmapLevelPack*>*);
        COMBINE(self->levelDetailViewController->levelFavoriteStatusDidChangeEvent, HandleLevelDetailViewControllerLevelFavoriteStatusDidChange, System::Action_2<StandardLevelDetailViewController*, bool>*);
        if (self->beatmapLevelToBeSelectedAfterPresent) {
            self->levelCollectionViewController->SelectLevel(self->beatmapLevelToBeSelectedAfterPresent);
            self->PresentViewControllersForLevelCollection();
            self->beatmapLevelToBeSelectedAfterPresent = nullptr;
        }
        else {
            // override here so that the pack detail controller will not be shown if no pack is selected
            if(self->levelPack)
                self->PresentViewControllersForPack();
            else
                self->PresentViewControllersForLevelCollection();
        }
    } else if(self->loading) {
        self->ClearChildViewControllers();
    }
    else if(self->hideDetailViewController) {
        self->PresentViewControllersForLevelCollection();
        self->hideDetailViewController = false;
    }
}

extern "C" void setup(ModInfo& info) {
    modInfo.id = "PlaylistManager";
    modInfo.version = VERSION;
    info = modInfo;
    
    auto playlistsPath = GetPlaylistsPath();
    if(!direxists(playlistsPath))
        mkpath(playlistsPath);
    
    auto coversPath = GetCoversPath();
    if(!direxists(coversPath))
        mkpath(coversPath);
    
    auto configPath = GetConfigPath();
    if(fileexists(configPath)) {
        try {
            ReadFromFile(configPath, playlistConfig);
        } catch (const std::exception& err) {
            LOG_ERROR("Error reading playlist config: %s", err.what());
            WriteToFile(configPath, playlistConfig);
        }
    } else
        WriteToFile(configPath, playlistConfig);
}

extern "C" void load() {
    LOG_INFO("Starting PlaylistManager installation...");
    il2cpp_functions::Init();
    QuestUI::Init();
    QuestUI::Register::RegisterModSettingsViewController<SettingsViewController*>(modInfo, "Playlist Manager");
    // create fake modInfo for reload playlists button
    ModInfo fakeModInfo;
    fakeModInfo.id = "Reload Playlists";
    QuestUI::Register::RegisterMainMenuModSettingsViewController(fakeModInfo);
    // get if custom songs are available in multiplayer
    // requireMod also forces its load function to be called, which is unnecessary, but appears to be the only simple way to check for a mod's existence
    allowInMultiplayer = Modloader::requireMod("MultiplayerCore");

    INSTALL_HOOK_ORIG(getLogger(), TableView_GetVisibleCellsIdRange);
    INSTALL_HOOK(getLogger(), InputFieldView_DeactivateKeyboard);
    INSTALL_HOOK(getLogger(), InputFieldView_Awake);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync);
    INSTALL_HOOK_ORIG(getLogger(), LevelCollectionViewController_SetData);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionsGridView_OnEnable);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionsGridViewAnimator_ScrollToRowIdxInstant);
    INSTALL_HOOK(getLogger(), LevelFilteringNavigationController_UpdateSecondChildControllerContent);
    INSTALL_HOOK(getLogger(), LevelFilteringNavigationController_DidActivate);
    INSTALL_HOOK(getLogger(), LevelFilteringNavigationController_DidDeactivate);
    INSTALL_HOOK(getLogger(), LevelPackDetailViewController_ShowContent);
    INSTALL_HOOK(getLogger(), StandardLevelDetailViewController_ShowContent);
    INSTALL_HOOK(getLogger(), TableView_HandleCellSelectionDidChange);
    INSTALL_HOOK(getLogger(), MenuTransitionsHelper_RestartGame);
    INSTALL_HOOK(getLogger(), MainMenuModSettingsViewController_DidActivate);
    INSTALL_HOOK(getLogger(), DownloadSongsFlowCoordinator_DidActivate);
    INSTALL_HOOK(getLogger(), DownloadSongsSearchViewController_DidActivate);
    INSTALL_HOOK_ORIG(getLogger(), LevelCollectionNavigationController_DidActivate);
    
    RuntimeSongLoader::API::AddRefreshLevelPacksEvent(
        [] (RuntimeSongLoader::SongLoaderBeatmapLevelPackCollectionSO* customBeatmapLevelPackCollectionSO) {
            LoadPlaylists(customBeatmapLevelPackCollectionSO);
        }
    );
    RuntimeSongLoader::API::AddSongDeletedEvent([] {
        for(auto& playlist : GetLoadedPlaylists())
            MarkPlaylistForReload(playlist);
    });
    
    AddPlaylistFilter(modInfo, [](std::string const& path) -> bool {
        if(path == "Defaults") {
            bool showDefaults = filterSelectionState != 2;
            if(filterSelectionState == 3 && currentFolder && !currentFolder->HasSubfolders)
                showDefaults = currentFolder->ShowDefaults;
            return showDefaults;
        }
        if(filterSelectionState == 3 && currentFolder && !currentFolder->HasSubfolders) {
            for(std::string& testPath : currentFolder->Playlists) {
                if(path == testPath)
                    return true;
            }
            return false;
        }
        return filterSelectionState != 1;
    });
    LOG_INFO("Successfully installed PlaylistManager!");
}
