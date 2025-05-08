#include "manager.hpp"

#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/LevelSelectionNavigationController.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "customtypes/allsongs.hpp"
#include "customtypes/mainmenu.hpp"
#include "customtypes/playlistgrid.hpp"
#include "customtypes/playlistinfo.hpp"
#include "customtypes/playlistsongs.hpp"
#include "main.hpp"
#include "metacore/shared/songs.hpp"
#include "playlistcore/shared/PlaylistCore.hpp"
#include "shortcuts.hpp"
#include "songcore/shared/SongCore.hpp"
#include "utils.hpp"
#include "web-utils/shared/WebUtils.hpp"

using namespace PlaylistManager;

namespace Manager {
    std::optional<PlaylistCore::Playlist> addingPlaylist = std::nullopt;
    PlaylistCore::Playlist* selectedPlaylist = nullptr;
    GlobalNamespace::BeatmapLevel* selectedLevel = nullptr;
    bool shouldReload = false;
    std::string packIdFromShortcut = "";

    PlaylistCore::Playlist* processingPlaylist = nullptr;
    bool syncing = false;
    bool downloading = false;

    GlobalNamespace::BeatmapLevel* addingLevel = nullptr;

    GlobalNamespace::LevelSelectionNavigationController* selectionController;

    GlobalNamespace::LevelSelectionNavigationController* GetSelectionNavigationController() {
        if (!selectionController)
            selectionController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::LevelSelectionNavigationController*>(true);
        return selectionController;
    }

    void PresentMenu() {
        selectedLevel = MetaCore::Songs::GetSelectedLevel(false);
        BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf()->PresentFlowCoordinator(
            MainMenu::GetInstance(), nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false, false
        );
    }

    void Invalidate() {
        addingPlaylist = std::nullopt;
        selectedPlaylist = nullptr;
        selectedLevel = nullptr;
        shouldReload = false;
        packIdFromShortcut = "";
        processingPlaylist = nullptr;
        syncing = false;
        downloading = false;
        selectionController = nullptr;
        Shortcuts::Invalidate();
    }

    void PresentAddShortcut(GlobalNamespace::BeatmapLevel* level) {
        if (auto nav = GetSelectionNavigationController())
            packIdFromShortcut = (std::string) nav->selectedBeatmapLevelPack->packID;
        MainMenu::GetInstance()->presentDestination = 0;
        addingLevel = level;
        PresentMenu();
    }

    void PresentCreateShortcut() {
        if (auto nav = GetSelectionNavigationController())
            packIdFromShortcut = (std::string) nav->selectedBeatmapLevelPack->packID;
        ResetAddition();
        MainMenu::GetInstance()->presentDestination = 1;
        PresentMenu();
    }

    void PresentEditShortcut(PlaylistCore::Playlist* playlist) {
        if (!playlist)
            return;
        if (auto nav = GetSelectionNavigationController())
            packIdFromShortcut = (std::string) nav->selectedBeatmapLevelPack->packID;
        selectedPlaylist = playlist;
        MainMenu::GetInstance()->presentDestination = 2;
        PresentMenu();
    }

    void PresentEditShortcut(GlobalNamespace::BeatmapLevelPack* pack) {
        PresentEditShortcut(PlaylistCore::GetPlaylistWithPrefix(pack->packID));
    }

    bool InAddShortcut() {
        return addingLevel != nullptr;
    }

    void OnDetailExit() {
        selectedPlaylist = nullptr;
    }

    void OnMenuExit() {
        if (shouldReload)
            Reload();
        // fix weird view controller layout issue and update playlists if necessary
        auto nav = GetSelectionNavigationController();
        if (nav && nav->_levelFilteringNavigationController->isInViewControllerHierarchy) {
            auto filter = nav->_levelFilteringNavigationController;
            filter->LayoutViewControllers(filter->viewControllers);
            if (!packIdFromShortcut.empty())
                filter->_levelPackIdToBeSelectedAfterPresent = packIdFromShortcut;
            filter->UpdateCustomSongs();
            // check that the task is completed instantly (it should be)
            if (!filter->_cancellationTokenSource && selectedLevel)
                nav->_levelCollectionNavigationController->SelectLevel(selectedLevel);
        }
        selectedPlaylist = nullptr;
        selectedLevel = nullptr;
        addingPlaylist = std::nullopt;
        addingLevel = nullptr;
        packIdFromShortcut = "";
    }

    bool IsCreatingPlaylist() {
        return addingPlaylist.has_value();
    }

