#include "Main.hpp"
#include "Types/LevelButtons.hpp"
#include "Types/PlaylistMenu.hpp"
#include "Icons.hpp"

#include "playlistcore/shared/PlaylistCore.hpp"
#include "playlistcore/shared/CustomTypes/CoverTableCell.hpp"
#include "playlistcore/shared/ResettableStaticPtr.hpp"
#include "playlistcore/shared/Utils.hpp"
#include "questui/shared/BeatSaberUI.hpp"

#include "songloader/shared/API.hpp"

#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSegmentedControlController.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/CustomPreviewBeatmapLevel.hpp"

#include "HMUI/ScrollView.hpp"
#include "HMUI/TextSegmentedControl.hpp"

#include "UnityEngine/Resources.hpp"

#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "System/Tuple_2.hpp"

using namespace PlaylistManager;
using namespace PlaylistCore;
using namespace PlaylistCore::Utils;
using namespace QuestUI;

ButtonsContainer* ButtonsContainer::buttonsInstance = nullptr;

#pragma region uiFunctions
void ButtonsContainer::saveCoverButtonPressed() {
    // get cover image (blocking async, yay)
    auto levelData = reinterpret_cast<GlobalNamespace::IPreviewBeatmapLevel*>(levelDetailView->level);
    auto sprite = levelData->GetCoverImageAsync(System::Threading::CancellationToken::get_None())->get_Result();
    if(!sprite)
        return;
    // copy sprite texture for downscaling
    auto newTexture = UnityEngine::Texture2D::New_ctor(sprite->get_texture()->get_width(), sprite->get_texture()->get_height());
    newTexture->SetPixels(sprite->get_texture()->GetPixels());
    // save to file
    ProcessImage(newTexture, false);
    WriteImageToFile(GetCoversPath() + "/" + levelData->get_songName().operator std::string() + "_cover.png", newTexture);
    // reload from file
    LoadCoverImages();
    // show modal for feedback
    infoModal->Show(true, true, nullptr);
    infoModal->get_transform()->set_localPosition({40, -10, 0});
}

void ButtonsContainer::addToPlaylistButtonPressed() {
    // ensure playlists are accurate
    RefreshPlaylists();
    playlistAddModal->Show(true, true, nullptr);
    // needed because of parenting shenanigans or something
    playlistAddModal->get_transform()->set_localPosition({40, -10, 0});
}

void ButtonsContainer::removeFromPlaylistButtonPressed() {
    removeModal->Show(true, true, nullptr);
    removeModal->get_transform()->set_localPosition({40, -10, 0});
}

void ButtonsContainer::playlistSelected(int listCellIdx) {
    // add to playlist
    auto& playlist = loadedPlaylists[listCellIdx];
    AddSongToPlaylist(playlist, currentLevel);
    // set cells deselected
    playlistCovers->tableView->ClearSelection();
    playlistAddModal->Hide(true, nullptr);
    // update playlist table if necessary
    if(playlist == currentPlaylist) {
        // keep selected cell
        int currIndex = levelListTableView->selectedRow;
        // keep scroll position
        float anchorPosY = levelListTableView->tableView->get_contentTransform()->get_anchoredPosition().y;
        auto levelList = currentPlaylist->playlistCS->beatmapLevelCollection->get_beatmapLevels();
        levelListTableView->SetData(levelList, levelListTableView->favoriteLevelIds, false);
        levelListTableView->tableView->SelectCellWithIdx(currIndex, true);
        levelListTableView->tableView->scrollView->ScrollTo(anchorPosY, false);
    }
}

void ButtonsContainer::scrollListLeftButtonPressed() {
    CustomListSource::ScrollListLeft(playlistCovers, 4);
}

void ButtonsContainer::scrollListRightButtonPressed() {
    CustomListSource::ScrollListRight(playlistCovers, 4);
}

