#include "utils.hpp"

#include <algorithm>

#include "BGLib/Polyglot/Localization.hpp"
#include "GlobalNamespace/IPreviewMediaData.hpp"
#include "HMUI/IconSegmentedControlCell.hpp"
#include "HMUI/ImageView.hpp"
#include "HMUI/SelectableCellStaticAnimations.hpp"
#include "HMUI/StackLayoutGroup.hpp"
#include "HMUI/Touchable.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/CanvasGroup.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/RectOffset.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/RenderTextureReadWrite.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "customtypes/dragintercept.hpp"
#include "customtypes/mainmenu.hpp"
#include "main.hpp"
#include "metacore/shared/delegates.hpp"
#include "metacore/shared/songs.hpp"
#include "metacore/shared/strings.hpp"
#include "metacore/shared/unity.hpp"

namespace Utils {
    static std::vector<std::string> const diffNames = {"Easy", "Normal", "Hard", "Expert", "ExpertPlus"};

    std::set<int> GetSelected(HMUI::TableView* tableView) {
        std::set<int> ret;
        auto enumerator = tableView->_selectedCellIdxs->GetEnumerator();
        while (enumerator.MoveNext())
            ret.emplace(enumerator.Current);
        return ret;
    }

    StringW CharacteristicName(GlobalNamespace::BeatmapCharacteristicSO* characteristic) {
        auto ret = BGLib::Polyglot::Localization::Get(characteristic->characteristicNameLocalizationKey);
        if (!ret)
            ret = characteristic->characteristicNameLocalizationKey;
        return ret;
    }

    std::vector<PlaylistCore::Playlist*> GetPlaylistsWithSong(GlobalNamespace::BeatmapLevel* level) {
        std::vector<PlaylistCore::Playlist*> ret;
        auto playlists = PlaylistCore::GetLoadedPlaylists();
        for (auto playlist : playlists) {
            if (playlist->playlistCS->_beatmapLevels.contains(level))
                ret.emplace_back(playlist);
        }
        return ret;
    }

    int GetLevelIndex(PlaylistCore::Playlist* playlist, GlobalNamespace::BeatmapLevel* level) {
        return playlist->playlistCS->_beatmapLevels.index_of(level).value_or(-1);
    }

    PlaylistCore::BPSong* GetLevelJson(PlaylistCore::Playlist* playlist, GlobalNamespace::BeatmapLevel* level) {
        if (!level)
            return nullptr;
        auto& songs = playlist->playlistJSON.Songs;
        for (auto& song : songs) {
            if (song.Hash == MetaCore::Songs::GetHash(level))
                return &song;
        }
        return nullptr;
    }

    std::set<int> GetHighlightedDifficulties(PlaylistCore::BPSong const& song, GlobalNamespace::BeatmapCharacteristicSO* characteristic) {
        std::set<int> ret = {};
        if (!song.Difficulties)
            return ret;
        for (auto& diff : *song.Difficulties) {
            if (!MetaCore::Strings::IEquals(diff.Characteristic, characteristic->serializedName))
                continue;
            auto namePos =
                std::find_if(diffNames.begin(), diffNames.end(), [&diff](auto& name) { return MetaCore::Strings::IEquals(name, diff.Name); });
            if (namePos != diffNames.end())
                ret.emplace(std::distance(diffNames.begin(), namePos));
        }
        return ret;
    }

    bool IsDifficultyHighlighted(PlaylistCore::BPSong const& song, GlobalNamespace::BeatmapCharacteristicSO* characteristic, int difficulty) {
        if (!song.Difficulties)
            return false;
        for (auto& diff : *song.Difficulties) {
            if (!MetaCore::Strings::IEquals(diff.Characteristic, characteristic->serializedName))
                continue;
            if (MetaCore::Strings::IEquals(diff.Name, diffNames[difficulty]))
                return true;
        }
        return false;
    }

