#pragma once

#include "config-utils/shared/config-utils.hpp"

namespace PlaylistManager {
    DECLARE_JSON_CLASS(Folder,
        NAMED_VECTOR(std::string, Playlists, "playlists")
        NAMED_VALUE(std::string, FolderName, "folderName")
        NAMED_VALUE(bool, ShowDefaults, "showDefaults")
        NAMED_VECTOR(PlaylistManager::Folder, Subfolders, "subfolders")
        NAMED_VALUE(bool, HasSubfolders, "hasSubfolders")
    )
}

DECLARE_CONFIG(Config,
    CONFIG_VALUE(DownloadIcon, bool, "showDownloadIcon", true);
    CONFIG_VALUE(Folders, std::vector<PlaylistManager::Folder>, "folders", {});
)

// 0: all playlists, 1: just defaults, 2: just customs, 3: use current folder
extern int filterSelectionState;
extern bool allowInMultiplayer;
