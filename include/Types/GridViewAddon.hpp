#pragma once

#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/GameObject.hpp"

#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsViewController.hpp"

namespace PlaylistManager {

    class GridViewAddon {

        private:
        UnityEngine::GameObject *container;
        UnityEngine::UI::Button *createButton;
        
        GlobalNamespace::AnnotatedBeatmapLevelCollectionsGridView *gridView;

        void createButtonPressed();

        public:
        static GridViewAddon* addonInstance;

        void Init(GlobalNamespace::AnnotatedBeatmapLevelCollectionsViewController* parent);
        void SetVisible(bool visible);
        void Destroy();
    };
}
