#include "customtypes/playlistinfo.hpp"

#include "UnityEngine/UI/LayoutRebuilder.hpp"
#include "assets.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "customtypes/playlistsongs.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/delegates.hpp"
#include "metacore/shared/game.hpp"
#include "playlistcore/shared/PlaylistCore.hpp"
#include "utils.hpp"

DEFINE_TYPE(PlaylistManager, PlaylistInfo);

using namespace PlaylistManager;

static constexpr int imagePageSize = 28;

void PlaylistInfo::OnEnable() {
    name = "PlaylistInfo";
    rectTransform->anchorMin = {0.5, 0.5};
    rectTransform->anchorMax = {0.5, 0.5};
    rectTransform->sizeDelta = {75, 80};
}

void PlaylistInfo::OnDisable() {
    if (processingModal)
        processingModal->Hide(false, nullptr);
}

void PlaylistInfo::SetupFields() {
    using Item = HMUI::IconSegmentedControl::DataItem;
    creationButtonData = ListW<Item*>::New(4);
    creationButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::save), "Add playlist", true));
    creationButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::save_edit), "Add and edit playlist", true));
    creationButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::reset), "Reset playlist details", true));
    creationButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::delete), "Cancel addition", true));
    editButtonData = ListW<Item*>::New(6);
    editButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::delete), "Delete playlist", true));
    editButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::clear_highlight), "Clear highlighted difficulties", true));
    editButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::clear_download), "Remove undownloaded songs", true));
    editButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::clear_sync), "Remove sync url", true));
    editButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::download), "Download missing songs", true));
    editButtonData->Add(Item::New_ctor(PNG_SPRITE(icons::sync), "Sync playlist", true));
}

void PlaylistInfo::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (firstActivation) {
        SetupFields();
        BSML_FILE(playlistinfo);
    } else
        Refresh();
}

void PlaylistInfo::PostParse() {
    if (!coverImage || !infoScroll || !nameText || !authorText || !namePlaceholder || !authorPlaceholder || !dummyButton || !dummyImage)
        return;

    coverImage->material = MetaCore::Game::GetCurvedCornersMaterial();

    infoScroll->viewportTransform->sizeDelta = {0, 0};
    infoScroll->add_scrollPositionChangedEvent(MetaCore::Delegates::MakeSystemAction([this](float) { UpdateKeyboardOffsets(); }));

    for (auto& stack : infoScroll->GetComponentsInChildren<HMUI::StackLayoutGroup*>())
        stack->childForceExpandHeight = false;

    nameInput = Utils::CreateInput(nameText, dummyImage, dummyButton, namePlaceholder, {0, -9}, [this](StringW input) { nameTyped(input); });
    authorInput = Utils::CreateInput(
        authorText, dummyImage, dummyButton, authorPlaceholder, {0, -3}, [this](StringW input) { authorTyped(input); }, {0.7, 0.7, 0.7, 1}
    );

    if (!imageGrid || !creationIconControl || !editIconControl)
        return;

    Utils::SetupIcons(creationIconControl);
    Utils::SetupIcons(editIconControl);

    // should these have curved corners?
    for (int i = 0; i < imageGrid->childCount; i++)
        imageGrid->GetChild(i)->GetComponent<BSML::ClickableImage*>()->onClick += {[this, i]() {
            imageClicked(i);
        }};

    RefreshImages();
    UpdateImageButtons();
    UpdateKeyboardOffsets();
    Refresh();
}

void PlaylistInfo::OnDestroy() {
    instance = nullptr;
}

PlaylistInfo* PlaylistInfo::GetInstance() {
    if (!instance)
        instance = BSML::Helpers::CreateViewController<PlaylistInfo*>();
    return instance;
}

void PlaylistInfo::Refresh() {
    if (!nameText || !authorText || !descriptionText || !coverImage)
        return;

    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist)
        return;

    auto& json = playlist->playlistJSON;
    nameInput->text = json.PlaylistTitle;
    authorInput->text = json.PlaylistAuthor.value_or("");
    descriptionText->SetText(json.PlaylistDescription.value_or("No description"));
    coverImage->sprite = PlaylistCore::GetCoverImage(playlist);

    bool creation = Manager::IsCreatingPlaylist();
    creationIconControl->gameObject->active = creation;
    editIconControl->gameObject->active = !creation;

    if (!creation) {
        bool canDownload = PlaylistCore::PlaylistHasMissingSongs(playlist);
        bool canSync = playlist->playlistJSON.CustomData && playlist->playlistJSON.CustomData->SyncURL.has_value();
        bool canProcess = !Manager::GetSyncing() && !Manager::GetDownloading();
        editIconControl->_dataItems[2]->interactable = canDownload;
        editIconControl->_dataItems[3]->interactable = canSync;
        editIconControl->_dataItems[4]->interactable = canDownload && canProcess;
        editIconControl->_dataItems[5]->interactable = canSync && canProcess;
        editIconControl->ReloadData();
        Utils::DeselectAllCells(editIconControl);
    } else {
        creationIconControl->_dataItems[1]->interactable = !Manager::InAddShortcut();
        creationIconControl->ReloadData();
        Utils::DeselectAllCells(creationIconControl);
    }

    RefreshProcessing();
    UpdateInfoLayout();
}

