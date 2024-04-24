#include "customtypes/allsongs.hpp"

#include "BGLib/Polyglot/Localization.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollection.hpp"
#include "GlobalNamespace/BeatmapCharacteristicsDropdown.hpp"
#include "GlobalNamespace/BeatmapDifficultyMethods.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/SearchFilterParamsViewController.hpp"
#include "HMUI/ScrollView.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "System/Collections/Generic/IEnumerable_1.hpp"
#include "System/Collections/Generic/IEnumerator_1.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "UnityEngine/UI/LayoutRebuilder.hpp"
#include "VRUIControls/VRGraphicRaycaster.hpp"
#include "assets.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "customtypes/dragintercept.hpp"
#include "customtypes/levelcell.hpp"
#include "customtypes/playlistsongs.hpp"
#include "customtypes/tablecallbacks.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "songcore/shared/SongCore.hpp"
#include "utils.hpp"

DEFINE_TYPE(PlaylistManager, AllSongs);

using namespace PlaylistManager;

void AllSongs::OnEnable() {
    name = "AllSongs";
    rectTransform->anchorMin = {0.5, 0.5};
    rectTransform->anchorMax = {0.5, 0.5};
    rectTransform->sizeDelta = {75, 80};
}

void AllSongs::SetupFields() {
    difficultyTexts = ListW<StringW>::New((int) GlobalNamespace::BeatmapDifficulty::ExpertPlus + 2);
    difficultyTexts->Add(BGLib::Polyglot::Localization::Get("BEATMAP_DIFFICULTY_ALL"));
    for (int i = 0; i <= (int) GlobalNamespace::BeatmapDifficulty::ExpertPlus; i++)
        difficultyTexts->Add(GlobalNamespace::BeatmapDifficultyMethods::Name((GlobalNamespace::BeatmapDifficulty) i));

    auto filterer = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::SearchFilterParamsViewController*>()->First();
    characteristics = ListW<GlobalNamespace::BeatmapCharacteristicSO*>(
        filterer->_beatmapCharacteristicsDropdown->_beatmapCharacteristicCollection->beatmapCharacteristics
    );
    characteristicTexts = ListW<StringW>::New(characteristics->Count);
    for (auto& characteristic : characteristics)
        characteristicTexts->Add(Utils::CharacteristicName(characteristic));

    filter.searchText = "";
    filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::All;
    filter.characteristicSerializedName = "Standard";
}

void AllSongs::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (firstActivation) {
        SetupFields();
        // AddHotReload(this, "allsongs");
        BSML::parse_and_construct(IncludedAssets::allsongs_bsml, transform, this);
    } else
        Refresh();
}

void AllSongs::PostParse() {
    if (!layout || !searchBar || !diffSelector || !charSelector)
        return;

    layout->gameObject->AddComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 100;
    auto existing = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::LevelCollectionTableView*>()->First([](auto obj) {
        return obj->transform->parent->name == std::string("LevelCollecionViewController");
    });
    levelTable = UnityEngine::Object::Instantiate(existing, layout, false);
    levelTable->_additionalContentModel = existing->_additionalContentModel;
    levelTable->_entitlementModel = existing->_entitlementModel;
    levelTable->_beatmapLevelsPromoModel = existing->_beatmapLevelsPromoModel;
    levelTable->_playerDataModel = existing->_playerDataModel;
    levelTable->_tableView->GetComponent<VRUIControls::VRGraphicRaycaster*>()->_physicsRaycaster = BSML::Helpers::GetPhysicsRaycasterWithCache();
    levelTable->_tableView->scrollView->_platformHelper = BSML::Helpers::GetIVRPlatformHelper();
    levelTable->_tableView->selectionType = HMUI::TableViewSelectionType::Multiple;
    levelTable->_showLevelPackHeader = false;
    levelTable->_alphabetScrollbar->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {0, -4};

    auto currentContent = levelTable->_tableView->scrollView->contentTransform;
    for (auto& cell : currentContent->GetComponentsInChildren<HMUI::TableCell*>()) {
        if (cell != levelTable->_levelCellPrefab.ptr())  // this can happen for some reason
            UnityEngine::Object::Destroy(cell->gameObject);
        else
            cell->gameObject->active = false;
    }

    levelTable->_tableView->didSelectCellWithIdxEvent =
        BSML::MakeSystemAction((std::function<void(UnityW<HMUI::TableView>, int)>) [this](auto, int idx) { levelSelected(idx); });
    auto callbacks = levelTable->_tableView->gameObject->AddComponent<TableCallbacks*>();
    callbacks->onCellDeselected = [this](int idx) {
        levelDeselected(idx);
    };

    searchInput = BSML::Lite::CreateStringSetting(searchBar, "Search", "", {}, {0, -35, 0}, [this](StringW value) { searchInputTyped(value); });
    searchInput->transform->SetAsFirstSibling();

    diffSelector->dropdown->_modalView->_animateParentCanvas = false;
    charSelector->dropdown->_modalView->_animateParentCanvas = false;
    Utils::AddModalAnimations(diffSelector->dropdown, filterModal);
    Utils::AddModalAnimations(charSelector->dropdown, filterModal);

    diffSelector->set_Value(diffSelector->values[0]);
    charSelector->set_Value(charSelector->values[0]);

    Refresh();
}