    void SetDifficultyHighlighted(PlaylistCore::BPSong& song, std::string characteristicSerializedName, int difficulty, bool value) {
        if (!song.Difficulties)
            song.Difficulties.emplace();
        auto& diffs = *song.Difficulties;

        auto foundItr = diffs.end();
        for (auto itr = diffs.begin(); itr != diffs.end(); itr++) {
            if (!MetaCore::Strings::IEquals(itr->Characteristic, characteristicSerializedName))
                continue;
            if (MetaCore::Strings::IEquals(itr->Name, diffNames[difficulty])) {
                if (foundItr == diffs.end())
                    foundItr = itr;
                else
                    itr = diffs.erase(itr);  // remove dupes, foundItr is not invalidated here
            }
        }
        bool found = foundItr != diffs.end();
        if (value && !found) {
            auto& added = diffs.emplace_back();
            added.Characteristic = characteristicSerializedName;
            added.Name = diffNames[difficulty];
        } else if (!value && found)
            diffs.erase(foundItr);
    }

    void SetupIcons(HMUI::IconSegmentedControl* iconControl) {
        iconControl->_hideCellBackground = false;
        iconControl->_overrideCellSize = true;
        iconControl->_iconSize = 6;
        iconControl->_padding = 2;
        iconControl->ReloadData();
        DeselectAllCells(iconControl);
    }

    void DeselectAllCells(HMUI::IconSegmentedControl* iconControl) {
        auto cell = iconControl->_cells->get_Item(iconControl->_selectedCellNumber);
        cell->selected = false;
        cell->GetComponent<HMUI::SelectableCellStaticAnimations*>()->RefreshVisuals();
    }

    HMUI::InputFieldView* CreateInput(
        TMPro::TextMeshProUGUI* text,
        HMUI::ImageView* caret,
        UnityEngine::UI::Button* clear,
        UnityEngine::GameObject* placeholder,
        UnityEngine::Vector2 keyboardOffset,
        std::function<void(StringW)> onInput,
        UnityEngine::Color textColor,
        UnityEngine::Color highlightColor
    ) {
        auto parent = text->transform->parent->gameObject;
        parent->active = false;
        auto ret = parent->AddComponent<HMUI::InputFieldView*>();
        ret->_textView = text;
        ret->_blinkingCaret = caret;
        ret->_clearSearchButton = clear;
        ret->_placeholderText = placeholder;
        ret->_keyboardPositionOffset = {keyboardOffset.x, keyboardOffset.y, 0};
        ret->_textLengthLimit = 9999;
        ret->onValueChanged->AddListener(MetaCore::Delegates::MakeUnityAction([onInput](UnityW<HMUI::InputFieldView> input) { onInput(input->text); })
        );
        ret->selectionStateDidChangeEvent =
            MetaCore::Delegates::MakeSystemAction([text, textColor, highlightColor](HMUI::InputFieldView::SelectionState state) {
                if (state == HMUI::InputFieldView::SelectionState::Highlighted)
                    text->color = highlightColor;
                else
                    text->color = textColor;
            });
        text->color = textColor;
        placeholder->active = false;
        parent->AddComponent<HMUI::Touchable*>();
        parent->active = true;
        return ret;
    }

    struct CoverGetter {
        std::vector<System::Threading::Tasks::Task_1<UnityW<UnityEngine::Sprite>>*> tasks;

        CoverGetter(std::span<GlobalNamespace::BeatmapLevel*> levels, size_t num) {
            levels = levels.subspan(0, std::min(levels.size(), num));
            for (auto& level : levels)
                tasks.emplace_back(level->previewMediaData->GetCoverSpriteAsync());
        }
        bool ShouldWait() {
            return std::any_of(tasks.begin(), tasks.end(), [](auto task) { return !task->IsCompleted; });
        }
        UnityEngine::Sprite* GetCover(int idx) { return tasks[idx]->Result; }
    };

