#include "utils.hpp"

#include <algorithm>

#include "BGLib/Polyglot/Localization.hpp"
#include "GlobalNamespace/IPreviewMediaData.hpp"
#include "HMUI/AnimatedSwitchView.hpp"
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
#include "bsml/shared/Helpers/delegates.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "customtypes/dragintercept.hpp"
#include "customtypes/mainmenu.hpp"
#include "main.hpp"
#include "playlistcore/shared/Utils.hpp"

namespace Utils {
    static std::vector<std::string> const diffNames = {"Easy", "Normal", "Hard", "Expert", "ExpertPlus"};

    bool CaseInsensitiveEquals(std::string a, std::string b) {
        std::transform(a.begin(), a.end(), a.begin(), tolower);
        std::transform(b.begin(), b.end(), b.begin(), tolower);
        return a == b;
    }

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

    void AnimateModal(HMUI::ModalView* modal, bool out) {
        auto bg = modal->transform->Find("BG")->GetComponent<UnityEngine::UI::Image*>();
        auto canvas = modal->GetComponent<UnityEngine::CanvasGroup*>();

        // todo: fancy coro curve
        if (out) {
            bg->color = {0.2, 0.2, 0.2, 1};
            canvas->alpha = 0.9;
        } else {
            bg->color = {1, 1, 1, 1};
            canvas->alpha = 1;
        }
    }

    void AddModalAnimations(HMUI::SimpleTextDropdown* dropdown, HMUI::ModalView* behindModal) {
        // technically could break with OnDisable, but would need a custom type to fix
        dropdown->_button->onClick->AddListener(BSML::MakeUnityAction([behindModal]() { AnimateModal(behindModal, true); }));
        dropdown->add_didSelectCellWithIdxEvent(BSML::MakeSystemAction(
            (std::function<void(UnityW<HMUI::DropdownWithTableView>, int)>) [behindModal](auto, int) { AnimateModal(behindModal, false); }
        ));
        dropdown->_modalView->add_blockerClickedEvent(BSML::MakeSystemAction([behindModal]() { AnimateModal(behindModal, false); }));
    }

    std::vector<PlaylistCore::Playlist*> GetPlaylistsWithSong(GlobalNamespace::BeatmapLevel* level) {
        std::vector<PlaylistCore::Playlist*> ret;
        auto playlists = PlaylistCore::GetLoadedPlaylists();
        for (auto playlist : playlists) {
            if (playlist->playlistCS->beatmapLevels.contains(level))
                ret.emplace_back(playlist);
        }
        return ret;
    }

    int GetLevelIndex(PlaylistCore::Playlist* playlist, GlobalNamespace::BeatmapLevel* level) {
        return playlist->playlistCS->beatmapLevels.index_of(level).value_or(-1);
    }

    PlaylistCore::BPSong* GetLevelJson(PlaylistCore::Playlist* playlist, GlobalNamespace::BeatmapLevel* level) {
        if (!level)
            return nullptr;
        auto& songs = playlist->playlistJSON.Songs;
        for (auto& song : songs) {
            if (song.Hash == PlaylistCore::Utils::GetLevelHash(level))
                return &song;
        }
        return nullptr;
    }

    std::set<int> GetHighlightedDifficulties(PlaylistCore::BPSong const& song, GlobalNamespace::BeatmapCharacteristicSO* characteristic) {
        std::set<int> ret = {};
        if (!song.Difficulties)
            return ret;
        for (auto& diff : *song.Difficulties) {
            if (!CaseInsensitiveEquals(diff.Characteristic, characteristic->serializedName))
                continue;
            auto namePos = std::find_if(diffNames.begin(), diffNames.end(), [&diff](auto& name) { return CaseInsensitiveEquals(name, diff.Name); });
            if (namePos != diffNames.end())
                ret.emplace(std::distance(diffNames.begin(), namePos));
        }
        return ret;
    }

