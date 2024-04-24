#include "customtypes/playlistsongs.hpp"

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
#include "customtypes/playlistinfo.hpp"
#include "customtypes/tablecallbacks.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "playlistcore/shared/PlaylistCore.hpp"
#include "playlistcore/shared/Utils.hpp"
#include "songcore/shared/SongCore.hpp"
#include "utils.hpp"

DEFINE_TYPE(PlaylistManager, PlaylistSongs);

using namespace PlaylistManager;

void PlaylistSongs::OnEnable() {
    name = "PlaylistSongs";
    rectTransform->anchorMin = {0.5, 0.5};
    rectTransform->anchorMax = {0.5, 0.5};
    rectTransform->sizeDelta = {75, 80};
}

void PlaylistSongs::SetupFields() {
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

void PlaylistSongs::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (firstActivation) {
        SetupFields();
        // AddHotReload(this, "playlistsongs");
        BSML::parse_and_construct(IncludedAssets::playlistsongs_bsml, transform, this);
    } else
        Refresh();
}

void PlaylistSongs::PostParse() {
    if (!layout || !searchBar || !diffSelector || !charSelector || !highlightCharSelector)
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
    callbacks->onCellDragReordered = [this](int oldIdx, int newIdx) {
        levelReordered(oldIdx, newIdx);
    };

    levelTable->gameObject->AddComponent<DragIntercept*>();
    levelTable->_levelCellsReuseIdentifier = "PlaylistManager_DraggableLevelCell";
    levelTable->_levelCellPrefab = UnityEngine::Object::Instantiate(levelTable->_levelCellPrefab);
    levelTable->_levelCellPrefab->gameObject->AddComponent<LevelCell*>();

    searchInput = BSML::Lite::CreateStringSetting(searchBar, "Search", "", {}, {0, -35, 0}, [this](StringW value) { searchInputTyped(value); });
    searchInput->transform->SetAsFirstSibling();

    highlightModal->_animateParentCanvas = false;
    diffSelector->dropdown->_modalView->_animateParentCanvas = false;
    charSelector->dropdown->_modalView->_animateParentCanvas = false;
    highlightCharSelector->dropdown->_modalView->_animateParentCanvas = false;
    Utils::AddModalAnimations(diffSelector->dropdown, filterModal);
    Utils::AddModalAnimations(charSelector->dropdown, filterModal);
    Utils::AddModalAnimations(highlightCharSelector->dropdown, highlightModal);
    highlightModal->add_blockerClickedEvent(BSML::MakeSystemAction([modal = optionsModal]() {
        modal->Show(false, false, nullptr);  // hides when disabled
        modal->gameObject->active = true;
    }));

    diffSelector->set_Value(diffSelector->values[0]);
    charSelector->set_Value(charSelector->values[0]);

    highlightCharSelector->GetComponent<UnityEngine::RectTransform*>()->sizeDelta = {40, 0};
    auto foundToggles = highlightCharSelector->transform->parent->parent->GetComponentsInChildren<BSML::ToggleSetting*>();
    highlightToggles = ArrayW<BSML::ToggleSetting*>(difficultyTexts.size() - 1);
    for (int i = 0; i < foundToggles.size(); i++) {
        if (i >= difficultyTexts.size() - 1)
            continue;
        highlightToggles[i] = foundToggles[i];
        highlightToggles[i]->set_text(difficultyTexts[i + 1]);
        highlightToggles[i]->toggle->onValueChanged->AddListener(BSML::MakeUnityAction((std::function<void(bool)>) [ this, i ](bool value) {
            difficultyToggled(i, value);
        }));
    }
    UnityEngine::Object::Destroy(foundToggles[foundToggles.size() - 1]->gameObject);

    Refresh();
}

void PlaylistSongs::OnDestroy() {
    instance = nullptr;
}

PlaylistSongs* PlaylistSongs::GetInstance() {
    if (!instance)
        instance = BSML::Helpers::CreateViewController<PlaylistSongs*>();
    return instance;
}

void PlaylistSongs::Refresh() {
    if (!levelTable)
        return;
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist || Manager::IsCreatingPlaylist())
        return;
    auto levelPacks = ArrayW<GlobalNamespace::BeatmapLevelPack*>(1);
    levelPacks[0] = (SongCore::SongLoader::CustomLevelPack*) playlist->playlistCS;

    auto& usedFilter = overrideFilter ? *overrideFilter : filter;
    filterTask =
        GlobalNamespace::LevelFilter::FilterLevelsAsync(levelPacks, usedFilter, levelTable->_playerDataModel, levelTable->_entitlementModel, nullptr);

    if (filterTask->IsCompleted) {
        logger.debug("filtering completed instantly");
        FinishFilterTask();
        return;
    }

    SetLoading(true);

    BSML::MainThreadScheduler::ScheduleUntil(
        [this, correctFilter = filterTask]() { return !filterTask || filterTask != correctFilter || filterTask->IsCompleted; },
        [this, correctFilter = filterTask]() {
            if (!filterTask || filterTask != correctFilter)
                return;
            logger.debug("filtering completed");
            FinishFilterTask();
        }
    );
}

