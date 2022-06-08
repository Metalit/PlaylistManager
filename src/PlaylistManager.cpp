#include "Main.hpp"
#include "Types/BPList.hpp"
#include "Types/Config.hpp"
#include "PlaylistManager.hpp"
#include "ResettableStaticPtr.hpp"
#include "SpriteCache.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

#include <filesystem>
#include <fstream>
#include <thread>

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "questui/shared/BeatSaberUI.hpp"
#include "questui/shared/CustomTypes/Components/MainThreadScheduler.hpp"

#include "songdownloader/shared/BeatSaverAPI.hpp"

#include "songloader/shared/API.hpp"

#include "System/Convert.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/SpriteMeshType.hpp"
#include "GlobalNamespace/CustomLevelLoader.hpp"
#include "GlobalNamespace/CustomPreviewBeatmapLevel.hpp"

using namespace RuntimeSongLoader;

namespace PlaylistManager {
    
    std::unordered_map<std::string, Playlist*> path_playlists;
    std::unordered_map<UnityEngine::Sprite*, std::string> image_paths;

    std::hash<std::string> hasher;
    std::unordered_map<std::size_t, int> imageHashes;
    // array of all loaded images
    std::vector<UnityEngine::Sprite*> loadedImages;

    bool hasLoaded = false;
    // all unusable playlists
    std::unordered_set<std::string> staticPackIDs{};
    // playlists that need to be reloaded on the next reload
    std::unordered_set<Playlist*> needsReloadPlaylists{};
    
    UnityEngine::Sprite* GetDefaultCoverImage() {
        return FindComponent<GlobalNamespace::CustomLevelLoader*>()->defaultPackCover;
    }

    UnityEngine::Sprite* GetCoverImage(Playlist* playlist) {
        // changes to playlist cover should change index as well
        if(playlist->imageIndex >= 0)
            return loadedImages[playlist->imageIndex];
        // index is -1 with unloaded or default cover image
        auto& json = playlist->playlistJSON;
        if(json.ImageString.has_value()) {
            std::string imageBase64 = json.ImageString.value();
            // trim "data:image/png;base64,"-like metadata
            static std::string searchString = "base64,";
            auto searchIndex = imageBase64.find(searchString);
            if(searchIndex != std::string::npos)
                imageBase64 = imageBase64.substr(searchIndex + searchString.length());
            // return loaded image if existing
            std::size_t imgHash = hasher(imageBase64);
            auto foundItr = imageHashes.find(imgHash);
            if (foundItr != imageHashes.end()) {
                LOG_INFO("Returning loaded image with hash %lu", imgHash);
                playlist->imageIndex = foundItr->second;
                return loadedImages[foundItr->second];
            }
            // check image type
            std::string imgType = GetBase64ImageType(imageBase64);
            if(imgType != ".png" && imgType != ".jpg") {
                LOG_ERROR("Unsupported image type %s", imgType.c_str());
                return GetDefaultCoverImage();
            }
            // get and write texture
            auto texture = UnityEngine::Texture2D::New_ctor(0, 0, UnityEngine::TextureFormat::RGBA32, false, false);
            UnityEngine::ImageConversion::LoadImage(texture, System::Convert::FromBase64String(imageBase64));
            // process texture size and png string and check hash for changes
            std::size_t oldHash = imgHash;
            imageBase64 = std::move(ProcessImage(texture, true));
            imgHash = hasher(imageBase64);
            // write to playlist if changed
            if(imgHash != oldHash) {
                json.ImageString = imageBase64;
                WriteToFile(playlist->path, json);
            }
            foundItr = imageHashes.find(imgHash);
            if (foundItr != imageHashes.end()) {
                LOG_INFO("Returning loaded image with hash %lu", imgHash);
                playlist->imageIndex = foundItr->second;
                return loadedImages[foundItr->second];
            }
            // save image as file and return
            LOG_INFO("Writing image with hash %lu", imgHash);
            // reuse playlist file name
            std::string playlistPathName = std::filesystem::path(playlist->path).stem();
            std::string imgPath = GetCoversPath() + "/" + playlistPathName + ".png";
            WriteImageToFile(imgPath, texture);
            imageHashes.insert({imgHash, loadedImages.size()});
            auto sprite = UnityEngine::Sprite::Create(texture, UnityEngine::Rect(0, 0, texture->get_width(), texture->get_height()), {0.5, 0.5}, 1024, 1, UnityEngine::SpriteMeshType::FullRect, {0, 0, 0, 0}, false);
            CacheSprite(sprite);
            image_paths.insert({sprite, imgPath});
            playlist->imageIndex = loadedImages.size();
            loadedImages.emplace_back(sprite);
            return sprite;
        }
        playlist->imageIndex = -1;
        return GetDefaultCoverImage();
    }