    bool IsDifficultyHighlighted(PlaylistCore::BPSong const& song, GlobalNamespace::BeatmapCharacteristicSO* characteristic, int difficulty) {
        if (!song.Difficulties)
            return false;
        for (auto& diff : *song.Difficulties) {
            if (!CaseInsensitiveEquals(diff.Characteristic, characteristic->serializedName))
                continue;
            if (CaseInsensitiveEquals(diff.Name, diffNames[difficulty]))
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
            if (!CaseInsensitiveEquals(itr->Characteristic, characteristicSerializedName))
                continue;
            if (CaseInsensitiveEquals(itr->Name, diffNames[difficulty])) {
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

    void SetCellInteractable(HMUI::IconSegmentedControl* iconControl, int idx, bool interactable) {
        auto cell = (HMUI::IconSegmentedControlCell*) iconControl->_cells->get_Item(idx).ptr();
        cell->hideBackgroundImage = !interactable;
        cell->enabled = interactable;
        cell->SetHighlight(false, HMUI::SelectableCell::TransitionType::Instant, true);
    }

    void InstantSetToggle(UnityEngine::UI::Toggle* toggle, bool value) {
        if (toggle->m_IsOn == value)
            return;
        toggle->m_IsOn = value;
        auto animatedSwitch = toggle->GetComponent<HMUI::AnimatedSwitchView*>();
        animatedSwitch->HandleOnValueChanged(value);
        animatedSwitch->_switchAmount = value;
        animatedSwitch->LerpPosition(value);
        animatedSwitch->LerpColors(value, animatedSwitch->_highlightAmount, animatedSwitch->_disabledAmount);
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
        ret->onValueChanged->AddListener(BSML::MakeUnityAction(
            (std::function<void(UnityW<HMUI::InputFieldView>)>) [onInput](UnityW<HMUI::InputFieldView> input) { onInput(input->text); }
        ));
        ret->selectionStateDidChangeEvent = BSML::MakeSystemAction(
            (std::function<void(HMUI::InputFieldView::SelectionState)>) [ text, textColor,
                                                                          highlightColor ](HMUI::InputFieldView::SelectionState state) {
                if (state == HMUI::InputFieldView::SelectionState::Highlighted)
                    text->color = highlightColor;
                else
                    text->color = textColor;
            }
        );
        text->color = textColor;
        placeholder->active = false;
        parent->AddComponent<HMUI::Touchable*>();
        parent->active = true;
        return ret;
    }

    UnityEngine::Color LerpColor(UnityEngine::Color c1, UnityEngine::Color c2, float value) {
        return {c1.r + (c2.r - c1.r) * value, c1.g + (c2.g - c1.g) * value, c1.b + (c2.b - c1.b) * value, c1.a + (c2.a - c1.a) * value};
    }

    ArrayW<UnityEngine::Color> ScaleTexture(UnityEngine::Texture2D* texture, int width, int height) {
        // https://gist.github.com/gszauer/7799899
        auto origColors = texture->GetPixels();
        ArrayW<UnityEngine::Color> destColors(width * height);

        int origWidth = texture->width;
        float ratioX = (texture->width - 1) / (float) width;
        float ratioY = (texture->height - 1) / (float) height;

        for (int destY = 0; destY < height; destY++) {
            int origY = (int) destY * ratioY;
            float yLerp = destY * ratioY - origY;

            float yIdx1 = origY * origWidth;
            float yIdx2 = (origY + 1) * origWidth;
            float yIdxDest = destY * width;

            for (int destX = 0; destX < width; destX++) {
                int origX = (int) destX * ratioX;
                float xLerp = destX * ratioX - origX;
                destColors[yIdxDest + destX] = LerpColor(
                    LerpColor(origColors[yIdx1 + origX], origColors[yIdx1 + origX + 1], xLerp),
                    LerpColor(origColors[yIdx2 + origX], origColors[yIdx2 + origX + 1], xLerp),
                    yLerp
                );
            }
        }
        return destColors;
    }

    struct CoverGetter {
        std::vector<System::Threading::Tasks::Task_1<UnityW<UnityEngine::Sprite>>*> tasks;

        CoverGetter(std::span<GlobalNamespace::BeatmapLevel*> levels, size_t num) {
            levels = levels.subspan(0, std::min(levels.size(), num));
            for (auto& level : levels)
                tasks.emplace_back(level->previewMediaData->GetCoverSpriteAsync(nullptr));
        }
        bool ShouldWait() {
            return std::any_of(tasks.begin(), tasks.end(), [](auto task) { return !task->IsCompleted; });
        }
        UnityEngine::Texture2D* GetCover(int idx) {
            auto unreadable = tasks[idx]->Result->texture;
            auto width = unreadable->width;
            auto height = unreadable->height;
            auto ret = UnityEngine::Texture2D::New_ctor(width, height, UnityEngine::TextureFormat::RGBA32, false, false);

            auto tmp = UnityEngine::RenderTexture::GetTemporary(
                width, height, 0, UnityEngine::RenderTextureFormat::Default, UnityEngine::RenderTextureReadWrite::Default
            );
            UnityEngine::Graphics::Blit(unreadable, tmp);
            auto active = UnityEngine::RenderTexture::GetActive();
            UnityEngine::RenderTexture::SetActive(tmp);
            ret->ReadPixels({0, 0, (float) width, (float) height}, 0, 0);
            ret->Apply();
            UnityEngine::RenderTexture::SetActive(active);
            UnityEngine::RenderTexture::ReleaseTemporary(tmp);

            return ret;
        }
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
                texture->SetPixels(ScaleTexture(getter.GetCover(0), 512, 512));
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
                texture->SetPixels(0, 0, 512, 512, ScaleTexture(getter.GetCover(0), 512, 512));
                auto cropped = texture->GetPixels(0, 0, 256, 512);
                co_yield nullptr;
                texture->SetPixels(0, 0, 512, 512, ScaleTexture(getter.GetCover(1), 512, 512));
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
                texture->SetPixels(0, 0, 512, 512, ScaleTexture(getter.GetCover(0), 512, 512));
                co_yield nullptr;
                texture->SetPixels(256, 0, 256, 256, ScaleTexture(getter.GetCover(1), 256, 256));
                co_yield nullptr;
                texture->SetPixels(256, 256, 256, 256, ScaleTexture(getter.GetCover(2), 256, 256));
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
                texture->SetPixels(0, 0, 256, 256, ScaleTexture(getter.GetCover(0), 256, 256));
                co_yield nullptr;
                texture->SetPixels(256, 0, 256, 256, ScaleTexture(getter.GetCover(1), 256, 256));
                co_yield nullptr;
                texture->SetPixels(0, 256, 256, 256, ScaleTexture(getter.GetCover(2), 256, 256));
                co_yield nullptr;
                texture->SetPixels(256, 256, 256, 256, ScaleTexture(getter.GetCover(3), 256, 256));
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
