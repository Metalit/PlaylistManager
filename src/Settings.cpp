#include "Main.hpp"
#include "Settings.hpp"
#include "Types/PlaylistMenu.hpp"
#include "Types/PlaylistFilters.hpp"
#include "Types/LevelButtons.hpp"
#include "Types/GridViewAddon.hpp"
#include "Types/Config.hpp"
#include "Icons.hpp"

#include "playlistcore/shared/PlaylistCore.hpp"
#include "playlistcore/shared/ResettableStaticPtr.hpp"
#include "playlistcore/shared/CustomTypes/CoverTableCell.hpp"
#include "playlistcore/shared/Utils.hpp"
#include "questui/shared/BeatSaberUI.hpp"

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "HMUI/Touchable.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/TableView_ScrollPositionType.hpp"
#include "UnityEngine/Resources.hpp"

DEFINE_TYPE(PlaylistManager, SettingsViewController);

using namespace PlaylistManager;
using namespace PlaylistCore;
using namespace PlaylistCore::Utils;
using namespace QuestUI;

void SettingsViewController::DestroyUI() {
    if(PlaylistFilters::filtersInstance)
        PlaylistFilters::filtersInstance->Destroy();
    
    if(ButtonsContainer::buttonsInstance)
        ButtonsContainer::buttonsInstance->Destroy();
    
    if(PlaylistMenu::menuInstance)
        PlaylistMenu::menuInstance->Destroy();

    if(GridViewAddon::addonInstance)
        GridViewAddon::addonInstance->Destroy();
}

void SettingsViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    
    using Vec = UnityEngine::Vector2;

    if(!firstActivation)
        return;

    get_gameObject()->AddComponent<HMUI::Touchable*>();

    auto container = BeatSaberUI::CreateScrollableSettingsContainer(this);
    auto parent = container->get_transform();

    auto coverModal = BeatSaberUI::CreateModal(get_transform(), {83, 17}, nullptr);
    
    auto list = BeatSaberUI::CreateCustomSourceList<CustomListSource*>(coverModal->get_transform(), {70, 15}, [this, coverModal](int cellIdx) {
        DeleteLoadedImage(cellIdx);
        coverModal->Hide(true, nullptr);
    });
    list->setType(csTypeOf(CoverTableCell*));
    list->tableView->tableType = HMUI::TableView::TableType::Horizontal;
    list->tableView->scrollView->scrollViewDirection = HMUI::ScrollView::ScrollViewDirection::Horizontal;

    // scroll arrows
    auto left = BeatSaberUI::CreateUIButton(coverModal->get_transform(), "", "SettingsButton", {-38, 0}, {8, 8}, [list] {
        CustomListSource::ScrollListLeft(list, 4);
    });
    ((UnityEngine::RectTransform*) left->get_transform()->GetChild(0))->set_sizeDelta({8, 8});
    BeatSaberUI::SetButtonSprites(left, LeftCaratInactiveSprite(), LeftCaratSprite());

    auto right = BeatSaberUI::CreateUIButton(coverModal->get_transform(), "", "SettingsButton", {38, 0}, {8, 8}, [list] {
        CustomListSource::ScrollListRight(list, 4);
    });
    ((UnityEngine::RectTransform*) right->get_transform()->GetChild(0))->set_sizeDelta({8, 8});
    BeatSaberUI::SetButtonSprites(right, RightCaratInactiveSprite(), RightCaratSprite());

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(parent);
    horizontal->set_childControlWidth(false);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    auto imageButton = BeatSaberUI::CreateUIButton(horizontal->get_transform(), "Delete Saved Image", Vec{0, 0}, Vec{40, 10}, [list, coverModal] {
        // reload covers from folder
        LoadCoverImages();
        // add cover images and reload
        list->replaceSprites(GetLoadedImages());
        list->tableView->ReloadData();
        list->tableView->ClearSelection();
        coverModal->Show(true, false, nullptr);
    });

    horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(parent);
    horizontal->set_childControlWidth(false);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    auto uiButton = BeatSaberUI::CreateUIButton(horizontal->get_transform(), "Reset UI", Vec{0, 0}, Vec{40, 10}, [this] {
        DestroyUI();
    });
    BeatSaberUI::AddHoverHint(uiButton->get_gameObject(), "Resets all UI instances");
    uiButton->set_interactable(playlistConfig.Management);

    auto managementToggle = BeatSaberUI::CreateToggle(parent, "Enable playlist management", playlistConfig.Management, [this, uiButton](bool enabled) {
        uiButton->set_interactable(enabled);
        playlistConfig.Management = enabled;
        SaveConfig();
        if(!enabled)
            DestroyUI();
    });
    managementToggle->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(60);
    
    auto downloadToggle = BeatSaberUI::CreateToggle(parent, "Show download icons in grid", playlistConfig.DownloadIcon, [](bool enabled){
        playlistConfig.DownloadIcon = enabled;
        SaveConfig();
    });
    downloadToggle->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(60);
    BeatSaberUI::AddHoverHint(downloadToggle->get_gameObject(), "Toggles download icons for custom playlists that do not have all their songs downloaded");

    auto removeSongsToggle = BeatSaberUI::CreateToggle(parent, "Remove songs not on BeatSaver", playlistConfig.RemoveMissing, [](bool enabled){
        playlistConfig.RemoveMissing = enabled;
        SaveConfig();
    });
    downloadToggle->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(60);
    BeatSaberUI::AddHoverHint(removeSongsToggle->get_gameObject(), "Automatically removes songs that are not present on Beat Saver from playlists");
}
