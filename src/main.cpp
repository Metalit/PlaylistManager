#include "main.hpp"

#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionCell.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSegmentedControlController.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"
#include "GlobalNamespace/IEntitlementModel.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/LevelPackDetailViewController.hpp"
#include "GlobalNamespace/LevelSelectionNavigationController.hpp"
#include "GlobalNamespace/SelectLevelCategoryViewController.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/StandardLevelDetailViewController.hpp"
#include "HMUI/TableView.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "UnityEngine/EventSystems/ExecuteEvents.hpp"
#include "VRUIControls/MouseButtonEventData.hpp"
#include "VRUIControls/VRInputModule.hpp"
#include "assets.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/BSMLDataCache.hpp"
#include "custom-types/shared/register.hpp"
#include "customtypes/allsongs.hpp"
#include "customtypes/mainmenu.hpp"
#include "customtypes/playlistgrid.hpp"
#include "customtypes/playlistinfo.hpp"
#include "customtypes/playlistsongs.hpp"
#include "manager.hpp"
#include "playlistcore/shared/PlaylistCore.hpp"
#include "scotland2/shared/modloader.h"
#include "shortcuts.hpp"
#include "songcore/shared/SongCore.hpp"
#include "utils.hpp"

using namespace GlobalNamespace;
using namespace PlaylistManager;

static modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

// prevent download icon showing up on empty custom playlists unless configured to
MAKE_HOOK_MATCH(
    AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync,
    &AnnotatedBeatmapLevelCollectionCell::RefreshAvailabilityAsync,
    void,
    AnnotatedBeatmapLevelCollectionCell* self,
    IEntitlementModel* entitlementModel
) {
    AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync(self, entitlementModel);

    if (PlaylistCore::GetPlaylistWithPrefix(self->_beatmapLevelPack->packID))
        self->SetDownloadIconVisible(false);
}

// show shortcuts on a selected level
MAKE_HOOK_MATCH(
    StandardLevelDetailViewController_ShowOwnedContent,
    &StandardLevelDetailViewController::ShowOwnedContent,
    void,
    StandardLevelDetailViewController* self
) {
    StandardLevelDetailViewController_ShowOwnedContent(self);

    Shortcuts::RefreshLevelShortcuts(self);
}

// show shortcuts on a selected playlist
MAKE_HOOK_MATCH(
    LevelPackDetailViewController_ShowContent,
    &LevelPackDetailViewController::ShowContent,
    void,
    LevelPackDetailViewController* self,
    LevelPackDetailViewController::ContentType contentType,
    StringW errorText
) {
    LevelPackDetailViewController_ShowContent(self, contentType, errorText);

    Shortcuts::RefreshPackShortcuts(self);
}

// show shortcut next to playlist selector
MAKE_HOOK_MATCH(
    LevelFilteringNavigationController_ShowPacksInSecondChildController,
    &LevelFilteringNavigationController::ShowPacksInSecondChildController,
    void,
    LevelFilteringNavigationController* self,
    System::Collections::Generic::IReadOnlyList_1<BeatmapLevelPack*>* beatmapLevelPacks
) {
    LevelFilteringNavigationController_ShowPacksInSecondChildController(self, beatmapLevelPacks);

    Shortcuts::RefreshCreateShortcuts(self);
}

// highlight level difficulties
MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {

    StandardLevelDetailView_RefreshContent(self);

    // songcore hooks and replaces the cells for custom labels so this is the easiest solution I can think of
    BSML::MainThreadScheduler::ScheduleNextFrame([self]() {
        ListW<UnityW<HMUI::SegmentedControlCell>> cells = self->_beatmapDifficultySegmentedControlController->_difficultySegmentedControl->_cells;
        // reset all text colors
        for (auto& cell : cells) {
            if (auto text = cell->GetComponentInChildren<TMPro::TextMeshProUGUI*>())
                text->faceColor = UnityEngine::Color32::op_Implicit___UnityEngine__Color32({1, 1, 1, 1});
        }

        auto selection = self->GetComponentInParent<LevelSelectionNavigationController*>();
        if (!selection || !selection->_levelFilteringNavigationController)
            return;
        if (selection->_levelFilteringNavigationController->selectedLevelCategory != SelectLevelCategoryViewController::LevelCategory::CustomSongs)
            return;

        auto pack = selection->selectedBeatmapLevelPack;
        if (!pack)
            return;
        auto playlist = PlaylistCore::GetPlaylistWithPrefix(pack->packID);
        if (!playlist)
            return;
        auto song = Utils::GetLevelJson(playlist, selection->beatmapLevel);
        if (!song)
            return;
        auto diffs = Utils::GetHighlightedDifficulties(*song, self->_beatmapCharacteristicSegmentedControlController->selectedBeatmapCharacteristic);

        logger.debug("highlighting difficulties {} for level {} in playlist {}", diffs, song->SongName.value_or(song->LevelID), playlist->name);

        for (int diff : diffs) {
            auto cell = cells[self->_beatmapDifficultySegmentedControlController->GetClosestDifficultyIndex(diff)];
            if (auto text = cell->GetComponentInChildren<TMPro::TextMeshProUGUI*>())
                text->faceColor = UnityEngine::Color32::op_Implicit___UnityEngine__Color32({1, 1, 0, 1});
        }
    });
}

extern "C" void setup(CModInfo* info) {
    info->id = MOD_ID;
    info->version = VERSION;
    modInfo.assign(*info);

    Paper::Logger::RegisterFileContextId(MOD_ID);

    logger.info("Completed setup!");
}

extern "C" void late_load() {
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();

    logger.info("Installing hooks...");
    INSTALL_HOOK(logger, AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync);
    INSTALL_HOOK(logger, StandardLevelDetailViewController_ShowOwnedContent);
    INSTALL_HOOK(logger, LevelPackDetailViewController_ShowContent);
    INSTALL_HOOK(logger, LevelFilteringNavigationController_ShowPacksInSecondChildController);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    logger.info("Installed all hooks!");

    BSML::Register::RegisterMenuButton("Manage Playlists", "Open the PlaylistManager menu", Manager::PresentMenu);
    SongCore::API::Loading::GetCustomLevelPacksRefreshedEvent().addCallback([](auto) {
        if (!MainMenu::InstanceCreated())
            return;
        PlaylistGrid::GetInstance()->Refresh();
        PlaylistInfo::GetInstance()->Refresh();
        PlaylistSongs::GetInstance()->Refresh();
    });
    SongCore::API::Loading::GetSongsLoadedEvent().addCallback([](auto) {
        if (MainMenu::InstanceCreated())
            AllSongs::GetInstance()->Refresh();
    });
}

#define BSML_IMAGE(name) \
BSML_DATACACHE(name##_png) { \
    return IncludedAssets::icons::name##_png; \
}

BSML_IMAGE(clear_download);
BSML_IMAGE(clear_highlight);
BSML_IMAGE(clear_sync);
BSML_IMAGE(delete);
BSML_IMAGE(download);
BSML_IMAGE(link);
BSML_IMAGE(options);
BSML_IMAGE(reset);
BSML_IMAGE(save_edit);
BSML_IMAGE(save);
BSML_IMAGE(sync);
BSML_IMAGE(unlink);
