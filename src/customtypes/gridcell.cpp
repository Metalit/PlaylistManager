#include "customtypes/gridcell.hpp"

#include "HMUI/Interactable.hpp"
#include "System/Single.hpp"
#include "UnityEngine/EventSystems/EventSystem.hpp"
#include "UnityEngine/EventSystems/PointerEventData.hpp"
#include "VRUIControls/ButtonState.hpp"
#include "VRUIControls/MouseButtonEventData.hpp"
#include "VRUIControls/MouseState.hpp"
#include "VRUIControls/VRInputModule.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "main.hpp"
#include "utils.hpp"

DEFINE_TYPE(PlaylistManager, GridCell);

using namespace PlaylistManager;

static constexpr UnityEngine::Color defaultColor = {1, 1, 1, 1};
static constexpr UnityEngine::Color highlightColor = {0.6, 0.8, 1, 1};

void GridCell::SetDragging(bool value) {
    isDragging = value;
    transform->localScale = value ? UnityEngine::Vector3(1.05, 1.05, 1.05) : UnityEngine::Vector3(1, 1, 1);
    auto hinter = BSML::Helpers::GetHoverHintController();
    hinter->gameObject->active = !value;
    if (layout)
        layout->ignoreLayout = value;
    if (value)
        transform->SetAsLastSibling();
}

bool GridCell::IsPosValid(UnityEngine::EventSystems::PointerEventData* eventData) {
    auto raycast = eventData->pointerCurrentRaycast;
    if (!raycast.isValid)
        return false;
    return raycast.screenPosition.x < System::Single::MaxValue || raycast.screenPosition.y < System::Single::MaxValue;
}

UnityEngine::Vector3 GridCell::GetPointerPos(UnityEngine::EventSystems::PointerEventData* eventData) {
    if (!canvas)
        canvas = GetComponentInParent<UnityEngine::Canvas*>();
    auto screenPos = eventData->pointerCurrentRaycast.screenPosition;
    auto pos = canvas->rootCanvas->transform->InverseTransformPoint({screenPos.x, screenPos.y, 0});
    pos.z = 0;
    return pos;
}

void GridCell::OnDisable() {
    if (isDragging)
        SetDragging(false);
    image->color = defaultColor;
}

void GridCell::OnInitializePotentialDrag(UnityEngine::EventSystems::PointerEventData* eventData) {
    // their drag threshold sucks so we do our own
    eventData->useDragThreshold = false;
    eventData->dragging = true;
    pointerGrab = UnityEngine::Vector3::op_Subtraction(transform->localPosition, GetPointerPos(eventData));
}

void GridCell::OnDrag(UnityEngine::EventSystems::PointerEventData* eventData) {
    if (!IsPosValid(eventData))
        return;
    auto pos = GetPointerPos(eventData);
    if (!isDragging) {
        auto diff = UnityEngine::Vector3::op_Addition(pos, pointerGrab);
        float dist = UnityEngine::Vector3::op_Subtraction(diff, transform->localPosition).sqrMagnitude;
        if (dist > threshold)
            SetDragging(true);
    }
    if (isDragging) {
        transform->localPosition = UnityEngine::Vector3::op_Addition(pos, pointerGrab);
        if (onDrag)
            onDrag();
    }
}

void GridCell::OnDrop(UnityEngine::EventSystems::PointerEventData* eventData) {
    bool wasDragging = isDragging;
    if (isDragging)
        SetDragging(false);
    if (wasDragging) {
        if (onEndDrag)
            onEndDrag();
    } else if (onClick)
        onClick();
}

void GridCell::OnPointerEnter(UnityEngine::EventSystems::PointerEventData* eventData) {
    image->color = highlightColor;
    if (eventData->pointerDrag)
        eventData->pointerPress = gameObject;
}

void GridCell::OnPointerExit(UnityEngine::EventSystems::PointerEventData* eventData) {
    image->color = defaultColor;
}

void GridCell::SetData(StringW hover, UnityEngine::Sprite* sprite) {
    image->sprite = sprite;
    hoverHint->text = hover;
}

GridCell* GridCell::Create(UnityEngine::Transform* parent, StringW hover, UnityEngine::Sprite* sprite) {
    auto image = BSML::Lite::CreateImage(parent, sprite, {}, {15, 15});
    image->material = Utils::GetCurvedCornersMaterial();
    image->name = "PlaylistManagerGridCell";
    auto ret = image->gameObject->AddComponent<GridCell*>();
    ret->image = image;
    ret->layout = image->GetComponent<UnityEngine::UI::LayoutElement*>();
    ret->hoverHint = image->gameObject->AddComponent<HMUI::HoverHint*>();
    ret->hoverHint->_hoverHintController = BSML::Helpers::GetHoverHintController();
    ret->hoverHint->text = hover;
    image->gameObject->AddComponent<HMUI::Interactable*>();
    return ret;
}