void AllSongs::OnDestroy() {
    instance = nullptr;
}

AllSongs* AllSongs::GetInstance() {
    if (!instance)
        instance = BSML::Helpers::CreateViewController<AllSongs*>();
    return instance;
}

void AllSongs::Refresh() {
    if (!levelTable || !unlinkButton)
        return;

    if (unlinkButton->active)
        PlaylistSongs::GetInstance()->Refresh();
    // todo: osts? would need core support
    auto levelPacks = ArrayW<GlobalNamespace::BeatmapLevelPack*>(1);
    levelPacks[0] = (SongCore::SongLoader::CustomLevelPack*) SongCore::API::Loading::GetCustomLevelPack();

    filterTask =
        GlobalNamespace::LevelFilter::FilterLevelsAsync(levelPacks, filter, levelTable->_playerDataModel, levelTable->_entitlementModel, nullptr);

    if (filterTask->IsCompleted) {
        FinishFilterTask();
        return;
    }

    SetLoading(true);

    BSML::MainThreadScheduler::ScheduleUntil(
        [this, correctFilter = filterTask]() { return !filterTask || filterTask != correctFilter || filterTask->IsCompleted; },
        [this, correctFilter = filterTask]() {
            if (!filterTask || filterTask != correctFilter)
                return;
            FinishFilterTask();
        }
    );
}

void AllSongs::FinishFilterTask() {
    if (!filterTask || !levelTable)
        return;
    // save selected levels across filters and drags
    std::set<GlobalNamespace::BeatmapLevel*> selectedLevels;
    for (auto& idx : Utils::GetSelected(levelTable->_tableView))
        selectedLevels.emplace(currentLevels[idx]);

    currentLevels = filterTask->Result;
    filterTask = nullptr;

    SetLoading(false);

    float scrollPos = levelTable->_tableView->contentTransform->anchoredPosition.y;
    auto castLevels = (System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::BeatmapLevel*>*) currentLevels.convert();
    levelTable->SetData(castLevels, levelTable->_playerDataModel->playerData->favoritesLevelIds, true, true);
    // alphabet scrollbar sorting sometimes disagrees with songcore
    currentLevels = levelTable->_beatmapLevels;
    levelTable->_tableView->scrollView->ScrollTo(scrollPos, false);

    levelTable->_tableView->ClearSelection();
    for (auto& level : selectedLevels) {
        if (auto newIdx = currentLevels.index_of(level))
            levelTable->_tableView->_selectedCellIdxs->Add(*newIdx);
    }
    levelTable->_tableView->RefreshCells(true, false);
    // bool canScroll = levelTable->_tableView->scrollView->_verticalScrollIndicator->gameObject->active;
    // levelTable->GetComponent<UnityEngine::RectTransform*>()->sizeDelta = {(float) (canScroll ? -8 : 0), 0};
    UpdateOptionsButton();
}

