#include "Main.hpp"
#include "Types/Config.hpp"
#include "Types/PlaylistMenu.hpp"
#include "Types/LevelButtons.hpp"
#include "Types/CustomListSource.hpp"
#include "Types/CoverTableCell.hpp"
#include "PlaylistManager.hpp"
#include "ResettableStaticPtr.hpp"
#include "Icons.hpp"
#include "Utils.hpp"

#include "questui/shared/BeatSaberUI.hpp"
#include "questui/shared/CustomTypes/Components/Backgroundable.hpp"

#include "songloader/shared/API.hpp"

#include "UnityEngine/Mathf.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/UI/Mask.hpp"

#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/UnityWebRequest_UnityWebRequestError.hpp"
#include "UnityEngine/Networking/DownloadHandler.hpp"

#include "HMUI/CurvedTextMeshPro.hpp"
#include "HMUI/UIKeyboard.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/TableView_ScrollPositionType.hpp"

#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/LevelPackHeaderTableCell.hpp"
#include "GlobalNamespace/BeatmapLevelPackCollection.hpp"
#include "GlobalNamespace/SharedCoroutineStarter.hpp"

#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/Action_1.hpp"

#include <math.h>

using namespace PlaylistManager;
using namespace QuestUI;
using namespace GlobalNamespace;

DEFINE_TYPE(PlaylistManager, PlaylistMenu);

#define ANCHOR(component, xmin, ymin, xmax, ymax) auto component##_rect = (UnityEngine::RectTransform*) component->get_transform(); \
component##_rect->set_anchorMin({xmin, ymin}); \
component##_rect->set_anchorMax({xmax, ymax});

std::function<void()> PlaylistMenu::nextCloseKeyboard = nullptr;
PlaylistMenu* PlaylistMenu::menuInstance = nullptr;

UnityEngine::GameObject* anchorContainer(UnityEngine::Transform* parent, float xmin, float ymin, float xmax, float ymax) {
    static ConstString name("BPContainer");
    auto go = UnityEngine::GameObject::New_ctor(name);
    go->AddComponent<UnityEngine::UI::ContentSizeFitter*>();
    
    auto rect = go->GetComponent<UnityEngine::RectTransform*>();
    rect->SetParent(parent, false);
    rect->set_anchorMin({xmin, ymin});
    rect->set_anchorMax({xmax, ymax});
    rect->set_sizeDelta({0, 0});

    go->AddComponent<UnityEngine::UI::LayoutElement*>();
    return go;
}