    void DeleteLoadedImage(int index) {
        auto sprite = loadedImages[index];
        // get path
        auto pathItr = image_paths.find(loadedImages[index]);
        if (pathItr == image_paths.end())
            return;
        // update image indices of playlists
        for(auto& playlist : GetLoadedPlaylists()) {
            if(playlist->imageIndex == index)
                playlist->imageIndex = -1;
            if(playlist->imageIndex > index)
                playlist->imageIndex--;
        }
        // remove from image hashes
        std::unordered_map<std::size_t, int>::iterator removeItr;
        for(auto itr = imageHashes.begin(); itr != imageHashes.end(); itr++) {
            if(itr->second == index) {
                removeItr = itr;
            }
            // decrement all indices later than i
            if(itr->second > index) {
                itr->second--;
            }
        }
        // remove from maps and delete file
        imageHashes.erase(removeItr);
        std::filesystem::remove(pathItr->second);
        image_paths.erase(sprite);
        // remove from loaded images
        loadedImages.erase(loadedImages.begin() + index);
        RemoveCachedSprite(sprite);
    }

    void LoadCoverImages() {
        // ensure path exists
        auto path = GetCoversPath();
        if(!std::filesystem::is_directory(path))
            return;
        // iterate through all image files
        for(const auto& file : std::filesystem::directory_iterator(path)) {
            if(!file.is_directory()) {
                auto path = file.path();
                // check file extension
                if(path.extension().string() == ".jpg") {
                    auto newPath = path.parent_path() / (path.stem().string() + ".png");
                    std::filesystem::rename(path, newPath);
                    path = newPath;
                } else if(path.extension().string() != ".png") {
                    LOG_ERROR("Incompatible file extension: %s", path.extension().string().c_str());
                    continue;
                }
                // check hash of base image before converting to sprite and to png
                std::ifstream instream(path, std::ios::in | std::ios::binary | std::ios::ate);
                auto size = instream.tellg();
                instream.seekg(0, instream.beg);
                auto bytes = Array<uint8_t>::NewLength(size);
                instream.read(reinterpret_cast<char*>(bytes->values), size);
                std::size_t imgHash = hasher(System::Convert::ToBase64String(bytes));
                if(imageHashes.contains(imgHash)) {
                    LOG_INFO("Skipping loading image with hash %lu", imgHash);
                    continue;
                }
                // sanatize hash by converting to png
                auto texture = UnityEngine::Texture2D::New_ctor(0, 0, UnityEngine::TextureFormat::RGBA32, false, false);
                UnityEngine::ImageConversion::LoadImage(texture, bytes);
                std::size_t oldHash = imgHash;
                imgHash = hasher(ProcessImage(texture, true));
                if(imgHash != oldHash)
                    WriteImageToFile(path.string(), texture);
                // check hash with loaded images
                if(imageHashes.contains(imgHash)) {
                    LOG_INFO("Skipping loading image with hash %lu", imgHash);
                    continue;
                }
                LOG_INFO("Loading image with hash %lu", imgHash);
                // rename file with hash for identification (coolImage -> coolImage_1987238271398)
                // could be useful if images from playlist files are loaded first, but atm they are not
                // auto searchIndex = path.stem().string().rfind("_") + 1;
                // if(path.stem().string().substr(searchIndex) != std::to_string(imgHash)) {
                //     LOG_INFO("Renaming image file with hash %lu", imgHash);
                //     std::filesystem::rename(path, path.parent_path() / (path.stem().string() + "_" + std::to_string(imgHash) + ".png"));
                // }
                imageHashes.insert({imgHash, loadedImages.size()});
                auto sprite = UnityEngine::Sprite::Create(texture, UnityEngine::Rect(0, 0, texture->get_width(), texture->get_height()), {0.5, 0.5}, 1024, 1, UnityEngine::SpriteMeshType::FullRect, {0, 0, 0, 0}, false);
                CacheSprite(sprite);
                image_paths.insert({sprite, path});
                loadedImages.emplace_back(sprite);
            }
        }
    }