    PlaylistCore::Playlist* GetSyncing() {
        if (syncing)
            return processingPlaylist;
        return nullptr;
    }

    PlaylistCore::Playlist* GetDownloading() {
        if (downloading)
            return processingPlaylist;
        return nullptr;
    }

    PlaylistCore::Playlist* GetSelectedPlaylist() {
        return selectedPlaylist;
    }

    void SelectPlaylist(PlaylistCore::Playlist* playlist) {
        logger.info("playlist {} ({}) selected", playlist->name, fmt::ptr(playlist));
        if (addingLevel) {
            PlaylistCore::AddSongToPlaylist(playlist, addingLevel);
            Shortcuts::RefreshPlaylistList();
            addingLevel = nullptr;
            MainMenu::GetInstance()->HandleScreenSystemBackButtonWasPressed();
            return;
        }
        // todo: confirm cancel (or do something else idk)
        addingPlaylist.reset();
        selectedPlaylist = playlist;
        MainMenu::GetInstance()->ShowDetail();
    }

    void BeginAddition() {
        if (IsCreatingPlaylist())
            return;
        ResetAddition();
    }

    void ResetAddition() {
        addingPlaylist.emplace();
        auto& json = addingPlaylist->playlistJSON;
        json.PlaylistTitle = "New Playlist";
        json.PlaylistAuthor = "Playlist Manager";
        selectedPlaylist = &addingPlaylist.value();
        PlaylistInfo::GetInstance()->Refresh();
        MainMenu::GetInstance()->ShowCreation();
    }

    void ConfirmAddition(bool select) {
        logger.info("adding new playlist {}", addingPlaylist->playlistJSON.PlaylistTitle);
        shouldReload = false;
        auto [_, added] = PlaylistCore::AddPlaylist(addingPlaylist->playlistJSON, true);
        if (addingPlaylist->imageIndex != -1)
            PlaylistCore::ChangePlaylistCover(added, addingPlaylist->imageIndex);
        addingPlaylist.reset();
        selectedPlaylist = nullptr;
        PlaylistGrid::GetInstance()->Refresh();
        MainMenu::GetInstance()->ShowGrid();
        if (select)
            SelectPlaylist(added);
    }

    void CancelAddition() {
        addingPlaylist.reset();
        selectedPlaylist = nullptr;
        MainMenu::GetInstance()->ShowGrid();
    }

    void SetShouldReload(PlaylistCore::Playlist* fullReloadPlaylist) {
        shouldReload = true;
        if (fullReloadPlaylist)
            PlaylistCore::MarkPlaylistForReload(fullReloadPlaylist);
    }

    void Reload() {
        shouldReload = false;
        PlaylistCore::ReloadPlaylists();
    }

    void SetPlaylistName(std::string value) {
        if (!selectedPlaylist)
            return;
        selectedPlaylist->playlistJSON.PlaylistTitle = value;
        if (!addingPlaylist)
            selectedPlaylist->Save();
        // PlaylistInfo::GetInstance()->Refresh();
    }

    void SetPlaylistAuthor(std::string value) {
        if (!selectedPlaylist)
            return;
        if (value.empty())
            selectedPlaylist->playlistJSON.PlaylistAuthor = std::nullopt;
        else
            selectedPlaylist->playlistJSON.PlaylistAuthor = value;
        if (!addingPlaylist)
            selectedPlaylist->Save();
        // PlaylistInfo::GetInstance()->Refresh();
    }

    void SetPlaylistDescription(std::string value) {
        if (!selectedPlaylist)
            return;
        selectedPlaylist->playlistJSON.PlaylistDescription = value;
        if (!addingPlaylist)
            selectedPlaylist->Save();
        // PlaylistInfo::GetInstance()->Refresh();
    }

    void SetPlaylistCover(int imageIndex) {
        if (!selectedPlaylist && !addingPlaylist)
            return;
        if (addingPlaylist)
            addingPlaylist->imageIndex = imageIndex;
        else
            PlaylistCore::ChangePlaylistCover(selectedPlaylist, imageIndex);
        PlaylistInfo::GetInstance()->Refresh();
    }

    void Delete() {
        if (!selectedPlaylist || addingPlaylist)
            return;
        logger.info("deleting playlist {}", selectedPlaylist->name);
        PlaylistCore::DeletePlaylist(selectedPlaylist);
        selectedPlaylist = nullptr;
        Reload();
        MainMenu::GetInstance()->ShowGrid();
    }

    void ClearHighlights() {
        if (!selectedPlaylist)
            return;
        for (auto& song : selectedPlaylist->playlistJSON.Songs)
            song.Difficulties.reset();
        if (!addingPlaylist)
            selectedPlaylist->Save();
    }

