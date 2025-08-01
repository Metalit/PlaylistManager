#include "customtypes/allsongs.hpp"

#include "BGLib/Polyglot/Localization.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollection.hpp"
#include "GlobalNamespace/BeatmapCharacteristicsDropdown.hpp"
#include "GlobalNamespace/BeatmapDifficultyMethods.hpp"
#include "GlobalNamespace/PlayerData.hpp"
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
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/delegates.hpp"
#include "metacore/shared/ui.hpp"
#include "songcore/shared/SongCore.hpp"
#include "utils.hpp"

DEFINE_TYPE(PlaylistManager, AllSongs);

using namespace PlaylistManager;

void AllSongs::OnEnable() {
    name = "AllSongs";
    rectTransform->anchorMin = {0.5, 0.5};
    rectTransform->anchorMax = {0.5, 0.5};
    rectTransform->sizeDelta = {79, 80};
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
    // weird const requrement
    characteristicTextsWithAll = ListW<StringW>::New((std::span<StringW const>) characteristicTexts.ref_to());
    characteristicTextsWithAll->Insert(0, "All");

    filter.searchText = "";
    filter.difficulties = GlobalNamespace::BeatmapDifficultyMask::All;
    filter.characteristicSerializedName = nullptr;

    playerDataModel = filterer->_playerDataModel;
    beatmapLevelsModel = BSML::Helpers::GetMainFlowCoordinator()->_beatmapLevelsModel;
}

void AllSongs::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (firstActivation) {
        SetupFields();
        BSML_FILE(allsongs);
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
    layout->gameObject->active = false;
    levelTable = UnityEngine::Object::Instantiate(existing, layout, false);
    levelTable->_additionalContentModel = existing->_additionalContentModel;
    levelTable->_entitlementModel = existing->_entitlementModel;
    levelTable->_beatmapLevelsPromoModel = existing->_beatmapLevelsPromoModel;
    levelTable->_tableView->GetComponent<VRUIControls::VRGraphicRaycaster*>()->_physicsRaycaster = BSML::Helpers::GetPhysicsRaycasterWithCache();
    levelTable->_tableView->scrollView->_platformHelper = BSML::Helpers::GetIVRPlatformHelper();
    levelTable->_tableView->selectionType = HMUI::TableViewSelectionType::Multiple;
    levelTable->_showLevelPackHeader = false;
    levelTable->_alphabetScrollbar->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {0, -4};
    layout->gameObject->active = true;

    auto currentContent = levelTable->_tableView->scrollView->contentTransform;
    for (auto& cell : currentContent->GetComponentsInChildren<HMUI::TableCell*>()) {
        if (cell != levelTable->_levelCellPrefab.ptr())  // this can happen for some reason
            UnityEngine::Object::Destroy(cell->gameObject);
        else
            cell->gameObject->active = false;
    }

    levelTable->_tableView->didSelectCellWithIdxEvent =
        MetaCore::Delegates::MakeSystemAction([this](UnityW<HMUI::TableView>, int idx) { levelSelected(idx); });
    levelTable->_tableView->didDeselectCellWithIdxEvent =
        MetaCore::Delegates::MakeSystemAction([this](UnityW<HMUI::TableView>, int idx) { levelDeselected(idx); });

    searchInput = BSML::Lite::CreateStringSetting(searchBar, "Search", "", {}, {0, -35, 0}, [this](StringW value) { searchInputTyped(value); });
    searchInput->transform->SetAsFirstSibling();

    MetaCore::UI::AddModalAnimations(diffSelector->dropdown, filterModal);
    MetaCore::UI::AddModalAnimations(charSelector->dropdown, filterModal);

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

    auto osts = beatmapLevelsModel->ostAndExtrasBeatmapLevelsRepository->_beatmapLevelPacks;
    auto dlcs = beatmapLevelsModel->dlcBeatmapLevelsRepository->_beatmapLevelPacks;
    auto levelPacks = ArrayW<GlobalNamespace::BeatmapLevelPack*>(osts.size() + dlcs.size() + 1);
    int i = 0;
    for (auto& ost : osts)
        levelPacks[i++] = ost;
    for (auto& dlc : dlcs)
        levelPacks[i++] = dlc;
    levelPacks[i] = SongCore::API::Loading::GetCustomLevelPack();

    filterTask = GlobalNamespace::LevelFilter::FilterLevelsAsync(levelPacks, filter, playerDataModel, levelTable->_entitlementModel, nullptr);

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
    levelTable->SetData(castLevels, playerDataModel->playerData->favoritesLevelIds, true, true);
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

    // keep selections order only if nothing is newly missing
    for (auto level : selectionsOrder) {
        if (!currentLevels.contains(level)) {
            selectionsOrder.clear();
            break;
        }
    }

    UpdateOptionsButton();
}