    std::vector<UnityEngine::Sprite*>& GetLoadedImages() {
        return loadedImages;
    }

    void ClearLoadedImages() {
        loadedImages.clear();
        image_paths.clear();
        imageHashes.clear();
        ClearCachedSprites();
    }

    void LoadPlaylists(SongLoaderBeatmapLevelPackCollectionSO* customBeatmapLevelPackCollectionSO, bool fullReload) {
        LoadCoverImages();
        // clear playlists if requested
        if(fullReload) {
            for(auto& pair : path_playlists)
                MarkPlaylistForReload(pair.second);
        }
        // ensure path exists
        auto path = GetPlaylistsPath();
        if(!std::filesystem::is_directory(path))
            return;
        // clear out old playlists if showDefaults is off
        bool showDefaults = filterSelectionState != 2;
        if(filterSelectionState == 3 && currentFolder && !currentFolder->HasSubfolders)
            showDefaults = currentFolder->ShowDefaults;
        if(!showDefaults) {
            GlobalNamespace::CustomBeatmapLevelPack *customsPack = nullptr, *customWIPsPack = nullptr;
            for(auto& pack : customBeatmapLevelPackCollectionSO->customBeatmapLevelPacks->items) {
                if(!pack)
                    continue;
                if(pack->get_packName() == "Custom Levels")
                    customsPack = pack;
                if(pack->get_packName() == "WIP Levels")
                    customWIPsPack = pack;
            }
            if(customsPack)
                customBeatmapLevelPackCollectionSO->RemoveLevelPack(customsPack);
            if(customWIPsPack)
                customBeatmapLevelPackCollectionSO->RemoveLevelPack(customWIPsPack);
        }
        // create set of playlists that aren't found when loading
        std::unordered_set<std::string> removedPaths{};
        for(auto& path : playlistConfig.Order)
            removedPaths.insert(path);
        // create array for playlists
        std::vector<GlobalNamespace::CustomBeatmapLevelPack*> sortedPlaylists(playlistConfig.Order.size());
        // iterate through all playlist files
        for(const auto& entry : std::filesystem::directory_iterator(path)) {
            if(!entry.is_directory()) {
                Playlist* playlist = nullptr;
                // check if playlist has been loaded already
                auto path = entry.path().string();
                auto path_iter = path_playlists.find(path);
                if(path_iter != path_playlists.end())
                    playlist = path_iter->second;
                // load from cache without reload
                if(playlist && !needsReloadPlaylists.contains(playlist)) {
                    LOG_INFO("Loading playlist file %s from cache", path.c_str());
                    // check if playlist should be added
                    // check if playlist needs to be reloaded
                    if(IsPlaylistShown(playlist->path)) {
                        int packPosition = GetPlaylistIndex(playlist->path);
                        // add if new (idk how)
                        if(packPosition < 0)
                            sortedPlaylists.emplace_back(playlist->playlistCS);
                        else
                            sortedPlaylists[packPosition] = (GlobalNamespace::CustomBeatmapLevelPack*) playlist->playlistCS;
                    }
                } else {
                    LOG_INFO("Loading playlist file %s", path.c_str());
                    // only create a new playlist if one doesn't exist
                    // if one does, its contents will simply be overwritten with the reloaded data
                    if(!playlist)
                        playlist = new Playlist();
                    else {
                        needsReloadPlaylists.erase(playlist);
                        // clear cached data in playlist object
                        playlist->imageIndex = -1;
                    }
                    // get playlist object from file
                    bool success = false;
                    try {
                        ReadFromFile(path, playlist->playlistJSON);
                        success = true;
                    } catch(const std::exception& err) {
                        LOG_ERROR("Error loading playlist %s: %s", path.c_str(), err.what());
                        success = false;
                    }
                    if(success) {
                        playlist->name = playlist->playlistJSON.PlaylistTitle;
                        playlist->path = path;
                        path_playlists.insert({playlist->path, playlist});
                        // create playlist object
                        SongLoaderCustomBeatmapLevelPack* songloaderBeatmapLevelPack = SongLoaderCustomBeatmapLevelPack::Make_New(playlist->path, playlist->name, GetCoverImage(playlist));
                        playlist->playlistCS = songloaderBeatmapLevelPack->CustomLevelsPack;
                        // clear out duplicate songs
                        auto& songs = playlist->playlistJSON.Songs;
                        std::unordered_set<std::string> hashes{};
                        // add all songs to the playlist object
                        auto foundSongs = List<GlobalNamespace::CustomPreviewBeatmapLevel*>::New_ctor();
                        for(auto itr = songs.begin(); itr != songs.end(); itr++) {
                            LOWER(itr->Hash);
                            if(hashes.contains(itr->Hash)) {
                                songs.erase(itr);
                                itr--;
                            } else {
                                hashes.insert(itr->Hash);
                                auto search = RuntimeSongLoader::API::GetLevelByHash(itr->Hash);
                                if(search.has_value())
                                    foundSongs->Add(search.value());
                            }
                        }
                        // save removed duplicates
                        WriteToFile(path, playlist->playlistJSON);
                        songloaderBeatmapLevelPack->SetCustomPreviewBeatmapLevels(foundSongs->ToArray());
                        // add the playlist to the sorted array
                        if(IsPlaylistShown(playlist->path)) {
                            int packPosition = GetPlaylistIndex(playlist->path);
                            // add if new
                            if(packPosition < 0)
                                sortedPlaylists.emplace_back(songloaderBeatmapLevelPack->CustomLevelsPack);
                            else
                                sortedPlaylists[packPosition] = songloaderBeatmapLevelPack->CustomLevelsPack;
                        }
                    } else {
                        delete playlist;
                        playlist = nullptr;
                    }
                }
                // keep path in order config if loaded
                if(playlist && removedPaths.contains(path))
                    removedPaths.erase(path);
            }
        }
        // add playlists to game in sorted order
        for(auto customBeatmapLevelPack : sortedPlaylists) {
            if(customBeatmapLevelPack)
                customBeatmapLevelPackCollectionSO->AddLevelPack(customBeatmapLevelPack);
        }
        // remove paths in order config that were not loaded
        for(auto& path : removedPaths) {
            for(auto iter = playlistConfig.Order.begin(); iter != playlistConfig.Order.end(); iter++) {
                if(*iter == path) {
                    playlistConfig.Order.erase(iter);
                    iter--;
                }
            }
            // delete them if they were still loaded as well
            auto pathItr = path_playlists.find(path);
            if(pathItr != path_playlists.end()) {
                delete pathItr->second;
                path_playlists.erase(pathItr);
            }
        }
        WriteToFile(GetConfigPath(), playlistConfig);
        hasLoaded = true;
        LOG_INFO("Playlists loaded");
    }

