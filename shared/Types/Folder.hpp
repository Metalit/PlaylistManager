#pragma once

#include "rapidjson-macros/shared/macros.hpp"

DECLARE_JSON_CLASS(PlaylistManager, Folder,
    NAMED_AUTO_VECTOR(std::string, Playlists, "playlists")
    NAMED_AUTO_VALUE(std::string, FolderName, "folderName")
    NAMED_AUTO_VALUE(bool, ShowDefaults, "showDefaults")
    NAMED_AUTO_VECTOR(PlaylistManager::Folder, Subfolders, "subfolders")
    NAMED_AUTO_VALUE(bool, HasSubfolders, "hasSubfolders")
)