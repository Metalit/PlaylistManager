#include "Main.hpp"
#include "Utils.hpp"
#include "PlaylistManager.hpp"
#include "ResettableStaticPtr.hpp"

#include "songloader/shared/API.hpp"

#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "System/Convert.hpp"
#include "System/Action_4.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/LevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/IBeatmapLevelPackCollection.hpp"
#include "GlobalNamespace/BeatmapLevelPackCollection.hpp"
#include "GlobalNamespace/IAnnotatedBeatmapLevelCollection.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridView.hpp"
#include "GlobalNamespace/PageControl.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsViewController.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridViewAnimator.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/LevelSearchViewController.hpp"

#include <filesystem>

using namespace PlaylistManager;

// desired image size
const int imageSize = 512;

std::string GetLevelHash(GlobalNamespace::IPreviewBeatmapLevel* level) {
    std::string id = level->get_levelID();
    // should be in all songloader levels
    auto prefixIndex = id.find("custom_level_");
    if(prefixIndex == std::string::npos)
        return "";
    // remove prefix
    id = id.substr(prefixIndex + 13);
    auto wipIndex = id.find(" WIP");
    if(wipIndex != std::string::npos)
        id = id.substr(0, wipIndex);
    LOWER(id);
    return id;
}

bool IsWipLevel(GlobalNamespace::IPreviewBeatmapLevel* level) {
    return level->get_levelID().ends_with(" WIP");
}

void RemoveAllBMBFSuffixes() {
    static const std::string suffix("_BMBF.json");
    static const int suffixLength = suffix.length();
    for(const auto& entry : std::filesystem::directory_iterator(GetPlaylistsPath())) {
        auto path = entry.path().string();
        while(path.ends_with(suffix))
            path = path.substr(0, path.length() - suffixLength);
        std::filesystem::rename(entry.path(), path + suffix);
    }
}

std::string SanitizeFileName(std::string const& fileName) {
    std::string newName;
    // yes I know not all of these are disallowed, and also that they are unlikely to end up in a name
    static const std::unordered_set<unsigned char> replacedChars = {
        '/', '\n', '\\', '\'', '\"', ' ', '?', '<', '>', ':', '*'
    };
    std::transform(fileName.begin(), fileName.end(), std::back_inserter(newName), [](unsigned char c){
        if(replacedChars.contains(c))
            return (unsigned char)('_');
        return c;
    });
    if(newName == "")
        return "_";
    return newName;
}

bool UniqueFileName(std::string const& fileName, std::string const& compareDirectory) {
    if(!std::filesystem::is_directory(compareDirectory))
        return true;
    for(auto& entry : std::filesystem::directory_iterator(compareDirectory)) {
        if(entry.is_directory())
            continue;
        if(entry.path().filename().string() == fileName)
            return false;
    }
    return true;
}

std::string GetNewPlaylistPath(std::string const& title) {
    std::string fileTitle = SanitizeFileName(title);
    while(!UniqueFileName(fileTitle + ".bplist_BMBF.json", GetPlaylistsPath()))
        fileTitle += "_";
    return GetPlaylistsPath() + "/" + fileTitle + ".bplist_BMBF.json";
}

std::string GetPlaylistBackupPath(std::string const& path) {
    if(!path.starts_with(GetPlaylistsPath()))
        return "";
    std::string suffix = path.substr(GetPlaylistsPath().length());
    return GetBackupsPath() + suffix;
}

std::string GetBase64ImageType(std::string const& base64) {
    if(base64.length() < 3)
        return "";
    std::string sub = base64.substr(0, 3);
    if(sub == "iVB")
        return ".png";
    if(sub == "/9j")
        return ".jpg";
    if(sub == "R0l")
        return ".gif";
    if(sub == "Qk1")
        return ".bmp";
    return "";
}