    std::vector<Playlist*> GetLoadedPlaylists() {
        // create return vector with base size
        std::vector<Playlist*> playlistArray(playlistConfig.Order.size());
        for(auto& pair : path_playlists) {
            auto& playlist = pair.second;
            int idx = GetPlaylistIndex(playlist->path);
            if(idx >= 0)
                playlistArray[idx] = playlist;
            else
                playlistArray.push_back(playlist);
        }
        // remove empty slots
        int i = 0;
        for(auto itr = playlistArray.begin(); itr != playlistArray.end(); itr++) {
            if(*itr == nullptr) {
                playlistArray.erase(itr);
                itr--;
            }
            i++;
        }
        return playlistArray;
    }

    Playlist* GetPlaylist(std::string const& path) {
        auto iter = path_playlists.find(path);
        if(iter == path_playlists.end())
            return nullptr;
        return iter->second;
    }
    
    Playlist* GetPlaylistWithPrefix(std::string const& id) {
        static const int prefixLength = std::string(CustomLevelPackPrefixID).length();
        if(id.starts_with(CustomLevelPackPrefixID))
            return GetPlaylist(id.substr(prefixLength));
        return nullptr;
    }

    int GetPlaylistIndex(std::string const& path) {
        // find index of playlist title in config
        for(int i = 0; i < playlistConfig.Order.size(); i++) {
            if(playlistConfig.Order[i] == path)
                return i;
        }
        // add to end of config if not found
        playlistConfig.Order.push_back(path);
        WriteToFile(GetConfigPath(), playlistConfig);
        return -1;
    }

