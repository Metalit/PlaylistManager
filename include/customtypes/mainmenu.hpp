#pragma once

#include "custom-types/shared/macros.hpp"

#include "bsml/shared/BSML/Components/ProgressBar.hpp"
#include "HMUI/FlowCoordinator.hpp"
#include "HMUI/NavigationController.hpp"

DECLARE_CLASS_CODEGEN(PlaylistManager, MainMenu, HMUI::FlowCoordinator,
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::FlowCoordinator::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_OVERRIDE_METHOD_MATCH(void, InitialViewControllerWasPresented, &HMUI::FlowCoordinator::InitialViewControllerWasPresented);
    DECLARE_OVERRIDE_METHOD_MATCH(void, BackButtonWasPressed, &HMUI::FlowCoordinator::BackButtonWasPressed, HMUI::ViewController* topViewController);
    DECLARE_INSTANCE_METHOD(HMUI::NavigationController*, GetNavigationController);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_STATIC_METHOD(bool, InstanceCreated);
    DECLARE_STATIC_METHOD(MainMenu*, GetInstance);
    DECLARE_STATIC_METHOD(BSML::ProgressBar*, GetProgress);

    DECLARE_INSTANCE_METHOD(void, ShowGrid);
    DECLARE_INSTANCE_METHOD(void, ShowCreation);
    DECLARE_INSTANCE_METHOD(void, ShowDetail);

    // grid, creation, detail
    DECLARE_INSTANCE_FIELD_DEFAULT(int, presentDestination, 0);
    DECLARE_INSTANCE_FIELD_DEFAULT(bool, inDetail, false);

   private:
    HMUI::NavigationController* navigationController;
    static inline MainMenu* instance;
    static inline BSML::ProgressBar* progress;
)