void ButtonsContainer::confirmRemovalButtonPressed() {
    removeModal->Hide(true, nullptr);
    if(deleteSongOnRemoval) {
        RemoveSongFromAllPlaylists(currentLevel);
        std::string path = ((GlobalNamespace::CustomPreviewBeatmapLevel*) currentLevel)->customLevelPath;
        auto state = GetSelectionState();
        RuntimeSongLoader::API::DeleteSong(path, [state] {
            RuntimeSongLoader::API::RefreshSongs(true, [state](std::vector<GlobalNamespace::CustomPreviewBeatmapLevel*> const& _) {
                SetSelectionState(state);
            });
        });
    } else {
        RemoveSongFromPlaylist(currentPlaylist, currentLevel);
        // keep scroll position
        float anchorPosY = levelListTableView->tableView->get_contentTransform()->get_anchoredPosition().y;
        auto levelList = currentPlaylist->playlistCS->beatmapLevelCollection->get_beatmapLevels();
        levelListTableView->SetData(levelList, levelListTableView->favoriteLevelIds, false);
        // clear selection since the selected song was just deleted
        levelListTableView->tableView->SelectCellWithIdx(0, true);
        levelListTableView->tableView->scrollView->ScrollTo(anchorPosY, false);
    }
}

void ButtonsContainer::cancelRemovalButtonPressed() {
    removeModal->Hide(true, nullptr);
}

void ButtonsContainer::deleteSongToggleToggled(bool enabled) {
    deleteSongOnRemoval = enabled;
}

void ButtonsContainer::moveUpButtonPressed() {
    // get song index
    int currIndex = levelListTableView->selectedRow;
    if(currIndex <= 1)
        return;
    // move song, offset currIndex by header cell
    SetSongIndex(currentPlaylist, currentLevel, currIndex - 2);
    // set level list and cheat with modifying selected cell idx to avoid "reselecting" the song cell
    auto levelList = currentPlaylist->playlistCS->beatmapLevelCollection->get_beatmapLevels();
    levelListTableView->previewBeatmapLevels = levelList;
    levelListTableView->tableView->selectedCellIdxs->Remove(currIndex);
    levelListTableView->tableView->selectedCellIdxs->Add(currIndex - 1);
    levelListTableView->tableView->ReloadDataKeepingPosition();
    // have to update this selected row manually, since it isn't being updated by a callback
    levelListTableView->selectedRow = currIndex - 1;
    // scroll down if moving level close to out of visible cells
    auto tuple = levelListTableView->tableView->GetVisibleCellsIdRange();
    if(currIndex - 2 <= tuple->get_Item1()) {
        float cellPosY = (currIndex - 2) * levelListTableView->CellSize();
        levelListTableView->tableView->scrollView->ScrollTo(cellPosY, true);
    }
}

void ButtonsContainer::moveDownButtonPressed() {
    // get song index
    int currIndex = levelListTableView->selectedRow;
    if(currIndex >= levelListTableView->NumberOfCells() - 1)
        return;
    // move song, offset currIndex by header cell
    SetSongIndex(currentPlaylist, currentLevel, currIndex);
    // set level list and cheat with modifying selected cell idx to avoid "reselecting" the song cell
    auto levelList = currentPlaylist->playlistCS->beatmapLevelCollection->get_beatmapLevels();
    levelListTableView->previewBeatmapLevels = levelList;
    levelListTableView->tableView->selectedCellIdxs->Remove(currIndex);
    levelListTableView->tableView->selectedCellIdxs->Add(currIndex + 1);
    levelListTableView->tableView->ReloadDataKeepingPosition();
    // have to update this selected row manually, since it isn't being updated by a callback
    levelListTableView->selectedRow = currIndex + 1;
    // scroll down if moving level close to out of visible cells
    auto tuple = levelListTableView->tableView->GetVisibleCellsIdRange();
    if(currIndex + 3 > tuple->get_Item2()) {
        float cellPosY = (currIndex + 3) * levelListTableView->CellSize();
        // offset by scroll page size since cell is at the bottom of the page
        cellPosY -= levelListTableView->tableView->scrollView->get_scrollPageSize();
        levelListTableView->tableView->scrollView->ScrollTo(cellPosY, true);
    }
}
#pragma endregion

