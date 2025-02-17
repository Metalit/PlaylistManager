#pragma once

#include "custom-types/shared/macros.hpp"

#include "HMUI/ViewController.hpp"
#include "HMUI/ImageView.hpp"
#include "HMUI/IconSegmentedControl.hpp"
#include "HMUI/InputFieldView.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/TextPageScrollView.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/GridLayoutGroup.hpp"

DECLARE_CLASS_CODEGEN(PlaylistManager, PlaylistInfo, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, SetupFields);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_INSTANCE_METHOD(void, PostParse);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_STATIC_METHOD(PlaylistInfo*, GetInstance);

    DECLARE_INSTANCE_METHOD(void, Refresh);
    DECLARE_INSTANCE_METHOD(void, RefreshProcessing);
    DECLARE_INSTANCE_METHOD(void, RefreshImages);
    DECLARE_INSTANCE_METHOD(void, UpdateImageButtons);
    DECLARE_INSTANCE_METHOD(void, UpdateInfoLayout);
    DECLARE_INSTANCE_METHOD(void, UpdateKeyboardOffsets);
    DECLARE_INSTANCE_METHOD(void, OpenCoverModal);
    DECLARE_INSTANCE_METHOD(void, OpenConfirmModal, int action);

    DECLARE_INSTANCE_FIELD(ListW<HMUI::IconSegmentedControl::DataItem*>, editButtonData);
    DECLARE_INSTANCE_FIELD(ListW<HMUI::IconSegmentedControl::DataItem*>, creationButtonData);

    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, dummyButton);
    DECLARE_INSTANCE_FIELD(HMUI::ImageView*, dummyImage);
    DECLARE_INSTANCE_FIELD(HMUI::ImageView*, coverImage);
    DECLARE_INSTANCE_FIELD(HMUI::ScrollView*, infoScroll);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, nameText);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, namePlaceholder);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, authorText);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, authorPlaceholder);
    DECLARE_INSTANCE_FIELD(HMUI::TextPageScrollView*, descriptionText);
    DECLARE_INSTANCE_FIELD(HMUI::IconSegmentedControl*, creationIconControl);
    DECLARE_INSTANCE_FIELD(HMUI::IconSegmentedControl*, editIconControl);
    DECLARE_INSTANCE_FIELD(HMUI::ModalView*, coverModal);
    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, imageGrid);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, imageLeft);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, imageRight);
    DECLARE_INSTANCE_FIELD(HMUI::ModalView*, confirmModal);
    DECLARE_INSTANCE_FIELD(HMUI::ModalView*, processingModal);
    DECLARE_INSTANCE_FIELD(HMUI::InputFieldView*, nameInput);
    DECLARE_INSTANCE_FIELD(HMUI::InputFieldView*, authorInput);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, syncText);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, downloadText);

    DECLARE_INSTANCE_METHOD(void, coverClicked);
    DECLARE_INSTANCE_METHOD(void, nameTyped, StringW value);
    DECLARE_INSTANCE_METHOD(void, authorTyped, StringW value);
    DECLARE_INSTANCE_METHOD(void, creationIconClicked, HMUI::SegmentedControl*, int index);
    DECLARE_INSTANCE_METHOD(void, editIconClicked, HMUI::SegmentedControl*, int index);
    DECLARE_INSTANCE_METHOD(void, imageClicked, int index);
    DECLARE_INSTANCE_METHOD(void, imageLeftClicked);
    DECLARE_INSTANCE_METHOD(void, imageRightClicked);
    DECLARE_INSTANCE_METHOD(void, modalYesClicked);

    DECLARE_INSTANCE_FIELD(int, imageCount);
    DECLARE_INSTANCE_FIELD_DEFAULT(int, imagePage, 0);
    // reset, cancel, delete, clear highlights, clear undownloaded, clear sync
    DECLARE_INSTANCE_FIELD_DEFAULT(int, confirmModalAction, 0);

   private:
    static inline PlaylistInfo* instance;
};
