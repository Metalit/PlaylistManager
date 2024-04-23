#include "customtypes/mainmenu.hpp"

#include "UnityEngine/RectTransform.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "customtypes/allsongs.hpp"
#include "customtypes/playlistgrid.hpp"
#include "customtypes/playlistinfo.hpp"
#include "customtypes/playlistsongs.hpp"
#include "main.hpp"
#include "manager.hpp"

DEFINE_TYPE(PlaylistManager, MainMenu);

using namespace PlaylistManager;

void MainMenu::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    instance = this;

    auto nav = GetNavigationController();
    nav->ClearChildViewControllers();
    switch (presentDestination) {
        case 0:
            inDetail = false;
            nav->SetChildViewController(PlaylistGrid::GetInstance());
            break;
        case 1:
            inDetail = false;
            nav->SetChildViewControllers({(HMUI::ViewController*) PlaylistGrid::GetInstance(), (HMUI::ViewController*) PlaylistInfo::GetInstance()});
            break;
        case 2:
            inDetail = true;
            nav->SetChildViewControllers(
                {(HMUI::ViewController*) AllSongs::GetInstance(),
                 (HMUI::ViewController*) PlaylistSongs::GetInstance(),
                 (HMUI::ViewController*) PlaylistInfo::GetInstance()}
            );
            break;
    }
    presentDestination = 0;

    if (!firstActivation)
        return;

    name = "PlaylistManagerMainMenu";
    showBackButton = true;
    SetTitle("Playlist Manager", HMUI::ViewController::AnimationType::None);

    ProvideInitialViewControllers(nav, nullptr, nullptr, nullptr, nullptr);
}

void MainMenu::InitialViewControllerWasPresented() {
    auto nav = GetNavigationController();
    // expand screen for wide hover hints
    auto screenRect = nav->transform->parent->GetComponent<UnityEngine::RectTransform*>();
    screenRect->sizeDelta = {screenRect->sizeDelta.x + 80, screenRect->sizeDelta.y};
    // fix layout issues if first present is in detail
    nav->LayoutViewControllers(nav->viewControllers);
}

void MainMenu::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    if (inDetail) {
        Manager::OnDetailExit();
        ShowGrid();
    } else {
        _parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
        // undo screen expansion just in case it could break anything
        auto screenRect = GetNavigationController()->transform->parent->GetComponent<UnityEngine::RectTransform*>();
        screenRect->sizeDelta = {screenRect->sizeDelta.x - 80, screenRect->sizeDelta.y};
        Manager::OnMenuExit();
    }
}

HMUI::NavigationController* MainMenu::GetNavigationController() {
    if (!navigationController) {
        navigationController = BSML::Helpers::CreateViewController<HMUI::NavigationController*>();
        navigationController->name = "PlaylistManagerNavigationController";
        navigationController->rectTransform->sizeDelta = {80, 0};
    }
    return navigationController;
}

void MainMenu::OnDestroy() {
    instance = nullptr;
    progress = nullptr;
    Manager::Invalidate();
}

bool MainMenu::InstanceCreated() {
    return instance != nullptr;
}

MainMenu* MainMenu::GetInstance() {
    if (!instance)
        instance = BSML::Helpers::CreateFlowCoordinator<MainMenu*>();
    return instance;
}

BSML::ProgressBar* MainMenu::GetProgress() {
    if (!progress) {
        progress = BSML::Lite::CreateProgressBar({1.4, 3.1, 4}, "Downloading Songs...", "0 / 0", "Playlist Manager");
        progress->gameObject->active = false;
    }
    return progress;
}

void MainMenu::ShowGrid() {
    auto controller = GetNavigationController();
    if (inDetail) {
        controller->PopViewControllers(3, nullptr, true);
        controller->PushViewController(PlaylistGrid::GetInstance(), nullptr, false);
    } else if (controller->viewControllers->Count == 2)
        controller->PopViewController(nullptr, true);
    inDetail = false;
}

void MainMenu::ShowCreation() {
    auto controller = GetNavigationController();
    if (inDetail) {
        controller->PopViewControllers(3, nullptr, true);
        controller->PushViewController(PlaylistGrid::GetInstance(), nullptr, true);
        controller->PushViewController(PlaylistInfo::GetInstance(), nullptr, false);
    } else if (controller->viewControllers->Count == 1)
        controller->PushViewController(PlaylistInfo::GetInstance(), nullptr, false);
    inDetail = false;
}

void MainMenu::ShowDetail() {
    if (inDetail)
        return;
    auto controller = GetNavigationController();
    controller->PopViewControllers(controller->viewControllers->Count, nullptr, true);
    controller->PushViewController(AllSongs::GetInstance(), nullptr, true);
    controller->PushViewController(PlaylistSongs::GetInstance(), nullptr, true);
    controller->PushViewController(PlaylistInfo::GetInstance(), nullptr, false);
    inDetail = true;
}