    void ClearUndownloaded() {
        if (!selectedPlaylist || addingPlaylist)
            return;
        PlaylistCore::RemoveMissingSongsFromPlaylist(selectedPlaylist);
        PlaylistInfo::GetInstance()->Refresh();
    }

    void ClearSync() {
        if (!selectedPlaylist)
            return;
        auto& json = selectedPlaylist->playlistJSON;
        if (json.CustomData)
            json.CustomData->SyncURL.reset();
        if (!addingPlaylist)
            selectedPlaylist->Save();
        PlaylistInfo::GetInstance()->Refresh();
    }

    void DownloadProgress(int total, int done) {
        BSML::MainThreadScheduler::Schedule([total, done]() {
            logger.info("download progress {} {}/{}", processingPlaylist->name, done, total);

            auto progress = MainMenu::GetProgress();
            progress->gameObject->active = true;
            progress->subText1->text = fmt::format("{} / {}", done, total);
            progress->SetProgress(done / (float) total);
        });
    }

    void FinishDownload() {
        logger.info("finishing missing songs download {}", processingPlaylist->name);

        MainMenu::GetProgress()->gameObject->active = false;
        PlaylistCore::MarkPlaylistForReload(processingPlaylist);

        processingPlaylist = nullptr;
        downloading = false;
        SongCore::API::Loading::RefreshSongs();
        // songloader event will refresh
    }

    void FinishSync() {
        logger.info("finishing playlist sync {}", processingPlaylist->name);

        MainMenu::GetProgress()->gameObject->active = false;
        PlaylistCore::MarkPlaylistForReload(processingPlaylist);

        bool refreshSongs = downloading;
        processingPlaylist = nullptr;
        syncing = false;
        downloading = false;

        if (refreshSongs)
            SongCore::API::Loading::RefreshSongs();
        else
            PlaylistCore::ReloadPlaylists();
        shouldReload = false;
        // songloader events will refresh
    }

    void DownloadMissing() {
        if (!selectedPlaylist || processingPlaylist)
            return;
        logger.info("downloading missing songs for playlist {}", selectedPlaylist->name);

        processingPlaylist = selectedPlaylist;
        syncing = false;
        downloading = true;
        PlaylistInfo::GetInstance()->RefreshProcessing();

        PlaylistCore::DownloadMissingSongsFromPlaylist(processingPlaylist, FinishDownload, DownloadProgress);
    }

    void Sync() {
        if (!selectedPlaylist || processingPlaylist)
            return;
        logger.info("syncing playlist {}", selectedPlaylist->name);

        processingPlaylist = selectedPlaylist;
        downloading = false;
        syncing = true;
        PlaylistInfo::GetInstance()->RefreshProcessing();

        auto& json = processingPlaylist->playlistJSON;
        if (!json.CustomData.has_value() || !json.CustomData->SyncURL.has_value())
            return;
        auto url = *json.CustomData->SyncURL;

        WebUtils::GetAsync<WebUtils::StringResponse>({url}, [url](WebUtils::StringResponse response) {
            BSML::MainThreadScheduler::Schedule([url, response]() {
                if (!response.responseData || !response.IsSuccessful()) {
                    logger.error("{} request failed: {} {}", url, response.httpCode, response.curlStatus);
                    processingPlaylist = nullptr;
                    syncing = false;
                    PlaylistInfo::GetInstance()->RefreshProcessing();
                    return;
                }
                try {
                    ReadFromString(*response.responseData, processingPlaylist->playlistJSON);
                } catch (JSONException const& exc) {
                    logger.error("reading playlist sync response failed: {} ({})", exc.what(), url);
                }
                // keep sync url if none is downloaded
                if (!processingPlaylist->playlistJSON.CustomData)
                    processingPlaylist->playlistJSON.CustomData.emplace();
                if (!processingPlaylist->playlistJSON.CustomData->SyncURL)
                    processingPlaylist->playlistJSON.CustomData->SyncURL = url;
                processingPlaylist->Save();
                SetShouldReload(processingPlaylist);
                Reload();

                if (PlaylistCore::PlaylistHasMissingSongs(processingPlaylist)) {
                    downloading = true;
                    PlaylistInfo::GetInstance()->RefreshProcessing();
                    PlaylistCore::DownloadMissingSongsFromPlaylist(processingPlaylist, FinishSync, DownloadProgress);
                } else
                    FinishSync();
            });
        });
    }
}
