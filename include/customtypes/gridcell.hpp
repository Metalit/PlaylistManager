#pragma once

#include "HMUI/HoverHint.hpp"
#include "HMUI/ImageView.hpp"
#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/EventSystems/IDragHandler.hpp"
#include "UnityEngine/EventSystems/IEndDragHandler.hpp"
#include "UnityEngine/EventSystems/IInitializePotentialDragHandler.hpp"
#include "UnityEngine/EventSystems/IPointerEnterHandler.hpp"
#include "UnityEngine/EventSystems/IPointerExitHandler.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "UnityEngine/Vector3.hpp"
#include "custom-types/shared/macros.hpp"

#define UES UnityEngine::EventSystems

DECLARE_CLASS_CODEGEN_INTERFACES(PlaylistManager, GridCell, UnityEngine::MonoBehaviour, UES::IInitializePotentialDragHandler*, UES::IDragHandler*, UES::IEndDragHandler*, UES::IPointerEnterHandler*, UES::IPointerExitHandler*) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_FIELD(bool, isDragging);
    DECLARE_INSTANCE_FIELD_DEFAULT(float, threshold, 14);
    DECLARE_INSTANCE_METHOD(void, SetDragging, bool value);
    DECLARE_INSTANCE_FIELD(UnityEngine::Vector3, pointerGrab);
    DECLARE_INSTANCE_METHOD(bool, IsPosValid, UES::PointerEventData* eventData);
    DECLARE_INSTANCE_METHOD(UnityEngine::Vector3, GetPointerPos, UES::PointerEventData * eventData);

    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_OVERRIDE_METHOD_MATCH(
        void, OnInitializePotentialDrag, &UES::IInitializePotentialDragHandler::OnInitializePotentialDrag, UES::PointerEventData* eventData
    );
    DECLARE_OVERRIDE_METHOD_MATCH(void, OnDrag, &UES::IDragHandler::OnDrag, UES::PointerEventData* eventData);
    DECLARE_OVERRIDE_METHOD_MATCH(void, OnEndDrag, &UES::IEndDragHandler::OnEndDrag, UES::PointerEventData* eventData);
    DECLARE_OVERRIDE_METHOD_MATCH(void, OnPointerEnter, &UES::IPointerEnterHandler::OnPointerEnter, UES::PointerEventData* eventData);
    DECLARE_OVERRIDE_METHOD_MATCH(void, OnPointerExit, &UES::IPointerExitHandler::OnPointerExit, UES::PointerEventData* eventData);

    DECLARE_INSTANCE_FIELD(HMUI::HoverHint*, hoverHint);
    DECLARE_INSTANCE_FIELD(HMUI::ImageView*, selectionImage);
    DECLARE_INSTANCE_FIELD(HMUI::ImageView*, spriteImage);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::LayoutElement*, layout);
    DECLARE_INSTANCE_FIELD(UnityEngine::Canvas*, canvas);
    DECLARE_INSTANCE_METHOD(void, SetData, StringW hover = "", UnityEngine::Sprite* sprite = nullptr);

   public:
    static GridCell* Create(UnityEngine::Transform * parent, StringW hover = "", UnityEngine::Sprite* sprite = nullptr);

    std::function<void()> onClick = nullptr;
    std::function<void()> onDrag = nullptr;
    std::function<void()> onEndDrag = nullptr;
};

#undef UES