UnityEngine::UI::Button* anchorMiniButton(UnityEngine::Transform* parent, std::string_view buttonText, std::string_view buttonTemplate, std::function<void()> onClick, float x, float y) {
    auto button = BeatSaberUI::CreateUIButton(parent, buttonText, buttonTemplate, {0, 0}, {8, 8}, onClick);

    static ConstString contentName("Content");

    UnityEngine::GameObject::Destroy(button->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    button->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_margin({-0.8, 0});

    ANCHOR(button, x, y, x, y);

    return button;
}

float movementEasing(float p) {
    // ease in out quartic
    if (p < 0.5f) {
        return 8 * p * p * p * p;
    } else {
        float f = p - 1;
        return (-8 * f * f * f * f) + 1;
    }
}

custom_types::Helpers::Coroutine PlaylistMenu::moveCoroutine(bool reversed) {
    // no overlap
    if(inMovement)
        co_return;
    inMovement = true;
    float time = 0;
    static const float duration = 0.4;
    static const double pi = 3.141592653589793;
    if(!reversed) {
        // moving into view
        detailsContainer->set_active(true);
        detailsVisible = true;
    }
    auto transf = (UnityEngine::RectTransform*) detailsContainer->get_transform();
    float minY = transf->get_anchorMin().y;
    float maxY = transf->get_anchorMax().y;
    while(time < 1) {
        time += UnityEngine::Time::get_deltaTime()/duration;
        float val = movementEasing(time);
        if(!reversed) {
            transf->set_anchorMin({1 - val, minY});
            transf->set_anchorMax({2 - val, maxY});
        } else {
            transf->set_anchorMin({val, minY});
            transf->set_anchorMax({val + 1, maxY});
        }
        co_yield nullptr;
    }
    if(reversed) {
        // moved out of view
        detailsContainer->set_active(false);
        detailsVisible = false;
    }
    inMovement = false;
    co_return;
}

custom_types::Helpers::Coroutine PlaylistMenu::refreshCoroutine() {
    // don't mess with other animations
    if(inMovement)
        co_return;
    // don't close if already closed
    if(detailsVisible)
        co_yield (System::Collections::IEnumerator*) StartCoroutine(custom_types::Helpers::CoroutineHelper::New(moveCoroutine(true)));
    updateDetailsMode();
    co_yield (System::Collections::IEnumerator*) StartCoroutine(custom_types::Helpers::CoroutineHelper::New(moveCoroutine(false)));
    co_return;
}

custom_types::Helpers::Coroutine PlaylistMenu::syncCoroutine() {
    auto& customData = playlist->playlistJSON.CustomData;
    if(!customData.has_value())
        co_return;
    auto& syncUrl = customData->SyncURL;
    if(!syncUrl.has_value())
        co_return;
    // get before co_yields to avoid a different playlist being selected
    auto syncingPlaylist = playlist;
    // probably best not to sync two playlists at once
    if(awaitingSync)
        co_return;
    awaitingSync = true;
    static ConstString syncText("Syncing playlist...");
    syncingText->set_text(syncText);
    ((UnityEngine::RectTransform*) syncingModal->get_transform())->set_anchoredPosition({-7, 0});
    syncingModal->Show(true, false, nullptr);
    auto webRequest = UnityEngine::Networking::UnityWebRequest::Get(syncUrl.value());

    co_yield (System::Collections::IEnumerator*) webRequest->SendWebRequest();

    if(webRequest->GetError() != UnityEngine::Networking::UnityWebRequest::UnityWebRequestError::OK) {
        LOG_ERROR("Sync request failed! Error: %s", webRequest->GetWebErrorString(webRequest->GetError()).operator std::string().c_str());
        awaitingSync = false;
        co_return;
    }

    // save synced playlist
    std::string text = webRequest->get_downloadHandler()->GetText();
    writefile(syncingPlaylist->path, text);
    // full reload the specific playlist only
    MarkPlaylistForReload(syncingPlaylist);
    // reload playlists but keep selection
    ReloadPlaylistsKeepingSelection();
    // check for missing songs, download then reload again if so
    if(PlaylistHasMissingSongs(syncingPlaylist)) {
        static ConstString downloadText("Downloading missing songs...");
        syncingText->set_text(downloadText);
        bool downloaded = false;
        DownloadMissingSongsFromPlaylist(syncingPlaylist, [&downloaded] {
            downloaded = true;
        });
        // wait for downloads
        while(!downloaded)
            co_yield nullptr;
        // full reload the specific playlist only
        MarkPlaylistForReload(syncingPlaylist);
        bool doneRefreshing = false;
        ReloadSongsKeepingSelection([&doneRefreshing] {
            doneRefreshing = true;
        });
        // wait for songs to refresh
        while(!doneRefreshing)
            co_yield nullptr;
        // clears any songs that could not be downloaded
        if(playlistConfig.RemoveMissing) {
            RemoveMissingSongsFromPlaylist(syncingPlaylist);
            // sets the download icons based on after songs have or haven't been removed
            gameTableView->RefreshAvailability();
        }
    }
    awaitingSync = false;
    syncingModal->Hide(true, nullptr);

    co_return;
}

#pragma region uiFunctions
void PlaylistMenu::infoButtonPressed() {
    if(inMovement)
        return;
    // toggle mode if open to viewing details
    if(!addingPlaylist) {
        // reset texts to current values for playlist when opening
        if(!detailsVisible)
            updateDetailsMode();
        showDetails(!detailsVisible);
    } else {
        addingPlaylist = false;
        refreshDetails();
    }
}

void PlaylistMenu::syncButtonPressed() {
    SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(syncCoroutine()));
}

