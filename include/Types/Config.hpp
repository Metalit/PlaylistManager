#pragma once

#include "shared/Types/Folder.hpp"

DECLARE_JSON_CLASS(PlaylistManager, PlaylistConfig,
    NAMED_AUTO_VALUE_DEFAULT(bool, Management, false, "enableManagement")
    NAMED_AUTO_VALUE_DEFAULT(bool, DownloadIcon, false, "showDownloadIcon")
    NAMED_AUTO_VALUE_DEFAULT(bool, RemoveMissing, true, "removeMissingBeatSaverSongs")
    NAMED_AUTO_VALUE_DEFAULT(float, ScrollSpeed, 2, "scrollSpeed")
    NAMED_AUTO_VECTOR(std::string, Order, "order")
    NAMED_AUTO_VECTOR(Folder, Folders, "folders")
)

// defined in Main.cpp
extern PlaylistManager::PlaylistConfig playlistConfig;
extern PlaylistManager::Folder* currentFolder;
// 0: all playlists, 1: just defaults, 2: just customs, 3: use current folder
extern int filterSelectionState;
extern bool allowInMultiplayer;

void SaveConfig();

void UpdateScrollSpeed();
