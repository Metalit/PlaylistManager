#pragma once

#include "rapidjson-macros/shared/macros.hpp"

DECLARE_JSON_CLASS(PlaylistManager, Folder,
    NAMED_VECTOR(std::string, Playlists, "playlists")
    NAMED_VALUE(std::string, FolderName, "folderName")
    NAMED_VALUE(bool, ShowDefaults, "showDefaults")
    NAMED_VECTOR(PlaylistManager::Folder, Subfolders, "subfolders")
    NAMED_VALUE(bool, HasSubfolders, "hasSubfolders")
)