void PlaylistMenu::downloadButtonPressed() {
    if(awaitingSync)
        return;
    awaitingSync = true;
    static ConstString downloadText("Downloading missing songs...");
    syncingText->set_text(downloadText);
    ((UnityEngine::RectTransform*) syncingModal->get_transform())->set_anchoredPosition({-7, 0});
    syncingModal->Show(true, false, nullptr);
    auto downloadingPlaylist = playlist;
    DownloadMissingSongsFromPlaylist(downloadingPlaylist, [this, downloadingPlaylist] {
        MarkPlaylistForReload(downloadingPlaylist);
        ReloadSongsKeepingSelection([this, downloadingPlaylist] {
            awaitingSync = false;
            syncingModal->Hide(true, nullptr);
            // clears any songs that could not be downloaded
            if(playlistConfig.RemoveMissing) {
                RemoveMissingSongsFromPlaylist(downloadingPlaylist);
                // sets the download icons based on after songs have or haven't been removed
                gameTableView->RefreshAvailability();
            }
        });
    });
}

void PlaylistMenu::addButtonPressed() {
    if(inMovement)
        return;
    // do nothing if already open for adding a playlist
    if(addingPlaylist && !detailsVisible) {
        // also reset on reopen
        updateDetailsMode();
        showDetails(true);
    } else if(!addingPlaylist) {
        addingPlaylist = true;
        refreshDetails();
    }
}

void PlaylistMenu::moveRightButtonPressed() {
    namespace Generic = System::Collections::Generic;
    // get current playlist and next playlist
    int oldCellIdx = gameTableView->selectedCellIndex;
    // last playlist, can't move further right
    if(oldCellIdx == gameTableView->GetNumberOfCells() - 1)
        return;
    // get list of all custom packs currently in the table view
    auto packList = GetCustomPacks();
    auto nextPlaylist = GetPlaylistWithPrefix(packList->get_Item(oldCellIdx + 1)->get_packID());
    if(!nextPlaylist)
        return;
    // get config index at the previous playlist (so immediately after it if moved)
    int newConfigIdx = GetPlaylistIndex(nextPlaylist->path);
    // move playlist in config
    MovePlaylist(playlist, newConfigIdx);
    // move playlist in table
    auto movedCollection = packList->get_Item(oldCellIdx);
    packList->RemoveAt(oldCellIdx);
    packList->Insert(oldCellIdx + 1, movedCollection);
    // update without reloading
    SetCustomPacks(packList, false);
    gameTableView->SelectAndScrollToCellWithIdx(oldCellIdx + 1);
}

void PlaylistMenu::moveLeftButtonPressed() {
    namespace Generic = System::Collections::Generic;
    // get current playlist and next playlist
    int oldCellIdx = gameTableView->selectedCellIndex;
    // last playlist, can't move further right
    if(oldCellIdx == 0)
        return;
    // get list of all custom packs currently in the table view
    auto packList = GetCustomPacks();
    auto prevPlaylist = GetPlaylistWithPrefix(packList->get_Item(oldCellIdx - 1)->get_packID());
    if(!prevPlaylist)
        return;
    // get config index at the previous playlist (so immediately before it if moved)
    int newConfigIdx = GetPlaylistIndex(prevPlaylist->path);
    // move playlist in config
    MovePlaylist(playlist, newConfigIdx);
    // move playlist in table
    auto movedCollection = packList->get_Item(oldCellIdx);
    packList->RemoveAt(oldCellIdx);
    packList->Insert(oldCellIdx - 1, movedCollection);
    // update without reloading
    SetCustomPacks(packList, false);
    gameTableView->SelectAndScrollToCellWithIdx(oldCellIdx - 1);
}

void PlaylistMenu::playlistTitleTyped(std::string newValue) {
    currentTitle = newValue.data();
    if(!PlaylistMenu::nextCloseKeyboard) {
        PlaylistMenu::nextCloseKeyboard = [this](){
            // don't change anything with the current playlist if we aren't editing it
            if(addingPlaylist)
                return;
            LOG_INFO("Title set to %s", currentTitle.c_str());
            RenamePlaylist(playlist, currentTitle);
            // get header cell and set text
            auto tableView = FindComponent<LevelCollectionTableView*>();
            if(!tableView->showLevelPackHeader || tableView->NumberOfCells() == 0)
                return;
            tableView->headerText = currentTitle;
            tableView->tableView->RefreshCells(true, true);
        };
    }
    // title cleared (x button)
    if(!playlistTitle->hasKeyboardAssigned && System::String::IsNullOrEmpty(playlistTitle->get_text())) {
        PlaylistMenu::nextCloseKeyboard();
        PlaylistMenu::nextCloseKeyboard = nullptr;
    }
}