std::string ProcessImage(UnityEngine::Texture2D* texture, bool returnPngString) {
    // check texture size and resize if necessary
    int width = texture->get_width();
    int height = texture->get_height();
    if(width > imageSize && height > imageSize) {
        // resize (https://gist.github.com/gszauer/7799899 modified for only downscaling)
        auto texColors = texture->GetPixels();
        ArrayW<UnityEngine::Color> newColors(imageSize * imageSize);
        float ratio_x = ((float) width - 1) / imageSize;
        float ratio_y = ((float) height - 1) / imageSize;

        for(int y = 0; y < imageSize; y++) {
            int offset_from_y = y * imageSize;

            int old_texture_y = floor(y * ratio_y);
            int old_texture_offset_from_y = old_texture_y * width;
            
            for(int x = 0; x < imageSize; x++) {
                int old_texture_x = floor(x * ratio_x);

                newColors[offset_from_y + x] = texColors[old_texture_offset_from_y + old_texture_x];
            }
        }
        texture->Resize(imageSize, imageSize);
        texture->SetPixels(newColors);
        texture->Apply();
    }
    if(!returnPngString)
        return "";
    // convert to png if necessary
    // can sometimes need two passes to reach a fixed result
    // but I don't want to to it twice for every cover, so it will just normalize itself after another restart
    // and it shouldn't be a problem through ingame cover management
    auto bytes = UnityEngine::ImageConversion::EncodeToPNG(texture);
    return System::Convert::ToBase64String(bytes);
}

void WriteImageToFile(std::string const& pathToPng, UnityEngine::Texture2D* texture) {
    auto bytes = UnityEngine::ImageConversion::EncodeToPNG(texture);
    writefile(pathToPng, std::string((char*) bytes.begin(), bytes.Length()));
}

List<GlobalNamespace::IBeatmapLevelPack*>* GetCustomPacks() {
    using namespace GlobalNamespace;
    return List<IBeatmapLevelPack*>::New_ctor(*((System::Collections::Generic::IEnumerable_1<IBeatmapLevelPack*>**) &FindComponent<LevelFilteringNavigationController*>()->customLevelPacks));
}

void SetCustomPacks(List<GlobalNamespace::IBeatmapLevelPack*>* newPlaylists, bool updateSongs) {
    using namespace GlobalNamespace;
    auto packArray = newPlaylists->ToArray();
    auto packReadOnly = (System::Collections::Generic::IReadOnlyList_1<IAnnotatedBeatmapLevelCollection*>*) newPlaylists->AsReadOnly();
    // update in levels model to avoid resetting
    auto levelsModel = FindComponent<BeatmapLevelsModel*>();
    levelsModel->customLevelPackCollection = (IBeatmapLevelPackCollection*) BeatmapLevelPackCollection::New_ctor(packArray);
    // update in navigation controller also to avoid resetting
    auto navigationController = FindComponent<GlobalNamespace::LevelFilteringNavigationController*>();
    if(!navigationController || !navigationController->get_isInViewControllerHierarchy())
        return;
    auto collectionsViewController = navigationController->annotatedBeatmapLevelCollectionsViewController;
    navigationController->customLevelPacks = packArray;
    auto gameTableView = FindComponent<AnnotatedBeatmapLevelCollectionsGridView*>();
    // only update the game table if custom songs are selected
    if(navigationController->selectLevelCategoryViewController->get_selectedLevelCategory() == SelectLevelCategoryViewController::LevelCategory::CustomSongs) {
        // SetData causes the page control to reset, causing scrolling flashes
        gameTableView->annotatedBeatmapLevelCollections = packReadOnly;
        gameTableView->gridView->ReloadData();
        gameTableView->pageControl->SetPagesCount(gameTableView->gridView->rowCount);
        // update row count in animator and animated objects
        gameTableView->animator->rowCount = gameTableView->gridView->rowCount;
        auto sizeDelta = gameTableView->animator->contentTransform->get_sizeDelta();
        sizeDelta.y = gameTableView->animator->rowHeight * gameTableView->animator->rowCount;
        gameTableView->animator->contentTransform->set_sizeDelta(sizeDelta);
        auto anchoredPosition = gameTableView->animator->contentTransform->get_anchoredPosition();
        anchoredPosition.y = gameTableView->animator->GetContentYOffset();
        gameTableView->animator->contentTransform->set_anchoredPosition(anchoredPosition);
    }
    if(updateSongs) {
        // concatenate arrays together
        auto arr1 = navigationController->ostBeatmapLevelPacks;
        auto arr2 = navigationController->musicPacksBeatmapLevelPacks;
        ArrayW<IBeatmapLevelPack*> newAllPacks(arr1.Length() + arr2.Length() + packArray.Length());
        for(int i = 0; i < arr1.Length(); i++)
            newAllPacks[i] = arr1[i];
        for(int i = 0; i < arr2.Length(); i++)
            newAllPacks[i + arr1.Length()] = arr2[i];
        for(int i = 0; i < packArray.Length(); i++)
            newAllPacks[i + arr1.Length() + arr2.Length()] = packArray[i];
        navigationController->allBeatmapLevelPacks = newAllPacks;
        // update the levels shown in the search view controller
        navigationController->levelSearchViewController->Setup(newAllPacks);
        // only invoke callbacks in custom songs view
        if(navigationController->selectLevelCategoryViewController->get_selectedLevelCategory() == SelectLevelCategoryViewController::LevelCategory::CustomSongs) {
            auto selectionCoordinator = FindComponent<LevelSelectionFlowCoordinator*>();
            auto collectionController = FindComponent<LevelCollectionNavigationController*>();
            if(packArray.Length() > 0) {
                // select cell with index 0 and invoke callback
                if(gameTableView->selectedCellIndex == 0)
                    collectionController->SetDataForPack(packArray.First(), true, !selectionCoordinator->get_hidePracticeButton(), selectionCoordinator->get_actionButtonText()); // skip to top of callback chain
                else
                    gameTableView->SelectAndScrollToCellWithIdx(0); // only invokes callback if selection has changed
            } else
                collectionController->SetDataForLevelCollection(nullptr, !selectionCoordinator->get_hidePracticeButton(), selectionCoordinator->get_actionButtonText(), navigationController->emptyCustomSongListInfoPrefab);
        // update level search controller - favorites or regular search - if open
        } else if(navigationController->selectLevelCategoryViewController->get_selectedLevelCategory() != SelectLevelCategoryViewController::LevelCategory::MusicPacks)
            navigationController->levelSearchViewController->UpdateBeatmapLevelPackCollectionAsync();
    }
}

