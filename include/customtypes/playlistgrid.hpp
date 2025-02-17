#pragma once

#include "custom-types/shared/macros.hpp"

#include "HMUI/ViewController.hpp"
#include "UnityEngine/UI/GridLayoutGroup.hpp"
#include "bsml/shared/BSML/Components/ScrollableContainer.hpp"
#include "customtypes/gridcell.hpp"

DECLARE_CLASS_CODEGEN(PlaylistManager, PlaylistGrid, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_INSTANCE_METHOD(void, PostParse);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_STATIC_METHOD(PlaylistGrid*, GetInstance);

    DECLARE_INSTANCE_METHOD(void, Refresh);
    DECLARE_INSTANCE_METHOD(int, GetIndexForPos, UnityEngine::Vector3 pos);
    DECLARE_INSTANCE_METHOD(GridCell*, GetCell, StringW name, UnityEngine::Sprite* image);

    DECLARE_INSTANCE_FIELD(BSML::ScrollableContainer*, scroller);
    DECLARE_INSTANCE_FIELD(UnityEngine::RectTransform*, scrollBar);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::GridLayoutGroup*, grid);
    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, addCell);
    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, fakeCell);

    DECLARE_INSTANCE_METHOD(void, addClicked);

    DECLARE_INSTANCE_FIELD(int, usedCells);
    DECLARE_INSTANCE_FIELD(ListW<GridCell*>, gridCells);

   private:
    static inline PlaylistGrid* instance;
};