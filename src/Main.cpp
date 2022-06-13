#include "Main.hpp"
#include "Types/PlaylistMenu.hpp"
#include "Types/PlaylistFilters.hpp"
#include "Types/LevelButtons.hpp"
#include "Types/GridViewAddon.hpp"
#include "Types/Config.hpp"
#include "Settings.hpp"

#include <chrono>

#include "playlistcore/shared/PlaylistCore.hpp"
#include "playlistcore/shared/ResettableStaticPtr.hpp"
#include "playlistcore/shared/Utils.hpp"

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
#include "HMUI/TableCell.hpp"
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
using namespace PlaylistCore;
using namespace PlaylistCore::Utils;

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

std::string GetConfigPath() {
    static std::string configPath = Configuration::getConfigFilePath(modInfo);
    return configPath;
}

std::string GetCoversPath() {
    static std::string coversPath(getDataDir(modInfo) + "Covers");
    return coversPath;
}

void SaveConfig() {
    if(!WriteToFile(GetConfigPath(), playlistConfig))
        LOG_ERROR("Error saving config!");
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
    }
    PlaylistFilters::filtersInstance->SetVisible(true);
    PlaylistFilters::filtersInstance->UpdateTransform();
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

extern "C" void setup(ModInfo& info) {
    modInfo.id = "PlaylistManager";
    modInfo.version = VERSION;
    info = modInfo;
    
    auto coversPath = GetCoversPath();
    if(!direxists(coversPath))
        mkpath(coversPath);
    
    auto configPath = GetConfigPath();
    if(fileexists(configPath)) {
        try {
            ReadFromFile(configPath, playlistConfig);
        } catch (const std::exception& err) {
            LOG_ERROR("Error reading playlist config: %s", err.what());
        }
    }
    SaveConfig();
}

// throw away objects on a soft restart
MAKE_HOOK_MATCH(MenuTransitionsHelper_RestartGame, &MenuTransitionsHelper::RestartGame,
        void, MenuTransitionsHelper* self, System::Action_1<Zenject::DiContainer*>* finishCallback) {

    ResettableStaticPtr::resetAll();

    SettingsViewController::DestroyUI();

    filterSelectionState = 0;
    
    MenuTransitionsHelper_RestartGame(self, finishCallback);
}

extern "C" void load() {
    LOG_INFO("Starting PlaylistManager installation...");
    il2cpp_functions::Init();
    QuestUI::Init();
    QuestUI::Register::RegisterModSettingsViewController<SettingsViewController*>(modInfo, "Playlist Manager");
    // get if custom songs are available in multiplayer
    // requireMod also forces its load function to be called, which is unnecessary, but appears to be the only simple way to check for a mod's existence
    allowInMultiplayer = Modloader::requireMod("MultiplayerCore");

    INSTALL_HOOK(getLogger(), InputFieldView_DeactivateKeyboard);
    INSTALL_HOOK(getLogger(), InputFieldView_Awake);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync);
    INSTALL_HOOK(getLogger(), LevelFilteringNavigationController_UpdateSecondChildControllerContent);
    INSTALL_HOOK(getLogger(), LevelFilteringNavigationController_DidActivate);
    INSTALL_HOOK(getLogger(), LevelFilteringNavigationController_DidDeactivate);
    INSTALL_HOOK(getLogger(), LevelPackDetailViewController_ShowContent);
    INSTALL_HOOK(getLogger(), StandardLevelDetailViewController_ShowContent);
    INSTALL_HOOK(getLogger(), TableView_HandleCellSelectionDidChange);
    INSTALL_HOOK(getLogger(), MenuTransitionsHelper_RestartGame);
    
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
