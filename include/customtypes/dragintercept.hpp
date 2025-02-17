#pragma once

#include "custom-types/shared/macros.hpp"

#include "UnityEngine/EventSystems/IPointerDownHandler.hpp"
#include "UnityEngine/MonoBehaviour.hpp"

#define UES UnityEngine::EventSystems

#define INTERFACES \
    UES::IPointerDownHandler*

// in the VRInputModule, if it detects an IPointerClickHandler on the pressed object, it will end the press there
// and never actually drag anything
// but if an IPointerDownHandler is in the hierarchy, it will take priority as the pressed object and not cancel drags
DECLARE_CLASS_CODEGEN_INTERFACES(PlaylistManager, DragIntercept, UnityEngine::MonoBehaviour, INTERFACES) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(void, OnPointerDown, &UES::IPointerDownHandler::OnPointerDown, UES::PointerEventData* eventData);
};

#undef INTERFACES
#undef UES