void PlaylistSongs::FinishFilterTask() {
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
    levelTable->SetData(castLevels, levelTable->_playerDataModel->playerData->favoritesLevelIds, false, false);
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

void PlaylistSongs::UpdateHighlightCharacteristics() {
    if (!highlightCharSelector || !levelTable)
        return;

    auto selected = Utils::GetSelected(levelTable->_tableView);
    if (selected.empty())
        return;

    std::set<GlobalNamespace::BeatmapCharacteristicSO*> filteredChars = {};
    for (auto levelIdx : selected) {
        auto level = currentLevels[levelIdx];
        auto levelChars = (ArrayW<GlobalNamespace::BeatmapCharacteristicSO*>) level->GetCharacteristics();
        for (auto characteristic : levelChars)
            filteredChars.emplace(characteristic);
        // why enumerator no work
        // auto enumerator = level->GetCharacteristics()->GetEnumerator();
        // while (enumerator->i___System__Collections__IEnumerator()->MoveNext())
        //     filteredChars.emplace(enumerator->Current);
    }

    auto filteredCharTexts = ListW<StringW>::New(filteredChars.size());
    for (auto orderedCharacteristic : characteristics) {
        if (filteredChars.contains(orderedCharacteristic))
            filteredCharTexts->Add(Utils::CharacteristicName(orderedCharacteristic));
    }
    highlightCharSelector->values = (ListW<System::Object*>) filteredCharTexts;
    highlightCharSelector->UpdateChoices();
    highlightCharSelector->set_Value(highlightCharSelector->values[0]);

    UpdateHighlightDifficulties();
}

void PlaylistSongs::UpdateHighlightDifficulties() {
    if (!charSelector || !levelTable)
        return;
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist)
        return;

    auto selected = Utils::GetSelected(levelTable->_tableView);
    if (selected.empty())
        return;

    auto characteristicIdx = characteristicTexts.index_of(highlightCharSelector->get_Value()).value();
    auto characteristic = characteristics[characteristicIdx];

    std::map<int, bool> diffsInSelection = {};
    for (auto levelIdx : selected) {
        auto level = currentLevels[levelIdx];
        auto json = Utils::GetLevelJson(playlist, level);
        // auto levelDiffs = (ArrayW<int>) level->GetDifficulties(characteristic);
        // if (!levelDiffs)
        //     continue;
        // for (auto diff : levelDiffs)
        //     diffsInSelection.emplace(diff);
        auto enumerator = level->GetDifficulties(characteristic)->GetEnumerator();
        while (enumerator->i___System__Collections__IEnumerator()->MoveNext()) {
            int diff = (int) enumerator->Current;
            // make sure all other levels have the difficulty highlighted
            bool highlight = !diffsInSelection.contains(diff) || diffsInSelection[diff];
            highlight = highlight && json && Utils::IsDifficultyHighlighted(*json, characteristic, diff);
            diffsInSelection[diff] = highlight;
        }
    }

    for (int i = 0; i < highlightToggles.size(); i++) {
        bool selected = diffsInSelection.contains(i);
        highlightToggles[i]->set_interactable(selected);
        bool selectionAllHighlighted = !selected ? false : diffsInSelection[i];
        Utils::InstantSetToggle(highlightToggles[i]->toggle, selectionAllHighlighted);
    }
}

void PlaylistSongs::UpdateOptionsButton() {
    if (!levelTable || !optionsButton || !selectionText)
        return;
    int selected = levelTable->_tableView->_selectedCellIdxs->Count;
    optionsButton->active = selected > 0;
    selectionText->text = std::to_string(selected);
}

void PlaylistSongs::SetLoading(bool value) {
    if (!levelTable || !levelList || !loadingIndicator)
        return;
    bool hasSongs = currentLevels.size() > 0;
    loadingIndicator->active = value;
    emptyText->active = !value && !hasSongs;
    levelList->active = !value && hasSongs;
}

void PlaylistSongs::SetOverrideFilter(GlobalNamespace::LevelFilter* value) {
    if (!searchInput || !filterButton)
        return;
    searchInput->interactable = value == nullptr;
    filterButton->interactable = value == nullptr;
    overrideFilter = value;
    Refresh();
}

void PlaylistSongs::levelSelected(int idx) {
    UpdateOptionsButton();
}

void PlaylistSongs::levelDeselected(int idx) {
    UpdateOptionsButton();
}

