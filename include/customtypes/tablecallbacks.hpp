#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(PlaylistManager, TableCallbacks, UnityEngine::MonoBehaviour) {
    DECLARE_DEFAULT_CTOR();

   public:
    std::function<void(int, int)> onCellDragReordered = nullptr;
};