void AllSongs::UpdateOptionsButton() {
    if (!levelTable || !optionsButton || !selectionText || !deleteText || !deleteTextNoClick || !betweenText || !betweenTextNoClick)
        return;
    auto selected = Utils::GetSelected(levelTable->_tableView);
    optionsButton->active = selected.size() > 0;
    optionsButton->transform->parent->gameObject->active = selected.size() > 0;
    selectionText->text = std::to_string(selected.size());

    bool canDelete = false;
    for (auto& idx : selected) {
        if (il2cpp_utils::try_cast<SongCore::SongLoader::CustomBeatmapLevel>(currentLevels[idx])) {
            canDelete = true;
            break;
        }
    }
    deleteText->gameObject->active = canDelete;
    deleteTextNoClick->active = !canDelete;

    bool canSelectBetween = selectionsOrder.size() >= 2;
    betweenText->gameObject->active = canSelectBetween;
    betweenTextNoClick->active = !canSelectBetween;
}

void AllSongs::CloseOptions() {
    if (!optionsModal)
        return;
    auto onHide = MetaCore::Delegates::MakeSystemAction([modal = optionsModal]() {
        for (auto text : modal->GetComponentsInChildren<BSML::ClickableText*>())
            text->set_isHighlighted(false);
    });
    optionsModal->Hide(true, onHide);
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
    selectionsOrder.emplace_back(currentLevels[idx]);
    UpdateOptionsButton();
}

void AllSongs::levelDeselected(int idx) {
    std::erase(selectionsOrder, currentLevels[idx]);
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
    if (value != "All") {
        auto idx = characteristicTexts.index_of(value).value();
        auto characteristic = characteristics[idx];
        filter.characteristicSerializedName = characteristic->serializedName;
    } else
        filter.characteristicSerializedName = nullptr;
    Refresh();
}

void AllSongs::addClicked() {
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist || !levelTable)
        return;

    for (auto& idx : Utils::GetSelected(levelTable->_tableView))
        PlaylistCore::AddSongToPlaylist(playlist, currentLevels[idx]);

    CloseOptions();
    PlaylistSongs::GetInstance()->Refresh();
}

void AllSongs::deleteClicked() {
    if (!levelTable)
        return;

    std::vector<std::shared_future<void>> futures = {};
    for (auto& idx : Utils::GetSelected(levelTable->_tableView)) {
        auto level = currentLevels[idx];
        if (auto custom = il2cpp_utils::try_cast<SongCore::SongLoader::CustomBeatmapLevel>(level)) {
            PlaylistCore::RemoveSongFromAllPlaylists(level);
            futures.emplace_back(SongCore::API::Loading::DeleteSong(*custom));
        }
    }

    selectionsOrder.clear();
    CloseOptions();
    SetLoading(true);

    BSML::MainThreadScheduler::ScheduleUntil(
        [futures]() {
            for (auto& future : futures) {
                if (!future.valid() || future.wait_for(std::chrono::nanoseconds(0)) != std::future_status::ready)
                    return false;
            }
            return true;
        },
        []() { SongCore::API::Loading::RefreshSongs(); }
    );
}

void AllSongs::betweenClicked() {
    if (!levelTable || selectionsOrder.size() < 2)
        return;
    auto start = currentLevels.index_of(selectionsOrder[selectionsOrder.size() - 1]);
    auto end = currentLevels.index_of(selectionsOrder[selectionsOrder.size() - 2]);
    selectionsOrder.clear();  // choosing not to have it go back to the previous pair if you click it again
    if (start && end)
        Utils::AddSelected(levelTable->_tableView, *start, *end);
    CloseOptions();
    UpdateOptionsButton();
}

void AllSongs::invertClicked() {
    if (!levelTable)
        return;
    selectionsOrder.clear();
    Utils::InvertSelected(levelTable->_tableView);
    CloseOptions();
    UpdateOptionsButton();
}

void AllSongs::clearClicked() {
    if (!levelTable)
        return;
    levelTable->_tableView->ClearSelection();
    CloseOptions();
    UpdateOptionsButton();
}
