#include "Main.hpp"
#include "PlaylistManager.hpp"
#include "Utils.hpp"
#include "Backup.hpp"
#include "ResettableStaticPtr.hpp"

#include "Types/BPList.hpp"

#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "UnityEngine/WaitForFixedUpdate.hpp"

#include "questui/shared/BeatSaberUI.hpp"
#include "custom-types/shared/coroutine.hpp"

#include <filesystem>

using namespace PlaylistManager;
using namespace QuestUI;

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

using RestoreFunc = std::function<void()>;

// returns a function that will handle either restoring or not based on an object and a backup of it
template<class T>
bool ProcessBackup(T& obj, T& backup) {
    return false;
}

template<class V>
bool ProcessBackup(std::optional<V>& obj, std::optional<V>& backup) {
    if(!backup.has_value())
        return false;
    if(!obj.has_value()) {
        obj.emplace(backup.value());
        return true;
    }
    return ProcessBackup(obj.value(), backup.value());
}

template<>
bool ProcessBackup(BPSong& obj, BPSong& backup) {
    bool changed = false;
    // restore difficulties if removed
    changed |= ProcessBackup(obj.Difficulties, backup.Difficulties);
    return changed;
}

template<>
bool ProcessBackup(CustomData& obj, CustomData& backup) {
    bool changed = false;
    // restore sync url if removed
    changed |= ProcessBackup(obj.SyncURL, backup.SyncURL);
    return changed;
}

template<>
bool ProcessBackup(BPList& obj, BPList& backup) {
    bool changed = false;
    // restore author if removed
    changed |= ProcessBackup(obj.PlaylistAuthor, backup.PlaylistAuthor);
    // restore description if removed
    changed |= ProcessBackup(obj.PlaylistDescription, backup.PlaylistDescription);
    // do backup restoration for all preserved songs
    if(obj.Songs != backup.Songs) {
        for(auto& song : obj.Songs) {
            auto& songBackup = IdentifyMatch(song, backup.Songs);
            // backup processor for two of the same object should be nothing
            changed |= ProcessBackup(song, songBackup);
        }
    }
    // restore customData if removed
    changed |= ProcessBackup(obj.CustomData, backup.CustomData);
    return changed;
}

RestoreFunc GetBackupFunction() {
    // make backup if none exists
    if(std::filesystem::is_empty(GetBackupsPath())) {
        LOG_INFO("Creating backups");
        std::filesystem::remove(GetBackupsPath());
        std::filesystem::copy(GetPlaylistsPath(), GetBackupsPath());
        return nullptr;
    }
    // whether the playlists are different from those backed up
    bool changes = false;
    // whether changes to the playlists were made when processing the backups
    bool changed = false;
    // get lists of names of playlist and backup files
    std::unordered_set<std::string> playlistPaths;
    std::unordered_set<std::string> backupPaths;
    for(const auto& entry : std::filesystem::directory_iterator(GetPlaylistsPath())) {
        // trim all but file name
        std::string path(entry.path().string());
        path = path.substr(GetPlaylistsPath().length());
        playlistPaths.insert(path);
    }
    for(const auto& entry : std::filesystem::directory_iterator(GetBackupsPath())) {
        // trim all but file name
        std::string path(entry.path().string());
        path = path.substr(GetBackupsPath().length());
        backupPaths.insert(path);
    }
    // get backup processors for all playlists present in both places
    for(auto& path : backupPaths) {
        bool inBoth = false;
        for(auto& currentPath : playlistPaths) {
            if(currentPath == path) {
                inBoth = true;
                break;
            }
        }
        if(inBoth) {
            LOG_INFO("comparing playlist %s", path.c_str());
            // load both into objects
            static BPList currentJson;
            ReadFromFile(GetPlaylistsPath() + path, currentJson);
            static BPList backupJson;
            ReadFromFile(GetBackupsPath() + path, backupJson);
            // process backup and make sure the playlist is reloaded if changed
            if(ProcessBackup(currentJson, backupJson)) {
                changed = true;
                WriteToFile(GetPlaylistsPath() + path, currentJson);
                // reload playlist if already loaded
                if(auto playlist = GetPlaylist(GetPlaylistsPath() + path)) {
                    MarkPlaylistForReload(playlist);
                }
            }
            if(currentJson != backupJson) {
                changes = true;
            }
        }
    }
    // add function to copy all playlists from backup if there are differences
    if(playlistPaths != backupPaths || changes) {
        return [playlistPaths = std::move(playlistPaths), backupPaths = std::move(backupPaths)] {
            // copy and reload everything when restoring a backup
            std::filesystem::remove_all(GetPlaylistsPath());
            std::filesystem::copy(GetBackupsPath(), GetPlaylistsPath());
            for(auto& playlist : GetLoadedPlaylists()) {
                MarkPlaylistForReload(playlist);
            }
        };
    }
    return nullptr;
}

RestoreFunc backupFunction;

// significant credit for the ui to https://github.com/jk4837/PlaylistEditor/blob/master/src/Utils/UIUtils.cpp
HMUI::ModalView* MakeDialog() {
    auto parent = FindComponent<GlobalNamespace::MainMenuViewController*>()->get_transform();
    auto modal = BeatSaberUI::CreateModal(parent, {65, 41}, nullptr, false);

    static ConstString contentName("Content");

    auto restoreButton = BeatSaberUI::CreateUIButton(modal->get_transform(), "Revert", "ActionButton", {-16, -14}, [modal] {
        LOG_INFO("Restoring backup");
        modal->Hide(true, nullptr);
        backupFunction();
        ReloadPlaylists();
    });
    UnityEngine::Object::Destroy(restoreButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());

    auto cancelButton = QuestUI::BeatSaberUI::CreateUIButton(modal->get_transform(), "Keep", {16, -14}, [modal] {
        modal->Hide(true, nullptr);
        std::filesystem::remove_all(GetBackupsPath());
        std::filesystem::copy(GetPlaylistsPath(), GetBackupsPath());
        ReloadPlaylists();
    });
    UnityEngine::Object::Destroy(cancelButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());

    TMPro::TextMeshProUGUI* title = BeatSaberUI::CreateText(modal->get_transform(), "Playlist Manager", false, {0, 16}, {60, 8.5});
    title->set_alignment(TMPro::TextAlignmentOptions::Center);
    title->set_fontStyle(TMPro::FontStyles::Bold);

    static ConstString dialogText("External playlist modifications detected (likely through BMBF). Changes made by Playlist Manager may be lost. Would you like to revert or keep the changes?");

    TMPro::TextMeshProUGUI* message = QuestUI::BeatSaberUI::CreateText(modal->get_transform(), dialogText, false, {0, 2}, {60, 25.5});
    message->set_enableWordWrapping(true);
    message->set_alignment(TMPro::TextAlignmentOptions::Center);

    modal->get_transform()->SetAsLastSibling();
    return modal;
}

custom_types::Helpers::Coroutine ShowBackupDialogCoroutine() {
    auto mainViewController = FindComponent<GlobalNamespace::MainMenuViewController*>();
    while(!mainViewController->wasActivatedBefore)
        co_yield nullptr;
    
    STATIC_AUTO(modal, MakeDialog());
    modal->Show(true, true, nullptr);
}

void ShowBackupDialog(RestoreFunc backupFunc) {
    backupFunction = backupFunc;
    if(!backupFunction)
        return;
    GlobalNamespace::SharedCoroutineStarter::get_instance()->StartCoroutine(
        custom_types::Helpers::CoroutineHelper::New(ShowBackupDialogCoroutine()) );
}