void PlaylistSongs::levelReordered(int oldIdx, int newIdx) {
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist)
        return;
    logger.debug("moving song from {} to {}", oldIdx, newIdx);
    auto level = currentLevels[oldIdx];
    // in case of search or filter, put it next to the nearby shown level
    newIdx = Utils::GetLevelIndex(playlist, currentLevels[newIdx]);
    if (newIdx < 0)
        return;
    PlaylistCore::SetSongIndex(playlist, level, newIdx);
    // keep scroll position
    if (!levelTable)
        return;
    auto scrollView = levelTable->_tableView->scrollView;
    float pos = scrollView->contentTransform->anchoredPosition.y;
    Refresh();
    scrollView->ScrollTo(pos, false);
}

void PlaylistSongs::searchInputTyped(StringW value) {
    filter.searchText = value;
    Refresh();
}

void PlaylistSongs::ownedToggled(bool value) {
    filter.songOwned = value;
    Refresh();
}

void PlaylistSongs::unplayedToggled(bool value) {
    filter.songUnplayed = value;
    Refresh();
}

void PlaylistSongs::difficultySelected(StringW value) {
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

void PlaylistSongs::characteristicSelected(StringW value) {
    auto idx = characteristicTexts.index_of(value).value();
    auto characteristic = characteristics[idx];
    filter.characteristicSerializedName = characteristic->serializedName;
    Refresh();
}

void PlaylistSongs::removeClicked() {
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist || !levelTable || !optionsModal)
        return;

    for (auto& idx : Utils::GetSelected(levelTable->_tableView))
        PlaylistCore::RemoveSongFromPlaylist(playlist, currentLevels[idx]);

    optionsModal->Hide(true, nullptr);
    Refresh();
}

void PlaylistSongs::coversClicked() {
    if (!levelTable || !optionsModal)
        return;
    optionsModal->Hide(true, nullptr);

    std::vector<GlobalNamespace::BeatmapLevel*> selectedLevels;
    for (auto& idx : Utils::GetSelected(levelTable->_tableView))
        selectedLevels.emplace_back(currentLevels[idx]);

    if (selectedLevels.empty())
        return;

    std::string fileName = fmt::format("{}_generated_{}", selectedLevels[0]->songName, selectedLevels.size());
    fileName = PlaylistCore::Utils::SanitizeFileName(fileName);

    auto future = Utils::GenerateCoverImage(selectedLevels);
    BSML::MainThreadScheduler::AwaitFuture(future, [future, fileName]() mutable {
        auto texture = future.get();
        if (texture) {
            static std::string const dir = "/sdcard/ModData/com.beatgames.beatsaber/Mods/PlaylistManager/Covers/";
            while (!PlaylistCore::Utils::UniqueFileName(fileName, dir))
                fileName = "_" + fileName;
            PlaylistCore::Utils::WriteImageToFile(dir + fileName + ".png", texture);
            PlaylistCore::LoadCoverImages();
            PlaylistInfo::GetInstance()->RefreshImages();
            logger.debug("setting cover to {}", PlaylistCore::GetLoadedImages().size() - 1);
            Manager::SetPlaylistCover(PlaylistCore::GetLoadedImages().size() - 1);
        } else
            Manager::SetPlaylistCover(-1);
    });
}

void PlaylistSongs::deleteClicked() {
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

void PlaylistSongs::clearClicked() {
    if (!levelTable || !optionsModal)
        return;
    levelTable->_tableView->ClearSelection();
    optionsModal->Hide(true, nullptr);
    UpdateOptionsButton();
}

void PlaylistSongs::highlightClicked() {
    if (!highlightModal || !optionsModal)
        return;
    UpdateHighlightCharacteristics();
    highlightModal->Show(true, false, BSML::MakeSystemAction([modal = optionsModal]() { modal->gameObject->active = false; }));
}

void PlaylistSongs::highlightCharSelected(StringW value) {
    UpdateHighlightDifficulties();
}

void PlaylistSongs::difficultyToggled(int difficulty, bool value) {
    if (!highlightCharSelector)
        return;
    auto playlist = Manager::GetSelectedPlaylist();
    if (!playlist)
        return;

    auto characteristicIdx = characteristicTexts.index_of(highlightCharSelector->get_Value()).value();
    std::string serializedName = characteristics[characteristicIdx]->serializedName;

    auto selected = Utils::GetSelected(levelTable->_tableView);
    if (selected.empty())
        return;

    for (auto levelIdx : selected) {
        auto json = Utils::GetLevelJson(playlist, currentLevels[levelIdx]);
        if (!json)
            continue;
        Utils::SetDifficultyHighlighted(*json, serializedName, difficulty, value);
    }
    playlist->Save();
}