    custom_types::Helpers::Coroutine
    CoverImageCoroutine(std::span<GlobalNamespace::BeatmapLevel*> levels, std::promise<UnityEngine::Texture2D*> promise) {
        switch (levels.size()) {
            case 0:
                promise.set_value(nullptr);
                break;
            case 1: {
                auto getter = CoverGetter(levels, 1);
                while (getter.ShouldWait())
                    co_yield nullptr;

                auto texture = UnityEngine::Texture2D::New_ctor(512, 512, UnityEngine::TextureFormat::RGBA32, false, false);
                texture->SetPixels(MetaCore::Engine::ScalePixels(getter.GetCover(0), 512, 512));
                texture->Apply();

                co_yield nullptr;
                promise.set_value(texture);
                break;
            }
            case 2: {
                auto getter = CoverGetter(levels, 2);
                while (getter.ShouldWait())
                    co_yield nullptr;

                auto texture = UnityEngine::Texture2D::New_ctor(512, 512, UnityEngine::TextureFormat::RGBA32, false, false);
                texture->SetPixels(0, 0, 512, 512, MetaCore::Engine::ScalePixels(getter.GetCover(0), 512, 512));
                auto cropped = texture->GetPixels(0, 0, 256, 512);
                co_yield nullptr;
                texture->SetPixels(0, 0, 512, 512, MetaCore::Engine::ScalePixels(getter.GetCover(1), 512, 512));
                co_yield nullptr;
                texture->SetPixels(0, 0, 256, 512, cropped);
                texture->Apply();

                co_yield nullptr;
                promise.set_value(texture);
                break;
            }
            case 3: {
                auto getter = CoverGetter(levels, 3);
                while (getter.ShouldWait())
                    co_yield nullptr;

                auto texture = UnityEngine::Texture2D::New_ctor(512, 512, UnityEngine::TextureFormat::RGBA32, false, false);
                texture->SetPixels(0, 0, 512, 512, MetaCore::Engine::ScalePixels(getter.GetCover(0), 512, 512));
                co_yield nullptr;
                texture->SetPixels(256, 0, 256, 256, MetaCore::Engine::ScalePixels(getter.GetCover(1), 256, 256));
                co_yield nullptr;
                texture->SetPixels(256, 256, 256, 256, MetaCore::Engine::ScalePixels(getter.GetCover(2), 256, 256));
                texture->Apply();

                co_yield nullptr;
                promise.set_value(texture);
                break;
            }
            default: {
                auto getter = CoverGetter(levels, 4);
                while (getter.ShouldWait())
                    co_yield nullptr;

                auto texture = UnityEngine::Texture2D::New_ctor(512, 512, UnityEngine::TextureFormat::RGBA32, false, false);
                texture->SetPixels(0, 0, 256, 256, MetaCore::Engine::ScalePixels(getter.GetCover(0), 256, 256));
                co_yield nullptr;
                texture->SetPixels(256, 0, 256, 256, MetaCore::Engine::ScalePixels(getter.GetCover(1), 256, 256));
                co_yield nullptr;
                texture->SetPixels(0, 256, 256, 256, MetaCore::Engine::ScalePixels(getter.GetCover(2), 256, 256));
                co_yield nullptr;
                texture->SetPixels(256, 256, 256, 256, MetaCore::Engine::ScalePixels(getter.GetCover(3), 256, 256));
                texture->Apply();

                co_yield nullptr;
                promise.set_value(texture);
                break;
            }
        }
        co_return;
    }

    std::shared_future<UnityEngine::Texture2D*> GenerateCoverImage(std::span<GlobalNamespace::BeatmapLevel*> levels) {
        std::promise<UnityEngine::Texture2D*> promise;
        auto ret = promise.get_future();
        PlaylistManager::MainMenu::GetInstance()->StartCoroutine(
            custom_types::Helpers::CoroutineHelper::New(CoverImageCoroutine(levels, std::move(promise)))
        );
        return ret;
    }

    UnityEngine::Material* GetCurvedCornersMaterial() {
        static UnityW<UnityEngine::Material> material;
        if (!material) {
            material = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::Material*>()->First([](auto mat) {
                return mat->name == std::string("UINoGlowRoundEdge");
            });
        }
        return material;
    }
}
