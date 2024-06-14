#pragma once

#include "playlistcore/shared/PlaylistCore.hpp"

namespace Manager {
    void PresentMenu();
    void Invalidate();

    void PresentAddShortcut(GlobalNamespace::BeatmapLevel* level);
    void PresentCreateShortcut();
    void PresentEditShortcut(PlaylistCore::Playlist* playlist);
    void PresentEditShortcut(GlobalNamespace::BeatmapLevelPack* pack);
    bool InAddShortcut();

    void OnDetailExit();
    void OnMenuExit();

    bool IsCreatingPlaylist();
    PlaylistCore::Playlist* GetSyncing();
    PlaylistCore::Playlist* GetDownloading();
    PlaylistCore::Playlist* GetSelectedPlaylist();
    void SelectPlaylist(PlaylistCore::Playlist* playlist);

    void BeginAddition();
    void ResetAddition();
    void ConfirmAddition(bool select);
    void CancelAddition();

    void SetShouldReload(PlaylistCore::Playlist* fullReloadPlaylist = nullptr);
    void Reload();

    void SetPlaylistName(std::string value);
    void SetPlaylistAuthor(std::string value);
    void SetPlaylistDescription(std::string value);
    void SetPlaylistCover(int imageIndex);

    void Delete();
    void ClearHighlights();
    void ClearUndownloaded();
    void ClearSync();
    void DownloadMissing();
    void Sync();
}
