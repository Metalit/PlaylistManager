#pragma once

#include <vector>
#include <functional>

std::function<void(bool)> GetBackupFunction(std::vector<class PlaylistManager::Playlist*> playlists);

// no onKeep function, as nothing should need to change in terms of loaded playlists
void ShowBackupDialog(std::function<void(bool)> backupFunction, std::function<void()> onRestore);