    bool IsPlaylistShown(std::string const& path) {
        if(filterSelectionState == 3 && currentFolder && !currentFolder->HasSubfolders) {
            for(std::string& testPath : currentFolder->Playlists) {
                if(path == testPath)
                    return true;
            }
            return false;
        }
        return filterSelectionState != 1;
    }

    std::string AddPlaylist(std::string const& title, std::string const& author, UnityEngine::Sprite* coverImage) {
        // create playlist with info
        auto newPlaylist = BPList();
        newPlaylist.PlaylistTitle = title;
        if(author != "")
            newPlaylist.PlaylistAuthor = author;
        if(coverImage) {
            auto bytes = UnityEngine::ImageConversion::EncodeToPNG(coverImage->get_texture());
            newPlaylist.ImageString = System::Convert::ToBase64String(bytes);
        }
        // save playlist
        std::string path = GetNewPlaylistPath(title);
        WriteToFile(path, newPlaylist);
        return path;
    }

    void MovePlaylist(Playlist* playlist, int index) {
        int originalIndex = GetPlaylistIndex(playlist->path);
        if(originalIndex < 0) {
            LOG_ERROR("Attempting to move unloaded playlist");
            return;
        }
        playlistConfig.Order.erase(playlistConfig.Order.begin() + originalIndex);
        playlistConfig.Order.insert(playlistConfig.Order.begin() + index, playlist->path);
        WriteToFile(GetConfigPath(), playlistConfig);
    }

    void RenamePlaylist(Playlist* playlist, std::string const& title) {
        // edit variables
        playlist->name = title;
        playlist->playlistJSON.PlaylistTitle = title;
        // rename playlist ingame
        auto& levelPack = playlist->playlistCS;
        if(levelPack) {
            levelPack->packName = title;
            levelPack->shortPackName = title;
        }
        // save changes
        WriteToFile(playlist->path, playlist->playlistJSON);
    }

    void ChangePlaylistCover(Playlist* playlist, int index) {
        UnityEngine::Sprite* newCover = nullptr;
        // update json image string
        auto& json = playlist->playlistJSON;
        if(index < 0) {
            newCover = GetDefaultCoverImage();
            // don't save string for default cover
            json.ImageString = std::nullopt;
        } else {
            newCover = GetLoadedImages()[index];
            // save image base 64
            auto bytes = UnityEngine::ImageConversion::EncodeToPNG(newCover->get_texture());
            json.ImageString = System::Convert::ToBase64String(bytes);
        }
        playlist->imageIndex = index;
        // change cover ingame
        auto& levelPack = playlist->playlistCS;
        if(levelPack) {
            levelPack->coverImage = newCover;
            levelPack->smallCoverImage = newCover;
        }
        WriteToFile(playlist->path, json);
    }

    void DeletePlaylist(Playlist* playlist) {
        // remove from map
        auto path_iter = path_playlists.find(playlist->path);
        if(path_iter == path_playlists.end()) {
            LOG_ERROR("Could not find playlist by path");
            return;
        }
        path_playlists.erase(path_iter);
        // delete file
        std::filesystem::remove(playlist->path);
        // remove name from order config
        int orderIndex = GetPlaylistIndex(playlist->path);
        if(orderIndex >= 0)
            playlistConfig.Order.erase(playlistConfig.Order.begin() + orderIndex);
        else
            playlistConfig.Order.erase(playlistConfig.Order.end() - 1);
        WriteToFile(GetConfigPath(), playlistConfig);
        // delete playlist object
        delete playlist;
    }

    void ReloadPlaylists(bool fullReload) {
        if(!hasLoaded)
            return;
        // handle full reload here since songloader's full refesh isn't carried through
        // also, we don't want to always full reload songs at the same time as playlists
        if(fullReload) {
            for(auto& pair : path_playlists)
                MarkPlaylistForReload(pair.second);
        }
        API::RefreshPacks();
    }