void PlaylistMenu::playlistAuthorTyped(std::string newValue) {
    currentAuthor = newValue.data();
    if(!PlaylistMenu::nextCloseKeyboard) {
        PlaylistMenu::nextCloseKeyboard = [this](){
            // don't change anything with the current playlist if we aren't editing it
            if(addingPlaylist)
                return;
            LOG_INFO("Author set to %s", currentAuthor.c_str());
            playlist->playlistJSON.PlaylistAuthor = currentAuthor;
            WriteToFile(playlist->path, playlist->playlistJSON);
        };
    }
    // author cleared (x button)
    if(!playlistAuthor->hasKeyboardAssigned && System::String::IsNullOrEmpty(playlistAuthor->get_text())) {
        PlaylistMenu::nextCloseKeyboard();
        PlaylistMenu::nextCloseKeyboard = nullptr;
    }
}

void PlaylistMenu::coverButtonPressed() {
    RefreshCovers();
    coverModal->Show(true, false, nullptr);
    // have correct cover selected and shown
    auto index = addingPlaylist ? coverImageIndex + 1 : playlist->imageIndex + 1;
    list->tableView->SelectCellWithIdx(index, false);
    list->tableView->ScrollToCellWithIdx(index, HMUI::TableView::ScrollPositionType::Center, false);
}

void PlaylistMenu::deleteButtonPressed() {
    if(confirmModal)
        confirmModal->Show(true, false, nullptr);
}

void PlaylistMenu::createButtonPressed() {
    // create new playlist based on fields
    std::string path = AddPlaylist(currentTitle, currentAuthor, coverImageIndex >= 0 ? coverImage->get_sprite() : nullptr);
    // add to current folder if inside one
    if(filterSelectionState == 3 && currentFolder && !currentFolder->HasSubfolders)
        currentFolder->Playlists.emplace_back(path);
    ReloadPlaylists();
}

void PlaylistMenu::cancelButtonPressed() {
    showDetails(false);
}

void PlaylistMenu::confirmDeleteButtonPressed() {
    confirmModal->Hide(true, nullptr);
    // delete playlist
    DeletePlaylist(playlist);
    playlist = nullptr;
    ReloadPlaylists();
}

void PlaylistMenu::cancelDeleteButtonPressed() {
    confirmModal->Hide(true, nullptr);
}

void PlaylistMenu::coverSelected(int listCellIdx) {
    auto sprite = list->getSprite(listCellIdx);
    // don't set playlist cover when adding a new one
    if(!addingPlaylist) {
        // change in list and json
        ChangePlaylistCover(playlist, listCellIdx - 1);
        // change background pack image
        packImage->set_sprite(sprite);
        // update image in playlist bar
        gameTableView->gridView->ReloadData();
    }
    coverImage->set_sprite(sprite);
    // one less because the default image is not counted
    coverImageIndex = listCellIdx - 1;
    coverModal->Hide(true, nullptr);
}

void PlaylistMenu::scrollListLeftButtonPressed() {
    CustomListSource::ScrollListLeft(list, 4);
}

void PlaylistMenu::scrollListRightButtonPressed() {
    CustomListSource::ScrollListRight(list, 4);
}
#pragma endregion

