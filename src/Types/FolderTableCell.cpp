#include "Main.hpp"
#include "Types/FolderTableCell.hpp"
#include "Icons.hpp"

#include "playlistcore/shared/ResettableStaticPtr.hpp"
#include "questui/shared/BeatSaberUI.hpp"

#include "UnityEngine/Resources.hpp"

using namespace PlaylistManager;
using namespace QuestUI;

DEFINE_TYPE(PlaylistManager, FolderTableCell);

// "polymorphism"
void FolderTableCell::ctor() {
    // run tableCell ctor
    auto thisref = this;
    static auto info = il2cpp_utils::FindMethodUnsafe("HMUI", "TableCell", ".ctor", 0);
    il2cpp_utils::RunMethodRethrow(thisref, info);
    
    refreshVisualsFunc = [this](){ refreshVisuals(); };
    initFunc = [this](UnityEngine::Sprite* sprite, std::string text){ init(sprite, text); };
    setSpriteFunc = [this](UnityEngine::Sprite* sprite){};
    setTextFunc = [this](std::string text){ setText(text); };
}

void FolderTableCell::refreshVisuals() {
    if(selected || highlighted) {
        selectImage->set_color({0.5, 0.5, 0.5, (float)(highlighted ? 0.7 : 0.5)});
        selectImage->get_gameObject()->set_active(true);
    } else
        selectImage->get_gameObject()->set_active(false);
}

void FolderTableCell::init(UnityEngine::Sprite* sprite, std::string text) {
    // get rounded sprite
    STATIC_AUTO(img, UnityEngine::Resources::FindObjectsOfTypeAll<HMUI::ImageView*>().Last([](HMUI::ImageView* x) {
        auto sprite = x->get_sprite();
        return sprite && sprite->get_name() == "RoundRect10";
    }));
    // create images
    selectImage = BeatSaberUI::CreateImage(get_transform(), img->get_sprite(), {0, 0}, {15, 25});
    auto folderIcon = BeatSaberUI::CreateImage(get_transform(), FolderSprite(), {0, 3.9}, {13, 13});
    folderName = BeatSaberUI::CreateText(get_transform(), text, false, {0, -5.3}, {13, 14});
    folderName->set_overflowMode(TMPro::TextOverflowModes::Ellipsis);
    folderName->set_alignment(TMPro::TextAlignmentOptions::Center);
    folderName->set_enableWordWrapping(true);
    folderName->set_lineSpacing(-40);
    folderName->set_fontSize(3);

    selectImage->set_type(UnityEngine::UI::Image::Type::Tiled);
    selectImage->get_gameObject()->set_active(false);
}

void FolderTableCell::setText(std::string text) {
    folderName->set_text(text);
}