    void MarkPlaylistForReload(Playlist* playlist) {
        needsReloadPlaylists.insert(playlist);
    }

    int PlaylistHasMissingSongs(Playlist* playlist) {
        int songsMissing = 0;
        for(auto& song : playlist->playlistJSON.Songs) {
            std::string& hash = song.Hash;
            LOWER(hash);
            bool hasSong = false;
            // search in songs in playlist instead of all songs
            // we need to treat the list as an array because it is initialized as an array elsewhere
            ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(playlist->playlistCS->beatmapLevelCollection->get_beatmapLevels());
            for(int i = 0; i < levelList.Length(); i++) {
                if(hash == GetLevelHash(levelList[i])) {
                    hasSong = true;
                    break;
                }
            }
            if(hasSong)
                continue;
            songsMissing += 1;
        }
        return songsMissing;
    }

    void DownloadMissingSongsFromPlaylist(Playlist* playlist, std::function<void()> finishCallback) {
        // keep track of how many songs need to be downloaded
        // use new so that it isn't freed when the function returns, before songs finish downloading
        auto songsLeft = new std::atomic_int(0);
        // keep track of if any songs were downloaded to clean up if none were
        bool songsMissing = false;
        for(auto& song : playlist->playlistJSON.Songs) {
            std::string& hash = song.Hash;
            LOWER(hash);
            bool hasSong = false;
            // search in songs in playlist instead of all songs
            // we need to treat the list as an array because it is initialized as an array elsewhere
            ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(playlist->playlistCS->beatmapLevelCollection->get_beatmapLevels());
            for(int i = 0; i < levelList.Length(); i++) {
                if(hash == GetLevelHash(levelList[i])) {
                    hasSong = true;
                    break;
                }
            }
            if(hasSong)
                continue;
            songsMissing = true;
            *songsLeft += 1;
            BeatSaver::API::GetBeatmapByHashAsync(hash, [songsLeft, hash, finishCallback](std::optional<BeatSaver::Beatmap> beatmap){
                // after beatmap is found, download if successful, but decrement songsLeft either way
                if(beatmap.has_value()) {
                    BeatSaver::API::DownloadBeatmapAsync(beatmap.value(), [songsLeft, finishCallback](bool _){
                        *songsLeft -= 1;
                        if(*songsLeft == 0) {
                            delete songsLeft;
                            if(finishCallback)
                                QuestUI::MainThreadScheduler::Schedule(finishCallback);
                        }
                    });
                } else {
                    LOG_INFO("Beatmap with hash %s not found on beatsaver", hash.c_str());
                    *songsLeft -= 1;
                    if(*songsLeft == 0) {
                        delete songsLeft;
                        if(finishCallback)
                            QuestUI::MainThreadScheduler::Schedule(finishCallback);
                    }
                }
            });
        }
        if(!songsMissing) {
            delete songsLeft;
            if(finishCallback)
                finishCallback();
        }
    }

    void RemoveMissingSongsFromPlaylist(Playlist* playlist) {
        // store exisiting songs in a new vector to replace the song list with
        std::vector<BPSong> existingSongs = {};
        for(auto& song : playlist->playlistJSON.Songs) {
            std::string& hash = song.Hash;
            if(RuntimeSongLoader::API::GetLevelByHash(hash).has_value())
                existingSongs.push_back(song);
            else if(song.SongName.has_value())
                LOG_INFO("Removing song %s from playlist %s", song.SongName.value().c_str(), playlist->name.c_str());
            else
                LOG_INFO("Removing song with hash %s from playlist %s", hash.c_str(), playlist->name.c_str());
        }
        // set the songs of the playlist to only those found
        playlist->playlistJSON.Songs = existingSongs;
        WriteToFile(playlist->path, playlist->playlistJSON);
    }