custom_types::Helpers::Coroutine ButtonsContainer::initCoroutine() {
    auto levelDetailTransform = levelDetailView->get_transform();

    #pragma region buttons
    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(levelDetailTransform);
    layoutObject = horizontal->get_gameObject();
    horizontal->set_childScaleWidth(false);
    horizontal->set_childControlWidth(false);
    horizontal->set_childForceExpandWidth(false);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleRight);
    horizontal->GetComponent<UnityEngine::RectTransform*>()->set_anchoredPosition({-1, 17});

    saveCoverButton = BeatSaberUI::CreateUIButton(horizontal->get_transform(), "", "SettingsButton", {0, 0}, {5, 5}, [this](){
        saveCoverButtonPressed();
    });
    ((UnityEngine::RectTransform*) saveCoverButton->get_transform()->GetChild(0))->set_sizeDelta({5, 5});
    saveCoverButton->GetComponentInChildren<HMUI::ImageView*>()->skew = 0.18;
    BeatSaberUI::SetButtonSprites(saveCoverButton, SaveCoverInactiveSprite(), SaveCoverSprite());
    BeatSaberUI::AddHoverHint(saveCoverButton, "Save the cover image of the song for use as a playlist cover");

    playlistAddButton = BeatSaberUI::CreateUIButton(horizontal->get_transform(), "", "SettingsButton", {0, 0}, {5, 5}, [this](){
        addToPlaylistButtonPressed();
    });
    ((UnityEngine::RectTransform*) playlistAddButton->get_transform()->GetChild(0))->set_sizeDelta({5, 5});
    playlistAddButton->GetComponentInChildren<HMUI::ImageView*>()->skew = 0.18;
    BeatSaberUI::SetButtonSprites(playlistAddButton, AddToPlaylistInactiveSprite(), AddToPlaylistSprite());
    BeatSaberUI::AddHoverHint(playlistAddButton, "Add this song to a playlist");

    playlistRemoveButton = BeatSaberUI::CreateUIButton(horizontal->get_transform(), "", "SettingsButton", {0, 0}, {5, 5}, [this](){
        removeFromPlaylistButtonPressed();
    });
    ((UnityEngine::RectTransform*) playlistRemoveButton->get_transform()->GetChild(0))->set_sizeDelta({5, 5});
    playlistRemoveButton->GetComponentInChildren<HMUI::ImageView*>()->skew = 0.18;
    BeatSaberUI::SetButtonSprites(playlistRemoveButton, RemoveFromPlaylistInactiveSprite(), RemoveFromPlaylistSprite());
    BeatSaberUI::AddHoverHint(playlistRemoveButton, "Remove this song from this playlist");

    saveCoverButton->get_gameObject()->SetActive(false);
    playlistAddButton->get_gameObject()->SetActive(false);
    playlistRemoveButton->get_gameObject()->SetActive(false);
    co_yield nullptr;

    movementButtonsContainer = BeatSaberUI::CreateCanvas();
    movementButtonsContainer->get_transform()->SetParent(levelDetailTransform, false);
    // local scale is funky on canvases because they're intended to be used unparented
    movementButtonsContainer->get_transform()->set_localScale({1, 1, 1});

    auto moveUpButton = BeatSaberUI::CreateUIButton(movementButtonsContainer->get_transform(), "", "SettingsButton", {-36, 24}, {5, 5}, [this]() {
        moveUpButtonPressed();
    });
    ((UnityEngine::RectTransform*) moveUpButton->get_transform()->GetChild(0))->set_sizeDelta({5, 5});
    moveUpButton->GetComponentInChildren<HMUI::ImageView*>()->skew = 0.18;
    BeatSaberUI::SetButtonSprites(moveUpButton, UpButtonInactiveSprite(), UpButtonSprite());
    BeatSaberUI::AddHoverHint(moveUpButton, "Move this song up in the order of songs in this playlist");

    auto moveDownButton = BeatSaberUI::CreateUIButton(movementButtonsContainer->get_transform(), "", "SettingsButton", {-37, 19}, {5, 5}, [this]() {
        moveDownButtonPressed();
    });
    ((UnityEngine::RectTransform*) moveDownButton->get_transform()->GetChild(0))->set_sizeDelta({5, 5});
    moveDownButton->GetComponentInChildren<HMUI::ImageView*>()->skew = 0.18;
    BeatSaberUI::SetButtonSprites(moveDownButton, DownButtonInactiveSprite(), DownButtonSprite());
    BeatSaberUI::AddHoverHint(moveDownButton, "Move this song down in the order of songs in this playlist");

    movementButtonsContainer->SetActive(false);
    #pragma endregion

    co_yield nullptr;

    #pragma region modals
    playlistAddModal = BeatSaberUI::CreateModal(levelDetailTransform, {83, 17}, nullptr);

    playlistCovers = BeatSaberUI::CreateCustomSourceList<CustomListSource*>(playlistAddModal->get_transform(), {70, 15}, [this](int cellIdx){
        playlistSelected(cellIdx);
    });
    playlistCovers->setType(csTypeOf(CoverTableCell*));
    playlistCovers->tableView->tableType = HMUI::TableView::TableType::Horizontal;
    playlistCovers->tableView->scrollView->scrollViewDirection = HMUI::ScrollView::ScrollViewDirection::Horizontal;
    playlistCovers->tableView->LazyInit();
    
    RefreshPlaylists();

    co_yield nullptr;
    
    // scroll arrows
    auto left = BeatSaberUI::CreateUIButton(playlistAddModal->get_transform(), "", "SettingsButton", {-38, 0}, {8, 8}, [this](){
        scrollListLeftButtonPressed();
    });
    ((UnityEngine::RectTransform*) left->get_transform()->GetChild(0))->set_sizeDelta({8, 8});
    BeatSaberUI::SetButtonSprites(left, LeftCaratInactiveSprite(), LeftCaratSprite());

    auto right = BeatSaberUI::CreateUIButton(playlistAddModal->get_transform(), "", "SettingsButton", {38, 0}, {8, 8}, [this](){
        scrollListRightButtonPressed();
    });
    ((UnityEngine::RectTransform*) right->get_transform()->GetChild(0))->set_sizeDelta({8, 8});
    BeatSaberUI::SetButtonSprites(right, RightCaratInactiveSprite(), RightCaratSprite());

    infoModal = BeatSaberUI::CreateModal(levelDetailTransform, {40, 15}, nullptr);
    auto infoText = BeatSaberUI::CreateText(infoModal, "Song cover image saved!");
    infoText->set_alignment(TMPro::TextAlignmentOptions::Center);

    co_yield nullptr;

    removeModal = BeatSaberUI::CreateModal(levelDetailTransform, {50, 32}, nullptr);
    
    // janky raw newline because I don't want to add a layout element
    auto removeText = BeatSaberUI::CreateText(removeModal, "Do you really want to remove this\nsong from the playlist?", false, UnityEngine::Vector2(0, 9));
    removeText->set_alignment(TMPro::TextAlignmentOptions::Center);

    static ConstString contentName("Content");
    auto confirmRemovalButton = BeatSaberUI::CreateUIButton(removeModal->get_transform(), "Remove", "ActionButton", {-11.5, -2}, {20, 10}, [this] {
        confirmRemovalButtonPressed();
    });
    UnityEngine::Object::Destroy(confirmRemovalButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    auto cancelRemovalButton = BeatSaberUI::CreateUIButton(removeModal->get_transform(), "Cancel", {11.5, -2}, {20, 10}, [this] {
        cancelRemovalButtonPressed();
    });
    UnityEngine::Object::Destroy(cancelRemovalButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    cancelRemovalButton->get_gameObject()->SetActive(true);

    auto deleteToggle = BeatSaberUI::CreateToggle(removeModal->get_transform(), "Delete Song", deleteSongOnRemoval, {0, -27}, [this](bool enabled) {
        deleteSongToggleToggled(enabled);
    });
    // the toggle really isn't a fan of going this thin
    auto parent = deleteToggle->get_transform()->GetParent();
    parent->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(30);
    auto nameText = parent->Find("NameText")->GetComponent<TMPro::TextMeshProUGUI*>();
    nameText->set_enableAutoSizing(false);
    ((UnityEngine::RectTransform*) parent)->set_sizeDelta({-8, 0});
    #pragma endregion

    hasConstructed = true;
    SetVisible(visibleOnFinish, inPlaylistOnFinish, isWIPOnFinish);

    co_return;
}

void ButtonsContainer::Init(GlobalNamespace::StandardLevelDetailView* detailView) {
    // get constant references
    levelDetailView = detailView;
    levelListTableView = FindComponent<GlobalNamespace::LevelCollectionTableView*>();
    
    GlobalNamespace::SharedCoroutineStarter::get_instance()->StartCoroutine(
        custom_types::Helpers::CoroutineHelper::New(initCoroutine()));
}

void ButtonsContainer::SetVisible(bool visible, bool inPlaylist, bool WIP) {
    if(!hasConstructed) {
        visibleOnFinish = visible;
        inPlaylistOnFinish = inPlaylist;
        isWIPOnFinish = WIP;
        return;
    }
    if(saveCoverButton)
        saveCoverButton->get_gameObject()->SetActive(visible);
    if(playlistAddButton)
        playlistAddButton->get_gameObject()->SetActive(visible && !WIP);
    if(playlistRemoveButton)
        playlistRemoveButton->get_gameObject()->SetActive(visible && inPlaylist);
    if(movementButtonsContainer)
        movementButtonsContainer->SetActive(visible && inPlaylist);
    if(playlistAddModal)
        playlistAddModal->Hide(false, nullptr);
    if(infoModal)
        infoModal->Hide(false, nullptr);
}

void ButtonsContainer::SetLevel(GlobalNamespace::IPreviewBeatmapLevel* level) {
    currentLevel = level;
    if(playlistAddModal)
        playlistAddModal->Hide(false, nullptr);
    if(infoModal)
        infoModal->Hide(false, nullptr);
}

void ButtonsContainer::SetPlaylist(Playlist* playlist) {
    currentPlaylist = playlist;
    if(playlistAddModal)
        playlistAddModal->Hide(false, nullptr);
    if(infoModal)
        infoModal->Hide(false, nullptr);
}

void ButtonsContainer::RefreshPlaylists() {
    if(!playlistCovers)
        return;
    loadedPlaylists = GetLoadedPlaylists();
    std::vector<UnityEngine::Sprite*> newCovers;
    std::vector<std::string> newHovers;
    for(auto& playlist : loadedPlaylists) {
        newCovers.emplace_back(GetCoverImage(playlist));
        newHovers.emplace_back(playlist->name);
    }
    playlistCovers->replaceSprites(newCovers);
    playlistCovers->replaceTexts(newHovers);
    playlistCovers->tableView->ReloadDataKeepingPosition();
}

void ButtonsContainer::RefreshHighlightedDifficulties() {
    // get difficulty display object
    auto segmentedController = levelDetailView->beatmapDifficultySegmentedControlController;
    auto cells = segmentedController->difficultySegmentedControl->cells;
    // reset all text colors
    for(int i = 0; i < cells->get_Count(); i++) {
        auto text = cells->get_Item(i)->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
        if(!text)
            continue;
        text->set_faceColor({255, 255, 255, 255});
    }
    // get selected characteristic
    auto selected = levelDetailView->beatmapCharacteristicSegmentedControlController->selectedBeatmapCharacteristic;
    // might not be selected at first
    if(!selected)
        return;
    std::string characteristic = selected->serializedName;
    LOWER(characteristic);
    // get current song difficulties
    if(!currentPlaylist)
        return;
    auto& songs = currentPlaylist->playlistJSON.Songs;
    auto currentHash = GetLevelHash(currentLevel);
    std::vector<Difficulty> difficulties;
    for(auto& song : songs) {
        LOWER(song.Hash);
        if(song.Hash == currentHash && song.Difficulties.has_value()) {
            difficulties = song.Difficulties.value();
            break;
        }
    }
    if(difficulties.size() == 0)
        return;
    // keep set of indices to be highlighted so the rest can be unhighlighted
    std::set<int> highlights;
    // set highlights based on found difficulties
    for(auto& difficulty : difficulties) {
        LOWER(difficulty.Characteristic);
        if(difficulty.Characteristic == characteristic) {
            // attempt to find difficulty
            LOWER(difficulty.Name);
            int diff = -1;
            static const std::vector<std::string> diffNames = {"easy", "normal", "hard", "expert", "expertPlus"};
            for(int i = 0; i < diffNames.size(); i++) {
                if(diffNames[i] == difficulty.Name) {
                    diff = i;
                    break;
                }
            } if(diff < 0)
                continue;
            // get closest index for pc parity
            auto text = cells->get_Item(segmentedController->GetClosestDifficultyIndex(diff))->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
            if(text)
                text->set_faceColor({255, 255, 0, 255});
        }
    }
}

void ButtonsContainer::Destroy() {
    ButtonsContainer::buttonsInstance = nullptr;
    UnityEngine::Object::Destroy(layoutObject);
    UnityEngine::Object::Destroy(movementButtonsContainer);
    // assumes it's always allocated with new
    delete this;
}
