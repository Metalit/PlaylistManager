#pragma once

#include "UnityEngine/Texture2D.hpp"
#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/IBeatmapLevelPack.hpp"

namespace PlaylistManager {
    class Playlist;
}

struct SelectionState {
    PlaylistManager::Playlist* selectedPlaylist;
    int selectedPlaylistIdx;
    GlobalNamespace::IPreviewBeatmapLevel* selectedSong;
    int selectedSongIdx;
};

std::string GetLevelHash(GlobalNamespace::IPreviewBeatmapLevel* level);

bool IsWipLevel(GlobalNamespace::IPreviewBeatmapLevel* level);

void RemoveAllBMBFSuffixes();

std::string SanitizeFileName(std::string const& fileName);

bool UniqueFileName(std::string const& fileName, std::string const& compareDirectory);

std::string GetNewPlaylistPath(std::string const& title);

std::string GetPlaylistBackupPath(std::string const& path);

std::string GetBase64ImageType(std::string const& base64);

std::string ProcessImage(UnityEngine::Texture2D* texture, bool returnPngString);

void WriteImageToFile(std::string const& pathToPng, UnityEngine::Texture2D* texture);

List<GlobalNamespace::IBeatmapLevelPack*>* GetCustomPacks();

void SetCustomPacks(List<GlobalNamespace::IBeatmapLevelPack*>* newPlaylists, bool updateSongs);

SelectionState GetSelectionState();

void SetSelectionState(const SelectionState& state);

void ReloadSongsKeepingSelection(std::function<void()> finishCallback = nullptr);

void ReloadPlaylistsKeepingSelection();
