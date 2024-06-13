#pragma once

#include "custom-types/shared/macros.hpp"

#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/LevelFilter.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "HMUI/InputFieldView.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "bsml/shared/BSML/Components/ClickableText.hpp"
#include "bsml/shared/BSML/Components/Settings/DropdownListSetting.hpp"

DECLARE_CLASS_CODEGEN(PlaylistManager, AllSongs, HMUI::ViewController,
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, SetupFields);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_INSTANCE_METHOD(void, PostParse);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_STATIC_METHOD(AllSongs*, GetInstance);

    DECLARE_INSTANCE_METHOD(void, Refresh);
    DECLARE_INSTANCE_METHOD(void, FinishFilterTask);
    DECLARE_INSTANCE_METHOD(void, UpdateOptionsButton);
    DECLARE_INSTANCE_METHOD(void, CloseOptions);
    DECLARE_INSTANCE_METHOD(void, SetLoading, bool value);

    DECLARE_INSTANCE_FIELD(ListW<StringW>, difficultyTexts);
    DECLARE_INSTANCE_FIELD(ListW<StringW>, characteristicTexts);
    DECLARE_INSTANCE_FIELD(ListW<StringW>, characteristicTextsWithAll);

    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, linkButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, unlinkButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, loadingIndicator);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, emptyText);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, levelList);
    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, layout);
    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, searchBar);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, optionsButton);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, selectionText);
    DECLARE_INSTANCE_FIELD(HMUI::ModalView*, filterModal);
    DECLARE_INSTANCE_FIELD(BSML::DropdownListSetting*, diffSelector);
    DECLARE_INSTANCE_FIELD(BSML::DropdownListSetting*, charSelector);
    DECLARE_INSTANCE_FIELD(HMUI::ModalView*, optionsModal);
    DECLARE_INSTANCE_FIELD(BSML::ClickableText*, deleteText);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, deleteTextNoClick);
    DECLARE_INSTANCE_FIELD(HMUI::InputFieldView*, searchInput);
    DECLARE_INSTANCE_FIELD(GlobalNamespace::LevelCollectionTableView*, levelTable);

    DECLARE_INSTANCE_METHOD(void, linkClicked);
    DECLARE_INSTANCE_METHOD(void, unlinkClicked);
    DECLARE_INSTANCE_METHOD(void, levelSelected, int idx);
    DECLARE_INSTANCE_METHOD(void, levelDeselected, int idx);
    DECLARE_INSTANCE_METHOD(void, searchInputTyped, StringW value);
    DECLARE_INSTANCE_METHOD(void, ownedToggled, bool value);
    DECLARE_INSTANCE_METHOD(void, unplayedToggled, bool value);
    DECLARE_INSTANCE_METHOD(void, difficultySelected, StringW value);
    DECLARE_INSTANCE_METHOD(void, characteristicSelected, StringW value);
    DECLARE_INSTANCE_METHOD(void, addClicked);
    DECLARE_INSTANCE_METHOD(void, deleteClicked);
    DECLARE_INSTANCE_METHOD(void, clearClicked);

    DECLARE_INSTANCE_FIELD(ListW<GlobalNamespace::BeatmapCharacteristicSO*>, characteristics);
    DECLARE_INSTANCE_FIELD(ArrayW<GlobalNamespace::BeatmapLevel*>, currentLevels);
    DECLARE_INSTANCE_FIELD_DEFAULT(GlobalNamespace::LevelFilter, filter, {});
    DECLARE_INSTANCE_FIELD(System::Threading::Tasks::Task_1<ArrayW<GlobalNamespace::BeatmapLevel*>>*, filterTask);
    DECLARE_INSTANCE_FIELD(GlobalNamespace::PlayerDataModel*, playerDataModel);
    DECLARE_INSTANCE_FIELD(GlobalNamespace::BeatmapLevelsModel*, beatmapLevelsModel);

   private:
    static inline AllSongs* instance;
)
