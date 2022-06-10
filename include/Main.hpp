#pragma once

//#define LOG_INFO(value...)
#define LOG_INFO(value...) getLogger().info(value) 
// #define LOG_DEBUG(value...)
#define LOG_DEBUG(value...) getLogger().debug(value) 
//#define LOG_ERROR(value...)
#define LOG_ERROR(value...) getLogger().error(value)

#define LOWER(string) std::transform(string.begin(), string.end(), string.begin(), tolower)

#define CustomLevelPackPrefixID "custom_levelPack_"
#define CustomLevelsPackID CustomLevelPackPrefixID "CustomLevels"
#define CustomWIPLevelsPackID CustomLevelPackPrefixID "CustomWIPLevels"

#include "beatsaber-hook/shared/utils/utils.h"

Logger& getLogger();

std::string GetPlaylistsPath();
std::string GetConfigPath();
std::string GetCoversPath();