void AllSongs::UpdateOptionsButton() {
    if (!levelTable || !optionsButton || !selectionText)
        return;
    int selected = levelTable->_tableView->_selectedCellIdxs->Count;
    optionsButton->active = selected > 0;
    selectionText->text = std::to_string(selected);
}

void AllSongs::SetLoading(bool value) {
    if (!levelTable || !levelList || !loadingIndicator)
        return;
    bool hasSongs = currentLevels.size() > 0;
    loadingIndicator->active = value;
    emptyText->active = !value && !hasSongs;
    levelList->active = !value && hasSongs;
}

void AllSongs::linkClicked() {
    if (!linkButton || !unlinkButton)
        return;
    linkButton->active = false;
    unlinkButton->active = true;

    PlaylistSongs::GetInstance()->SetOverrideFilter(&filter);
}

void AllSongs::unlinkClicked() {
    if (!linkButton || !unlinkButton)
        return;
    linkButton->active = true;
    unlinkButton->active = false;

    PlaylistSongs::GetInstance()->SetOverrideFilter(nullptr);
}

void AllSongs::levelSelected(int idx) {
    UpdateOptionsButton();
}

void AllSongs::levelDeselected(int idx) {
    UpdateOptionsButton();
}

void AllSongs::searchInputTyped(StringW value) {
    filter.searchText = value;
    Refresh();
}

void AllSongs::ownedToggled(bool value) {
    filter.songOwned = value;
    Refresh();
}

void AllSongs::unplayedToggled(bool value) {
    filter.songUnplayed = value;
    Refresh();
}

void AllSongs::difficultySelected(StringW value) {
    int diff = difficultyTexts.index_of(value).value();
    // lame
    switch (diff - 1) {
        case -1:
            filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::All;
            break;
        case (int) GlobalNamespace::BeatmapDifficulty::Easy:
            filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::Easy;
            break;
        case (int) GlobalNamespace::BeatmapDifficulty::Normal:
            filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::Normal;
            break;
        case (int) GlobalNamespace::BeatmapDifficulty::Hard:
            filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::Hard;
            break;
        case (int) GlobalNamespace::BeatmapDifficulty::Expert:
            filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::Expert;
            break;
        case (int) GlobalNamespace::BeatmapDifficulty::ExpertPlus:
            filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::ExpertPlus;
            break;
    }
    Refresh();
}

void AllSongs::characteristicSelected(StringW value) {
    auto idx = characteristicTexts.index_of(value).value();
    auto characteristic = characteristics[idx];
    filter.characteristicSerializedName = characteristic->serializedName;
    Refresh();
}

void AllSongs::addClicked() {
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist || !levelTable || !optionsModal)
        return;

    for (auto& idx : Utils::GetSelected(levelTable->_tableView))
        PlaylistCore::AddSongToPlaylist(playlist, currentLevels[idx]);

    optionsModal->Hide(true, nullptr);
    PlaylistSongs::GetInstance()->Refresh();
}

void AllSongs::deleteClicked() {
    if (!levelTable || !optionsModal)
        return;

    std::vector<std::shared_future<void>> futures = {};
    for (auto& idx : Utils::GetSelected(levelTable->_tableView)) {
        auto level = currentLevels[idx];
        PlaylistCore::RemoveSongFromAllPlaylists(level);
        if (auto custom = il2cpp_utils::try_cast<SongCore::SongLoader::CustomBeatmapLevel>(level))
            futures.emplace_back(SongCore::API::Loading::DeleteSong(*custom));
    }

    optionsModal->Hide(true, nullptr);
    SetLoading(true);
    BSML::MainThreadScheduler::ScheduleUntil(
        [futures]() {
            for (auto& future : futures) {
                if (!future.valid() || future.wait_for(std::chrono::nanoseconds(0)) != std::future_status::ready)
                    return false;
            }
            return true;
        },
        [this]() {
            auto refresh = SongCore::API::Loading::RefreshSongs();
            BSML::MainThreadScheduler::AwaitFuture(refresh, [this]() { Refresh(); });
        }
    );
}

void AllSongs::clearClicked() {
    if (!levelTable || !optionsModal)
        return;
    levelTable->_tableView->ClearSelection();
    optionsModal->Hide(true, nullptr);
    UpdateOptionsButton();
}