    void AddSongToPlaylist(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level) {
        if(!level)
            return;
        // add song to cs object
        auto& pack = playlist->playlistCS;
        if(!pack)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(pack->beatmapLevelCollection->get_beatmapLevels());
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> newLevels(levelList.Length() + 1);
        for(int i = 0; i < levelList.Length(); i++) {
            auto currentLevel = levelList[i];
            if(currentLevel->get_levelID() == level->get_levelID())
                return;
            newLevels[i] = currentLevel;
        }
        newLevels[levelList.Length()] = level;
        auto readOnlyList = (System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::CustomPreviewBeatmapLevel*>*) newLevels.convert();
        ((GlobalNamespace::CustomBeatmapLevelCollection*) pack->beatmapLevelCollection)->customPreviewBeatmapLevels = readOnlyList;
        // update json object
        auto& json = playlist->playlistJSON;
        // add a blank song
        json.Songs.emplace_back(BPSong());
        // set info
        auto& songJson = *(json.Songs.end() - 1);
        songJson.Hash = GetLevelHash(level);
        songJson.SongName = level->get_songName();
        // write to file
        WriteToFile(playlist->path, json);
    }

    void RemoveSongFromPlaylist(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level) {
        if(!level)
            return;
        // remove song from cs object
        auto& pack = playlist->playlistCS;
        if(!pack)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(pack->beatmapLevelCollection->get_beatmapLevels());
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> newLevels(levelList.Length() - 1);
        // remove only one level if duplicates
        bool removed = false;
        for(int i = 0; i < levelList.Length(); i++) {
            // comparison should work
            auto currentLevel = levelList[i];
            if(removed)
                newLevels[i - 1] = currentLevel;
            else if(currentLevel->get_levelID() != level->get_levelID())
                newLevels[i] = currentLevel;
            else
                removed = true;
        }
        if(removed) {
            auto readOnlyList = (System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::CustomPreviewBeatmapLevel*>*) newLevels.convert();
            ((GlobalNamespace::CustomBeatmapLevelCollection*) pack->beatmapLevelCollection)->customPreviewBeatmapLevels = readOnlyList;
        } else {
            LOG_ERROR("Could not find song to be removed!");
            return;
        }
        // update json object
        auto& json = playlist->playlistJSON;
        // find song by hash (since the field is required) and remove
        auto levelHash = GetLevelHash(level);
        for(auto itr = json.Songs.begin(); itr != json.Songs.end(); ++itr) {
            auto& song = *itr;
            LOWER(song.Hash);
            if(song.Hash == levelHash) {
                json.Songs.erase(itr);
                // only erase
                break;
            }
        }
        // write to file
        WriteToFile(playlist->path, json);
    }

    void SetSongIndex(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level, int index) {
        if(!level)
            return;
        // remove song from cs object
        auto& pack = playlist->playlistCS;
        if(!pack)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(pack->beatmapLevelCollection->get_beatmapLevels());
        if(index >= levelList.Length() || index < 0)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> newLevels(levelList.Length());
        bool found = false;
        // ensure we traverse the whole of both lists
        for(int i = 0, j = 0; i < newLevels.Length() || j < levelList.Length(); i++) {
            // skip past level in original list, but only the first time
            if(j < levelList.Length() && levelList[j]->get_levelID() == level->get_levelID() && !found) {
                j++;
                found = true;
            }
            // shift backwards in original list when inserting level
            if(i == index) {
                j--;
                newLevels[i] = level;
            } else if(i < newLevels.Length()) {
                newLevels[i] = levelList[j];
            }
            j++;
        }
        if(found) {
            auto readOnlyList = (System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::CustomPreviewBeatmapLevel*>*) newLevels.convert();
            ((GlobalNamespace::CustomBeatmapLevelCollection*) pack->beatmapLevelCollection)->customPreviewBeatmapLevels = readOnlyList;
        } else {
            LOG_ERROR("Could not find song to be moved!");
            return;
        }
        // update json object
        auto& json = playlist->playlistJSON;
        // find song by hash (since the field is required) and remove
        auto levelHash = GetLevelHash(level);
        for(auto itr = json.Songs.begin(); itr != json.Songs.end(); ++itr) {
            auto& song = *itr;
            LOWER(song.Hash);
            if(song.Hash == levelHash) {
                json.Songs.erase(itr);
                // only erase
                break;
            }
        }
        // add a blank song
        json.Songs.insert(json.Songs.begin() + index, BPSong());
        // set info
        auto& songJson = json.Songs[index];
        songJson.Hash = GetLevelHash(level);
        songJson.SongName = level->get_songName();
        // write to file
        WriteToFile(playlist->path, json);
    }
}
