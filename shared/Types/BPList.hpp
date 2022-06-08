#pragma once

#include "rapidjson-macros/shared/macros.hpp"

DECLARE_JSON_CLASS(PlaylistManager, Difficulty,
    NAMED_AUTO_VALUE(std::string, Characteristic, "characteristic");
    NAMED_AUTO_VALUE(std::string, Name, "name");
)

DECLARE_JSON_CLASS(PlaylistManager, BPSong,
    NAMED_AUTO_VALUE(std::string, Hash, "hash");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, SongName, "songName");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, Key, "key");
    NAMED_AUTO_VECTOR_OPTIONAL(PlaylistManager::Difficulty, Difficulties, "difficulties");
)

DECLARE_JSON_CLASS(PlaylistManager, CustomData,
    NAMED_AUTO_VALUE_OPTIONAL(std::string, SyncURL, "syncURL");
)

DECLARE_JSON_CLASS(PlaylistManager, BPList,
    NAMED_AUTO_VALUE(std::string, PlaylistTitle, "playlistTitle");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, PlaylistAuthor, "playlistAuthor");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, PlaylistDescription, "playlistDescription");
    NAMED_AUTO_VECTOR(PlaylistManager::BPSong, Songs, "songs");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, ImageString, "imageString");
    NAMED_AUTO_VALUE_OPTIONAL(PlaylistManager::CustomData, CustomData, "customData");
    DESERIALIZE_ACTION(0,
        if(jsonValue.HasMember("downloadURL") && jsonValue["downloadURL"].IsString()) {
            if(!outerClass->CustomData.has_value())
                outerClass->CustomData.emplace();
            outerClass->CustomData->SyncURL = jsonValue["downloadURL"].GetString();
        }
    )
)