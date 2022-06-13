#include "Main.hpp"
#include "Types/GridViewAddon.hpp"
#include "Types/PlaylistMenu.hpp"

#include "playlistcore/shared/ResettableStaticPtr.hpp"
#include "questui/shared/BeatSaberUI.hpp"

#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridView.hpp"

#include "HMUI/TableView_ScrollPositionType.hpp"

using namespace PlaylistManager;
using namespace QuestUI;

GridViewAddon* GridViewAddon::addonInstance;

void GridViewAddon::createButtonPressed() {
    if(gridView->GetNumberOfCells() == 0)
        return;
    // select first playlist
    gridView->SelectAndScrollToCellWithIdx(0);
    // select header cell
    auto table = FindComponent<GlobalNamespace::LevelCollectionTableView*>();
    if(table->selectedRow != 0) {
        table->selectedRow = 0;
        table->tableView->SelectCellWithIdx(0, true);
        table->tableView->ScrollToCellWithIdx(0, HMUI::TableView::ScrollPositionType::Center, true);
    }
    // open create menu
    PlaylistMenu::menuInstance->ShowCreateMenu();
}

void GridViewAddon::Init(GlobalNamespace::AnnotatedBeatmapLevelCollectionsViewController* parent) {
    gridView = parent->annotatedBeatmapLevelCollectionsGridView;

    static ConstString name("PlaylistManagerGridViewAddon");
    container = UnityEngine::GameObject::New_ctor(name);
    container->get_transform()->SetParent(parent->get_transform(), false);

    static ConstString contentName("Content");

    auto createButton = BeatSaberUI::CreateUIButton(container->get_transform(), "+", "ActionButton", {37, -3}, [this] {
        createButtonPressed();
    });
    UnityEngine::Object::Destroy(createButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());
    auto sizeFitter = createButton->get_gameObject()->AddComponent<UnityEngine::UI::ContentSizeFitter*>();
    sizeFitter->set_horizontalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
    sizeFitter->set_verticalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
    // slightly adjust centering of "+"
    createButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_margin({-0.5, 0.5, 0, 0});
}

void GridViewAddon::SetVisible(bool visible) {
    container->SetActive(visible);
}

void GridViewAddon::Destroy() {
    GridViewAddon::addonInstance = nullptr;
    UnityEngine::Object::Destroy(container);
    delete this;
}
