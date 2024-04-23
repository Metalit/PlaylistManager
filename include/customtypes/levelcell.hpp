#pragma once

#include "custom-types/shared/macros.hpp"

#include "GlobalNamespace/LevelListTableCell.hpp"
#include "HMUI/TableView.hpp"
#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/EventSystems/IDragHandler.hpp"
#include "UnityEngine/EventSystems/IDropHandler.hpp"
#include "UnityEngine/EventSystems/IInitializePotentialDragHandler.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Vector3.hpp"

#define UES UnityEngine::EventSystems

#define INTERFACES std::vector<Il2CppClass*>({ \
    classof(UES::IInitializePotentialDragHandler*), \
    classof(UES::IDragHandler*), \
    classof(UES::IDropHandler*), \
})

DECLARE_CLASS_CODEGEN_INTERFACES(PlaylistManager, LevelCell, UnityEngine::MonoBehaviour, INTERFACES,
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_FIELD(bool, isDragging);
    DECLARE_INSTANCE_FIELD_DEFAULT(float, threshold, 15);
    DECLARE_INSTANCE_METHOD(void, SetDragging, bool value);
    DECLARE_INSTANCE_FIELD(UnityEngine::Vector3, pointerGrab);
    DECLARE_INSTANCE_METHOD(bool, IsPosValid, UES::PointerEventData* eventData);
    DECLARE_INSTANCE_METHOD(UnityEngine::Vector3, GetPointerPos, UES::PointerEventData* eventData);
    DECLARE_INSTANCE_METHOD(HMUI::TableView*, GetTable);
    DECLARE_INSTANCE_FIELD(int, originalCellIdx);
    DECLARE_INSTANCE_METHOD(int, GetHoveredIndex);

    DECLARE_INSTANCE_METHOD(void, Awake);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_OVERRIDE_METHOD_MATCH(void, OnInitializePotentialDrag, &UES::IInitializePotentialDragHandler::OnInitializePotentialDrag, UES::PointerEventData* eventData);
    DECLARE_OVERRIDE_METHOD_MATCH(void, OnDrag, &UES::IDragHandler::OnDrag, UES::PointerEventData* eventData);
    DECLARE_OVERRIDE_METHOD_MATCH(void, OnDrop, &UES::IDropHandler::OnDrop, UES::PointerEventData* eventData);

    DECLARE_INSTANCE_FIELD(GlobalNamespace::LevelListTableCell*, cell);
)

#undef INTERFACES
#undef UES