void PlaylistInfo::RefreshProcessing() {
    if (!processingModal)
        return;
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist)
        return;
    bool sync = Manager::GetSyncing() == playlist;
    bool download = Manager::GetDownloading() == playlist;
    if (!sync && !download)
        processingModal->Hide(true, nullptr);
    else {
        syncText->active = sync;
        downloadText->active = download;
        processingModal->Show(true, true, nullptr);
    }
}

void PlaylistInfo::RefreshImages() {
    if (!imageGrid)
        return;
    auto& images = PlaylistCore::GetLoadedImages();
    // include default but then also subtract one for when multiple of imagePageSize
    imageCount = images.size();
    imagePage = std::clamp(imagePage, 0, imageCount / imagePageSize);
    UpdateImageButtons();
    for (int i = 0; i < imageGrid->childCount; i++) {
        auto child = imageGrid->GetChild(i);
        if (!child)
            continue;
        int idx = imagePageSize * imagePage + i;
        bool inRange = idx <= imageCount;
        if (inRange) {
            auto sprite = idx > 0 ? images[idx - 1] : PlaylistCore::GetDefaultCoverImage();
            child->GetComponent<HMUI::ImageView*>()->sprite = sprite;
        }
        child->gameObject->active = inRange;
    }
}

void PlaylistInfo::UpdateImageButtons() {
    if (!imageLeft || !imageRight)
        return;
    imageLeft->interactable = imagePage > 0;
    imageRight->interactable = imagePage < imageCount / imagePageSize;
}

void PlaylistInfo::UpdateInfoLayout() {
    if (!infoScroll)
        return;
    BSML::MainThreadScheduler::ScheduleNextFrame([this]() {
        UnityEngine::UI::LayoutRebuilder::ForceRebuildLayoutImmediate(infoScroll->contentTransform);
    });
    infoScroll->ScrollTo(0, false);
}

void PlaylistInfo::UpdateKeyboardOffsets() {
    if (!nameInput || !authorInput)
        return;
    auto pos = transform->TransformPoint({-8, 24.5, 0});
    nameInput->_keyboardPositionOffset = nameInput->transform->InverseTransformPoint(pos);
    authorInput->_keyboardPositionOffset = authorInput->transform->InverseTransformPoint(pos);
}

void PlaylistInfo::OpenCoverModal() {
    if (!coverModal)
        return;
    coverModal->Show(true, true, nullptr);
    RefreshImages();
}

void PlaylistInfo::OpenConfirmModal(int action) {
    if (!confirmModal)
        return;
    confirmModalAction = action;
    // todo: change text?
    confirmModal->Show(true, true, nullptr);
}

void PlaylistInfo::coverClicked() {
    OpenCoverModal();
}

void PlaylistInfo::nameTyped(StringW value) {
    if (!infoScroll || !authorInput)
        return;
    UnityEngine::UI::LayoutRebuilder::ForceRebuildLayoutImmediate(infoScroll->contentTransform);
    auto rect = authorInput->GetComponent<UnityEngine::RectTransform*>();
    infoScroll->ScrollTo(rect->rect.m_YMin - rect->anchoredPosition.y - infoScroll->scrollPageSize, true);
    Manager::SetPlaylistName(value);
}

void PlaylistInfo::authorTyped(StringW value) {
    if (!infoScroll || !authorInput)
        return;
    UnityEngine::UI::LayoutRebuilder::ForceRebuildLayoutImmediate(infoScroll->contentTransform);
    infoScroll->ScrollToEnd(true);
    Manager::SetPlaylistAuthor(value);
}

void PlaylistInfo::creationIconClicked(HMUI::SegmentedControl*, int index) {
    if (creationIconControl)
        Utils::DeselectAllCells(creationIconControl);
    if (index >= 2)
        OpenConfirmModal(index - 2);
    else
        Manager::ConfirmAddition(index == 1);
}

void PlaylistInfo::editIconClicked(HMUI::SegmentedControl*, int index) {
    if (editIconControl)
        Utils::DeselectAllCells(editIconControl);
    switch (index) {
        case 4:
            Manager::DownloadMissing();
            break;
        case 5:
            Manager::Sync();
            break;
        default:
            OpenConfirmModal(index + 2);
    }
}

void PlaylistInfo::imageClicked(int index) {
    if (!coverModal)
        return;
    Manager::SetPlaylistCover(imagePageSize * imagePage + index - 1);
    coverModal->Hide(true, nullptr);
}

void PlaylistInfo::imageLeftClicked() {
    imagePage--;
    RefreshImages();
}

void PlaylistInfo::imageRightClicked() {
    imagePage++;
    RefreshImages();
}

void PlaylistInfo::modalYesClicked() {
    if (!confirmModal)
        return;
    confirmModal->Hide(true, nullptr);
    switch (confirmModalAction) {
        case 0:
            Manager::ResetAddition();
            break;
        case 1:
            Manager::CancelAddition();
            break;
        case 2:
            Manager::Delete();
            break;
        case 3:
            Manager::ClearHighlights();
            break;
        case 4:
            Manager::ClearUndownloaded();
            break;
        case 5:
            Manager::ClearSync();
            break;
    }
}