SelectionState GetSelectionState() {
    LOG_INFO("SelectionState get");
    SelectionState state{};
    auto gridView = FindComponent<GlobalNamespace::AnnotatedBeatmapLevelCollectionsGridView*>();
    auto listIdx = gridView->selectedCellIndex;
    // virtual methods T_T
    auto pack = ArrayW<GlobalNamespace::IBeatmapLevelPack*>(gridView->annotatedBeatmapLevelCollections)[listIdx];
    state.selectedPlaylist = GetPlaylistWithPrefix(pack->get_packID());
    state.selectedPlaylistIdx = listIdx;
    auto levelsView = FindComponent<GlobalNamespace::LevelCollectionTableView*>();
    state.selectedSong = levelsView->selectedPreviewBeatmapLevel;
    state.selectedSongIdx = levelsView->selectedRow;
    // the SelectLevelPackHeaderCell method doesn't update selectedPreviewBeatmapLevel
    if(levelsView->selectedRow == 0)
        state.selectedSong = nullptr;
    return state;
}

void SetSelectionState(const SelectionState& state) {
    LOG_INFO("SelectionState set");
    auto gridView = FindComponent<GlobalNamespace::AnnotatedBeatmapLevelCollectionsGridView*>();
    if(state.selectedPlaylistIdx >= gridView->GetNumberOfCells())
        return;
    // virtual methods T_T
    auto pack = ArrayW<GlobalNamespace::IBeatmapLevelPack*>(gridView->annotatedBeatmapLevelCollections)[state.selectedPlaylistIdx];
    if(GetPlaylistWithPrefix(pack->get_packID()) != state.selectedPlaylist)
        return;
    gridView->SelectAndScrollToCellWithIdx(state.selectedPlaylistIdx);
    auto levelsView = FindComponent<GlobalNamespace::LevelCollectionTableView*>();
    // checks for and finds song itself
    if(state.selectedSong)
        levelsView->SelectLevel(state.selectedSong);
    else
        levelsView->SelectLevelPackHeaderCell();
}

void ReloadSongsKeepingSelection(std::function<void()> finishCallback) {
    auto state = GetSelectionState();
    auto callback = [state, finishCallback](std::vector<GlobalNamespace::CustomPreviewBeatmapLevel*> const& _) {
        SetSelectionState(state);
        if(finishCallback)
            finishCallback();
    };
    RuntimeSongLoader::API::RefreshSongs(false, callback);
}

void ReloadPlaylistsKeepingSelection() {
    auto state = GetSelectionState();
    ReloadPlaylists();
    SetSelectionState(state);
}