custom_types::Helpers::Coroutine PlaylistMenu::initCoroutine() {
    #pragma region details
    // use pack image area as a mask for details
    auto maskImage = BeatSaberUI::CreateImage(packImage->get_transform(), WhiteSprite(), {0, 0}, {0, 0});
    auto rectTrans = (UnityEngine::RectTransform*) maskImage->get_transform();
    rectTrans->set_anchorMin({0, 0});
    rectTrans->set_anchorMax({1, 1});
    maskImage->set_material(packImage->get_material());
    maskImage->get_gameObject()->AddComponent<UnityEngine::UI::Mask*>()->set_showMaskGraphic(false);

    static ConstString contentName("Content");
    UnityEngine::Color detailsBackgroundColor(0.1, 0.1, 0.1, 0.93);
    // details container
    detailsContainer = anchorContainer(maskImage->get_transform(), 1, 0, 2, 1);
    auto detailsBackground = BeatSaberUI::CreateImage(detailsContainer->get_transform(), WhiteSprite(), {0, 0}, {0, 0});
    ANCHOR(detailsBackground, 0, 0.15, 1, 1);
    detailsBackground->set_color(detailsBackgroundColor);

    playlistTitle = BeatSaberUI::CreateStringSetting(detailsContainer->get_transform(), "Playlist Title", "", [this](StringW newValue){
        playlistTitleTyped(newValue);
    });
    playlistTitle->GetComponent<UnityEngine::RectTransform*>()->set_sizeDelta({19.8, 3});
    playlistTitle->textView->set_overflowMode(TMPro::TextOverflowModes::Ellipsis);
    ANCHOR(playlistTitle, 0.2, 0.89, 0.8, 0.99);

    playlistAuthor = BeatSaberUI::CreateStringSetting(detailsContainer->get_transform(), "Playlist Author", "", {0, 0}, {0, -0.1, 0}, [this](StringW newValue){
        playlistAuthorTyped(newValue);
    });
    playlistAuthor->GetComponent<UnityEngine::RectTransform*>()->set_sizeDelta({19.8, 3});
    playlistAuthor->textView->set_overflowMode(TMPro::TextOverflowModes::Ellipsis);
    ANCHOR(playlistAuthor, 0.2, 0.74, 0.8, 0.84);

    coverButton = BeatSaberUI::CreateUIButton(detailsContainer->get_transform(), "Change Cover", UnityEngine::Vector2{0, 0}, {15, 5}, [this](){
        coverButtonPressed();
    });
    UnityEngine::Object::Destroy(coverButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    ANCHOR(coverButton, 0.17, 0.57, 0.35, 0.64);

    deleteButton = BeatSaberUI::CreateUIButton(detailsContainer->get_transform(), "Delete", UnityEngine::Vector2{0, 0}, {15, 5}, [this](){
        deleteButtonPressed();
    });
    UnityEngine::Object::Destroy(deleteButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    ANCHOR(deleteButton, 0.65, 0.57, 0.73, 0.64);

    // description
    descriptionTitle = BeatSaberUI::CreateText(detailsContainer->get_transform(), "Description");
    ANCHOR(descriptionTitle, 0.565, 0.37, 0.6, 0.42);
    playlistDescription = BeatSaberUI::CreateText(detailsContainer->get_transform(), "", false, {0, 0}, {29, 5});
    playlistDescription->set_enableWordWrapping(true);
    playlistDescription->set_fontSize(3.5);
    playlistDescription->set_overflowMode(TMPro::TextOverflowModes::Ellipsis);
    playlistDescription->set_alignment(TMPro::TextAlignmentOptions::TopLeft);
    ANCHOR(playlistDescription, 0.29, 0.19, 0.7, 0.37);

    // use playlist sprite if playlist has been set
    coverImage = BeatSaberUI::CreateImage(detailsContainer->get_transform(), playlist ? GetCoverImage(playlist) : GetDefaultCoverImage(), {0, 0}, {0, 0});
    // set rounded corner material
    coverImage->set_material(packImage->get_material());
    ANCHOR(coverImage, 0.05, 0.4, 0.3, 0.65);

    createButton = BeatSaberUI::CreateUIButton(detailsContainer->get_transform(), "Create", "ActionButton", {0, 0}, {13, 5}, [this](){
        createButtonPressed();
    });
    UnityEngine::Object::Destroy(createButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    ANCHOR(createButton, 0.17, 0.2, 0.35, 0.27);
    
    cancelButton = BeatSaberUI::CreateUIButton(detailsContainer->get_transform(), "Cancel", UnityEngine::Vector2{0, 0}, {13, 5}, [this](){
        cancelButtonPressed();
    });
    UnityEngine::Object::Destroy(cancelButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    ANCHOR(cancelButton, 0.64, 0.2, 0.84, 0.27);
    
    detailsContainer->set_active(false);
    detailsVisible = false;
    #pragma endregion
    
    co_yield nullptr;

    #pragma region buttonsBar
    buttonsContainer = anchorContainer(maskImage->get_transform(), 0, 0, 1, 0.15);
    auto buttonsBackgroundImage = BeatSaberUI::CreateImage(buttonsContainer->get_transform(), WhiteSprite(), {0, 0}, {0, 0});
    ANCHOR(buttonsBackgroundImage, 0, 0.02, 1, 1);
    buttonsBackgroundImage->set_color(detailsBackgroundColor);

    // side menu buttons, from bottom to top
    auto infoButton = anchorMiniButton(buttonsContainer->get_transform(), "i", "ActionButton", [this](){
        infoButtonPressed();
    }, 0.9, 0.5);
    // disable all caps mode
    infoButton->GetComponentInChildren<HMUI::CurvedTextMeshPro*>()->set_fontStyle(2);
    BeatSaberUI::AddHoverHint(infoButton->get_gameObject(), "Playlist information");
    
    syncButton = anchorMiniButton(buttonsContainer->get_transform(), "", "ActionButton", [this](){
        syncButtonPressed();
    }, 0.74, 0.5);
    auto syncImg = BeatSaberUI::CreateImage(syncButton->get_transform(), SyncSprite(), {0, 0}, {0, 0});
    syncImg->set_preserveAspect(true);
    syncImg->get_transform()->set_localScale({0.55, 0.55, 0.55});
    BeatSaberUI::AddHoverHint(syncButton->get_gameObject(), "Sync playlist");
    bool syncActive = false;
    if(playlist && playlist->playlistJSON.CustomData.has_value())
        syncActive = playlist->playlistJSON.CustomData->SyncURL.has_value();
    syncButton->set_interactable(syncActive);
    
    downloadButton = anchorMiniButton(buttonsContainer->get_transform(), "", "ActionButton", [this](){
        downloadButtonPressed();
    }, 0.58, 0.5);
    auto downloadImg = BeatSaberUI::CreateImage(downloadButton->get_transform(), DownloadSprite(), {0, 0}, {0, 0});
    downloadImg->set_preserveAspect(true);
    downloadImg->get_transform()->set_localScale({0.6, 0.6, 0.6});
    BeatSaberUI::AddHoverHint(downloadButton->get_gameObject(), "Download missing songs");
    downloadButton->set_interactable(playlist && PlaylistHasMissingSongs(playlist));
    
    auto addButton = anchorMiniButton(buttonsContainer->get_transform(), "+", "PracticeButton", [this](){
        addButtonPressed();
    }, 0.42, 0.5);
    BeatSaberUI::AddHoverHint(addButton->get_gameObject(), "Create a new playlist");
    
    auto rightButton = anchorMiniButton(buttonsContainer->get_transform(), ">", "PracticeButton", [this](){
        moveRightButtonPressed();
    }, 0.26, 0.5);
    BeatSaberUI::AddHoverHint(rightButton->get_gameObject(), "Move playlist right");
    
    auto leftButton = anchorMiniButton(buttonsContainer->get_transform(), "<", "PracticeButton", [this](){
        moveLeftButtonPressed();
    }, 0.1, 0.5);
    BeatSaberUI::AddHoverHint(leftButton->get_gameObject(), "Move playlist left");
    
    buttonsContainer->SetActive(visibleOnFinish);
    
    bootstrapContainer = anchorContainer(maskImage->get_transform(), 0, 0, 1, 0.15);
    auto bootstrapBackgroundImage = BeatSaberUI::CreateImage(bootstrapContainer->get_transform(), WhiteSprite(), {0, 0}, {0, 0});
    ANCHOR(bootstrapBackgroundImage, 0, 0.02, 1, 1);
    bootstrapBackgroundImage->set_color(detailsBackgroundColor);
    
    addButton = anchorMiniButton(bootstrapContainer->get_transform(), "+", "PracticeButton", [this](){
        addButtonPressed();
    }, 0.9, 0.5);
    BeatSaberUI::AddHoverHint(addButton->get_gameObject(), "Create a new playlist");

    bootstrapContainer->SetActive(!visibleOnFinish && customOnFinish);
    #pragma endregion

    co_yield nullptr;

    #pragma region modals
    // confirmation modal for deletion
    confirmModal = BeatSaberUI::CreateModal(get_transform(), {43, 25}, {-7, 0}, nullptr);

    auto confirmText = BeatSaberUI::CreateText(confirmModal->get_transform(), "Are you sure you would like to delete this playlist?", false, {0, 0}, {30, 7});
    confirmText->set_enableWordWrapping(true);
    confirmText->set_alignment(TMPro::TextAlignmentOptions::Center);
    ANCHOR(confirmText, 0.4, 0.5, 0.6, 0.9);

    auto yesButton = BeatSaberUI::CreateUIButton(confirmModal->get_transform(), "Delete", "ActionButton", {0, 0}, {7, 5}, [this](){
        confirmDeleteButtonPressed();
    });
    UnityEngine::Object::Destroy(yesButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    ANCHOR(yesButton, 0.17, 0.15, 0.37, 0.35);

    auto noButton = BeatSaberUI::CreateUIButton(confirmModal->get_transform(), "Cancel", UnityEngine::Vector2{0, 0}, {7, 5}, [this](){
        cancelDeleteButtonPressed();
    });
    UnityEngine::Object::Destroy(noButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    ANCHOR(noButton, 0.63, 0.15, 0.83, 0.35);

    // playlist cover changing modal
    coverModal = BeatSaberUI::CreateModal(get_transform(), {83, 17}, {-6, -11}, nullptr);

    list = BeatSaberUI::CreateCustomSourceList<CustomListSource*>(coverModal->get_transform(), {70, 15}, [this](int cellIdx){
        coverSelected(cellIdx);
    });
    list->setType(csTypeOf(CoverTableCell*));
    list->tableView->tableType = HMUI::TableView::TableType::Horizontal;
    list->tableView->scrollView->scrollViewDirection = HMUI::ScrollView::ScrollViewDirection::Horizontal;
    list->tableView->LazyInit();

    co_yield nullptr;

    RefreshCovers();

    co_yield nullptr;

    // scroll arrows
    auto left = BeatSaberUI::CreateUIButton(coverModal->get_transform(), "", "SettingsButton", {-38, 0}, {8, 8}, [this](){
        scrollListLeftButtonPressed();
    });
    ((UnityEngine::RectTransform*) left->get_transform()->GetChild(0))->set_sizeDelta({8, 8});
    BeatSaberUI::SetButtonSprites(left, LeftCaratInactiveSprite(), LeftCaratSprite());

    auto right = BeatSaberUI::CreateUIButton(coverModal->get_transform(), "", "SettingsButton", {38, 0}, {8, 8}, [this](){
        scrollListRightButtonPressed();
    });
    ((UnityEngine::RectTransform*) right->get_transform()->GetChild(0))->set_sizeDelta({8, 8});
    BeatSaberUI::SetButtonSprites(right, RightCaratInactiveSprite(), RightCaratSprite());

    syncingModal = BeatSaberUI::CreateModal(get_transform(), {45, 20}, {-7, 0}, nullptr, false);
    
    syncingText = BeatSaberUI::CreateText(syncingModal->get_transform(), "Syncing Playlist...", false, {0, 0}, {30, 10});
    syncingText->set_alignment(TMPro::TextAlignmentOptions::Center);
    #pragma endregion
    
    hasConstructed = true;
    SetVisible(visibleOnFinish, customOnFinish);

    co_return;
}

void PlaylistMenu::updateDetailsMode() {
    descriptionTitle->get_gameObject()->set_active(!addingPlaylist);
    playlistDescription->get_gameObject()->set_active(!addingPlaylist);
    deleteButton->get_gameObject()->set_active(!addingPlaylist);
    coverImage->get_gameObject()->set_active(addingPlaylist);
    createButton->get_gameObject()->set_active(addingPlaylist);
    cancelButton->get_gameObject()->set_active(addingPlaylist);

    if(!addingPlaylist) {
        playlistTitle->SetText(playlist->name);

        std::string auth = playlist->playlistJSON.PlaylistAuthor ? playlist->playlistJSON.PlaylistAuthor.value() : "";
        playlistAuthor->SetText(auth);

        std::string desc = playlist->playlistJSON.PlaylistDescription ? playlist->playlistJSON.PlaylistDescription.value() : "...";
        playlistDescription->SetText(desc);

        static ConstString changeText("Change Cover");
        ANCHOR(coverButton, 0.17, 0.57, 0.35, 0.64);
        coverButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_text(changeText);
    } else {
        currentTitle = "New Playlist";
        static ConstString title("New Playlist");
        playlistTitle->SetText(title);
        currentAuthor = "Playlist Manager";
        static ConstString author("Playlist Manager");
        playlistAuthor->SetText(author);
        static ConstString empty("");
        playlistDescription->SetText(empty);

        static ConstString selectText("Select Cover");
        ANCHOR(coverButton, 0.55, 0.5, 0.73, 0.57);
        coverButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_text(selectText);
    }
}

void PlaylistMenu::showDetails(bool visible) {
    StartCoroutine(custom_types::Helpers::CoroutineHelper::New(moveCoroutine(!visible)));
}

void PlaylistMenu::refreshDetails() {
    StartCoroutine(custom_types::Helpers::CoroutineHelper::New(refreshCoroutine()));
}

void PlaylistMenu::Init(HMUI::ImageView* imageView) {
    // get table view for setting selected cell
    gameTableView = FindComponent<AnnotatedBeatmapLevelCollectionsGridView*>();
    packImage = imageView;
    // initial variable values
    visibleOnFinish = true;
    customOnFinish = false;
    hasConstructed = false;
    coverImageIndex = -1;
    
    // don't let it get stopped by set visible
    SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(initCoroutine()));
}

void PlaylistMenu::SetPlaylist(Playlist* list) {
    LOG_INFO("Playlist set to %s", list->path.c_str());
    playlist = list;
    // ensure cover is present
    auto cover = GetCoverImage(playlist);
    coverImageIndex = playlist->imageIndex;
    if(coverImage)
        coverImage->set_sprite(cover);
    if(coverModal)
        coverModal->Hide(true, nullptr);
    if(confirmModal)
        confirmModal->Hide(true, nullptr);
    if(downloadButton)
        downloadButton->set_interactable(PlaylistHasMissingSongs(playlist));
    if(syncButton) {
        bool syncActive = false;
        if(playlist->playlistJSON.CustomData.has_value())
            syncActive = playlist->playlistJSON.CustomData->SyncURL.has_value();
        syncButton->set_interactable(syncActive);
    }
}

void PlaylistMenu::RefreshCovers() {
    if(!list)
        return;
    // add cover images and reload
    list->replaceSprites({GetDefaultCoverImage()});
    list->addSprites(GetLoadedImages());
    list->tableView->ReloadDataKeepingPosition();
}

void PlaylistMenu::SetVisible(bool visible, bool custom) {
    if(!hasConstructed) {
        visibleOnFinish = visible;
        customOnFinish = custom;
        return;
    }
    StopAllCoroutines();
    inMovement = false;
    detailsVisible = false;
    if(buttonsContainer)
        buttonsContainer->SetActive(visible);
    if(bootstrapContainer)
        bootstrapContainer->SetActive(!visible && custom);
    if(detailsContainer)
        detailsContainer->SetActive(false);
    if(confirmModal)
        confirmModal->Hide(false, nullptr);
    if(coverModal)
        coverModal->Hide(false, nullptr);
    // ensure the default cover image is shown when creating a playlist from the custom songs pack
    if(!visible && custom) {
        if(coverImage)
            coverImage->set_sprite(GetDefaultCoverImage());
        coverImageIndex = -1;
    }
}

void PlaylistMenu::ShowInfoMenu() {
    infoButtonPressed();
}

void PlaylistMenu::ShowCreateMenu() {
    addButtonPressed();
}

void PlaylistMenu::Destroy() {
    StopAllCoroutines();
    UnityEngine::Object::Destroy(detailsContainer);
    UnityEngine::Object::Destroy(buttonsContainer);
    UnityEngine::Object::Destroy(bootstrapContainer);
    PlaylistMenu::menuInstance = nullptr;
    PlaylistMenu::nextCloseKeyboard = nullptr;
    UnityEngine::Object::Destroy(this);
}
