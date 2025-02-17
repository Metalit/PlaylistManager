#pragma once

#include "custom-types/shared/macros.hpp"

#include "UnityEngine/MonoBehaviour.hpp"

DECLARE_CLASS_CODEGEN(PlaylistManager, TableCallbacks, UnityEngine::MonoBehaviour) {
    DECLARE_DEFAULT_CTOR();

   public:
    std::function<void (int)> onCellDeselected = nullptr;
    std::function<void (int, int)> onCellDragReordered = nullptr;
};
