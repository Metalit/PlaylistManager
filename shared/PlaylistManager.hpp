#pragma once

#include "Types/BPList.hpp"
#include "SimpleSafePtr.hpp"
#include "songloader/shared/CustomTypes/SongLoaderBeatmapLevelPackCollectionSO.hpp"
#include "songloader/shared/CustomTypes/SongLoaderCustomBeatmapLevelPack.hpp"
#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Texture2D.hpp"

namespace PlaylistManager {

    /// @brief A struct that wraps the data for a playlist
    struct Playlist {
        BPList playlistJSON;
        SimpleSafePtr<GlobalNamespace::CustomBeatmapLevelPack> playlistCS;
        std::string name;
        std::string path;
        int imageIndex = -1;
    };

    /// @brief Whether the playlists have loaded at least once yet
    extern bool hasLoaded;

    /// @brief Gets the default playlist cover image
    /// @return The sprite with the default cover image
    UnityEngine::Sprite* GetDefaultCoverImage();

    /// @brief Gets the cover image for a playlist
    /// @param playlist The playlist to retrieve the cover image from
    /// @return The sprite with the playlist's cover image
    UnityEngine::Sprite* GetCoverImage(Playlist* playlist);

    /// @brief Deletes a loaded image from loaded images and its file
    /// @param index The index of the image in the loaded images array
    void DeleteLoadedImage(int index);

    /// @brief Loads all unloaded cover images from the cover folder
    void LoadCoverImages();

    /// @brief Gets all images that are loaded
    /// @return A vector of all loaded images in order
    std::vector<UnityEngine::Sprite*>& GetLoadedImages();

    /// @brief Unloads all loaded images
    void ClearLoadedImages();

    /// @brief Loads all playlists from the playlists folder, adding them to the collection
    /// @param customBeatmapLevelPackCollectionSO The level pack collection that the loaded playlists will be added to
    /// @param fullReload Whether to reload the contents of already loaded playlists
    void LoadPlaylists(RuntimeSongLoader::SongLoaderBeatmapLevelPackCollectionSO* customBeatmapLevelPackCollectionSO, bool fullReload = false);

    /// @brief Gets all playlists that are loaded
    /// @return A vector of all loaded playlists in order
    std::vector<Playlist*> GetLoadedPlaylists();

    /// @brief Gets a playlist based on the string of its path, which is different from its ID
    /// @return The playlist with that path
    Playlist* GetPlaylist(std::string const& path);

    /// @brief Gets a playlist based on its ingame ID, which is its path plus the custom_levelPack_ prefix
    /// @return The playlist with that ID
    Playlist* GetPlaylistWithPrefix(std::string const& id);

    /// @brief Gets the index of a playlist in the order config, adding the playlist to the end if it is not present
    /// @return The index in the full order config, or -1 if it was not contained
    int GetPlaylistIndex(std::string const& path);

    /// @brief Returns whether or not a playlist is currently visible in the game's selection menu
    /// @return If the playlist is currently visible
    bool IsPlaylistShown(std::string const& path);

    /// @brief Adds a function to filter out playlists from being shown.
    /// The function will receive the path to each custom playlist, and the string literal "Defaults" for the Custom and Custom WIP playlists
    /// @param mod The ModInfo of the mod registering the filter
    /// @param function The filter function - takes a playlist path and returns whether it should be shown
    void AddPlaylistFilter(ModInfo mod, std::function<bool(std::string const& path)> function);

    /// @brief Removes all of the playlist filter functions added by a mod
    /// @param mod The ModInfo of the mod
    void RemovePlaylistFilters(ModInfo mod);

    /// @brief Creates a new playlist file - does not load it
    /// @param title The name of the playlist to be created
    /// @param author The author of the playlist to be created
    /// @param coverImage The cover image for the playlist - does not have to be a loaded image
    /// @return The path of the created playlist
    std::string AddPlaylist(std::string const& title, std::string const& author, UnityEngine::Sprite* coverImage = nullptr);

    /// @brief Moves a playlist in the order config - does not reload playlists
    /// @param playlist The playlist to be moved
    /// @param index The index to move the playlist to
    void MovePlaylist(Playlist* playlist, int index);

    /// @brief Renames a playlist - does not reload playlists
    /// @param playlist The playlist to rename
    /// @param title The new name for the playlist
    void RenamePlaylist(Playlist* playlist, std::string const& title);

    /// @brief Changes the cover of a playlist to the cover at the index - does not reload playlists
    /// @param playlist The playlist to change the cover of
    /// @param index The index of the new cover image in the loaded images array
    void ChangePlaylistCover(Playlist* playlist, int index);

    /// @brief Deletes a playlist and its file - does not reload playlists
    /// @param playlist The playlist to delete
    void DeletePlaylist(Playlist* playlist);

    /// @brief Reloads playlists
    /// @param fullReload Whether to reload the contents of already loaded playlists
    void ReloadPlaylists(bool fullReload = false);
    
    /// @brief Makes a single playlist be fully reloaded on the next reload
    /// @param playlist The playlist to be reloaded
    void MarkPlaylistForReload(Playlist* playlist);

    /// @brief Checks whether or not a playlist is missing songs
    /// @param playlist The playlist to check for missing songs
    /// @return The number of songs missing from the playlist
    int PlaylistHasMissingSongs(Playlist* playlist);

    /// @brief Downloads songs that are supposed to be in a playlist but not owned - does not modify the playlist
    /// @param playlist The playlist to check for missing songs
    /// @param finishCallback A function to run after all missing songs have been downloaded
    void DownloadMissingSongsFromPlaylist(Playlist* playlist, std::function<void()> finishCallback = nullptr);

    /// @brief Removes songs that are supposed to be in a playlist but not owned from the playlist
    /// @param playlist The playlist to remove missing songs from
    void RemoveMissingSongsFromPlaylist(Playlist* playlist);

    /// @brief Adds a song to a playlist - does not reload playlists
    /// @param playlist The playlist to add the song to
    /// @param level The song to add to the playlist
    void AddSongToPlaylist(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level);

    /// @brief Removes a song from a playlist - does not reload playlists
    /// @param playlist The playlist to remove the song from
    /// @param level The song to remove from the playlist
    void RemoveSongFromPlaylist(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level);

    /// @brief Changes the index of a song inside a playlist - does not reload playlists
    /// @param playlist The playlist containing the song to be reordered
    /// @param index The new index for the song to be at
    void SetSongIndex(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level, int index);
}
