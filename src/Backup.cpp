#include "Main.hpp"
#include "PlaylistManager.hpp"
#include "Utils.hpp"
#include "Backup.hpp"

#include "Types/BPList.hpp"

#include <filesystem>

using namespace PlaylistManager;

// returns the match of a type in a list of it, or the search object if not found
template<class T>
T& IdentifyMatch(T& backup, std::vector<T>& objs);

BPSong& IdentifyMatch(BPSong& backup, std::vector<BPSong>& objs) {
    for(auto& obj : objs) {
        if(obj.Hash == backup.Hash)
            return obj;
    }
    return backup;
}

using DiffFunc = std::function<void(bool)>;

// returns a function that will handle either restoring or not based on an object and a backup of it
template<class T>
DiffFunc GetBackupProcessor(T& obj, T& backup);

template<>
DiffFunc GetBackupProcessor(BPSong& obj, BPSong& backup) {
    std::vector<DiffFunc> funcs;
    // use preferred songName
    if(obj.SongName != backup.SongName) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.SongName = backup.SongName;
        });
    }
    // use preferred key
    if(obj.Key != backup.Key) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.Key = backup.Key;
        });
    }
    // restore difficulties if removed, otherwise use preferred
    if(!obj.Difficulties && backup.Difficulties) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            obj.Difficulties = backup.Difficulties;
        });
    } else if(obj.Difficulties != backup.Difficulties) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.Difficulties = backup.Difficulties;
        });
    }
    if(funcs.empty())
        return nullptr;
    // return function that runs all functions
    return [funcs = std::move(funcs)](bool restore) {
        for(auto& func : funcs)
            func(restore);
    };
}

template<>
DiffFunc GetBackupProcessor<BPList>(BPList& obj, BPList& backup) {
    std::vector<DiffFunc> funcs;
    // use preferred title
    if(obj.PlaylistTitle != backup.PlaylistTitle) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.PlaylistTitle = backup.PlaylistTitle;
        });
    }
    // restore author if removed, otherwise use preferred
    if(!obj.PlaylistAuthor && backup.PlaylistAuthor) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            obj.PlaylistAuthor = backup.PlaylistAuthor;
        });
    } else if(obj.PlaylistAuthor != backup.PlaylistAuthor) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.PlaylistAuthor = backup.PlaylistAuthor;
        });
    }
    // restore description if removed, otherwise use preferred
    if(!obj.PlaylistDescription && backup.PlaylistDescription) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            obj.PlaylistDescription = backup.PlaylistDescription;
        });
    } else if(obj.PlaylistDescription != backup.PlaylistDescription) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.PlaylistDescription = backup.PlaylistDescription;
        });
    }
    // pick one if not equal (items removed, order changed, etc...)
    if(obj.Songs != backup.Songs) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            // copy songs if restoring from backup
            if(restore)
                obj.Songs = backup.Songs;
            else {
                // do backup restoration for all preserved songs
                for(auto& song : obj.Songs) {
                    auto& songBackup = IdentifyMatch(song, backup.Songs);
                    // backup processor for two of the same object should be nothing
                    if(auto func = GetBackupProcessor(song, songBackup))
                        func(restore);
                }
            }
        });
    }
    // use preferred image string
    if(obj.ImageString != backup.ImageString) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.ImageString = backup.ImageString;
        });
    }
    // restore customData if removed, otherwise use preferred
    if(!obj.CustomData && backup.CustomData) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            obj.CustomData = backup.CustomData;
        });
    } else if(obj.CustomData != backup.CustomData) {
        funcs.emplace_back([&obj, &backup](bool restore) {
            if(restore)
                obj.CustomData = backup.CustomData;
        });
    }
    if(funcs.empty())
        return nullptr;
    // return function that runs all functions
    return [funcs = std::move(funcs)](bool restore) {
        for(auto& func : funcs)
            func(restore);
    };
}

DiffFunc GetBackupFunction(std::vector<Playlist*> playlists) {
    // make backup if none exists
    if(std::filesystem::is_empty(GetBackupsPath())) {
        std::filesystem::remove_all(GetBackupsPath());
        std::filesystem::copy(GetPlaylistsPath(), GetBackupsPath());
        return nullptr;
    }
    std::vector<DiffFunc> funcs;
    // get lists of names of playlist and backup files
    std::vector<std::string> playlistPaths;
    std::vector<std::string> backupPaths;
    for(auto& playlist : playlists) {
        // trim all but file name
        std::string path(playlist->path);
        if(!path.starts_with(GetPlaylistsPath())) {
            LOG_ERROR("Playlist was not in playlist directory: %s", path.c_str());
            std::filesystem::remove(path);
            continue;
        }
        path = path.substr(GetPlaylistsPath().length());
        playlistPaths.emplace_back(path);
    }
    for(const auto& entry : std::filesystem::directory_iterator(GetBackupsPath())) {
        // trim all but file name
        std::string path(entry.path().string());
        if(!path.starts_with(GetBackupsPath())) {
            LOG_ERROR("Playlist was not in playlist directory: %s", path.c_str());
            std::filesystem::remove(path);
            continue;
        }
        path = path.substr(GetBackupsPath().length());
        backupPaths.emplace_back(path);
    }
    // get backup processors for all playlists present in both places
    for(auto& backupPath : backupPaths) {
        int idx = -1;
        for(int i = 0; i < playlistPaths.size(); i++) {
            if(playlistPaths[i] == backupPath) {
                idx = i;
                break;
            }
        }
        // playlist is present in backups and playlist vector
        if(idx >= 0) {
            // turn backup file into BPList object for comparison
            BPList backupJson;
            ReadFromFile(GetBackupsPath() + "/" + backupPath, backupJson);
            // add backup function if there is one for the playlists
            if(auto func = GetBackupProcessor(playlists[idx]->playlistJSON, backupJson)) {
                funcs.emplace_back([playlist = playlists[idx], func = std::move(func)](bool restore) {
                    func(restore);
                    playlist->Save();
                    MarkPlaylistForReload(playlist);
                });
            }
        }
    }
    // add function to update backup if an update would be needed
    if(!funcs.empty()) {
        funcs.emplace_back([](bool restore) {
            if(!restore) {
                std::filesystem::remove_all(GetBackupsPath());
                std::filesystem::copy(GetPlaylistsPath(), GetBackupsPath());
            }
        });
    }
    // add function to copy all playlists from backup if there are differences
    if(playlistPaths != backupPaths) {
        return [playlists = std::move(playlists), playlistPaths = std::move(playlistPaths), backupPaths = std::move(backupPaths), funcs = std::move(funcs)](bool restore) {
            // copy everything when restoring a backup
            if(restore) {
                std::filesystem::remove_all(GetPlaylistsPath());
                std::filesystem::copy(GetBackupsPath(), GetPlaylistsPath());
                for(auto& playlist : playlists) {
                    MarkPlaylistForReload(playlist);
                }
            // otherwise, still run backup logic for non restored
            } else {
                for(auto& func : funcs)
                    func(restore);
            }
        };
    }
    if(funcs.empty())
        return nullptr;
    return [funcs = std::move(funcs)](bool restore) {
        for(auto& func : funcs)
            func(restore);
    };
}

void ShowBackupDialog(std::function<void(bool)> backupFunction, std::function<void()> onRestore) {
    if(!backupFunction)
        return;
}
