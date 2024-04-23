#include "customtypes/levelcell.hpp"

#include "HMUI/ScrollView.hpp"
#include "System/Single.hpp"
#include "UnityEngine/EventSystems/PointerEventData.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "UnityEngine/UI/RectMask2D.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "customtypes/tablecallbacks.hpp"
#include "main.hpp"

DEFINE_TYPE(PlaylistManager, LevelCell);

using namespace PlaylistManager;

void LevelCell::SetDragging(bool value) {
    isDragging = value;
    auto hinter = BSML::Helpers::GetHoverHintController();
    hinter->gameObject->active = !value;
    if (value)
        transform->SetAsLastSibling();
    if (!cell)
        return;
    transform->localScale = value ? UnityEngine::Vector3(1.05, 1.05, 1.05) : UnityEngine::Vector3(1, 1, 1);
    cell->_backgroundImage->maskable = !value;
    cell->_coverImage->maskable = !value;
    cell->transform->Find("BpmIcon")->GetComponent<UnityEngine::UI::Image*>()->maskable = !value;
    if (auto mask = cell->GetComponentInParent<UnityEngine::UI::RectMask2D*>())
        mask->OnEnable();
}

bool LevelCell::IsPosValid(UnityEngine::EventSystems::PointerEventData* eventData) {
    auto raycast = eventData->pointerCurrentRaycast;
    if (!raycast.isValid)
        return false;
    return raycast.screenPosition.x < System::Single::MaxValue || raycast.screenPosition.y < System::Single::MaxValue;
}

UnityEngine::Vector3 LevelCell::GetPointerPos(UnityEngine::EventSystems::PointerEventData* eventData) {
    auto pos = transform->parent->InverseTransformPoint(eventData->pointerCurrentRaycast.worldPosition);
    pos.z = 0;
    return pos;
}

HMUI::TableView* LevelCell::GetTable() {
    if (!cell)
        return nullptr;
    return (HMUI::TableView*) cell->tableCellOwner;
}

int LevelCell::GetHoveredIndex() {
    auto table = GetTable();
    if (!table)
        return 0;
    float size = table->cellSize;
    float pos = -transform->localPosition.y;
    int ret = (pos + size / 2) / size;
    return std::clamp(ret, 0, table->numberOfCells - 1);
}

void LevelCell::Awake() {
    cell = GetComponent<GlobalNamespace::LevelListTableCell*>();
}

void LevelCell::OnDisable() {
    if (isDragging)
        SetDragging(false);
}

void LevelCell::OnInitializePotentialDrag(UnityEngine::EventSystems::PointerEventData* eventData) {
    // their drag threshold sucks so we do our own
    eventData->useDragThreshold = false;
    eventData->dragging = true;
    pointerGrab = UnityEngine::Vector3::op_Subtraction(transform->localPosition, GetPointerPos(eventData));
    if (cell)
        originalCellIdx = cell->idx;
}

void LevelCell::OnDrag(UnityEngine::EventSystems::PointerEventData* eventData) {
    if (!IsPosValid(eventData))
        return;
    auto pos = GetPointerPos(eventData);
    if (!isDragging) {
        auto diff = UnityEngine::Vector3::op_Addition(pos, pointerGrab);
        float dist = UnityEngine::Vector3::op_Subtraction(diff, transform->localPosition).sqrMagnitude;
        if (dist > threshold)
            SetDragging(true);
    }
    auto table = GetTable();
    if (isDragging && table) {
        float cellSize = table->cellSize;
        // clamp drag to inside visible content
        float y = UnityEngine::Vector3::op_Addition(pos, pointerGrab).y;
        float max = -table->scrollView->contentTransform->anchoredPosition.y;
        float min = max - table->scrollView->scrollPageSize + cellSize;
        transform->localPosition = {0, std::clamp(y, min, max), 0};
        // scroll if within cell of end
        if (y < min + (cellSize * 0.5))
            table->scrollView->HandleJoystickWasNotCenteredThisFrame({0, -0.8});
        else if (y > max - (cellSize * 0.5))
            table->scrollView->HandleJoystickWasNotCenteredThisFrame({0, 0.8});
        else
            table->scrollView->HandleJoystickWasCenteredThisFrame();
        // update cell index to avoid it being hidden by scrolling
        int posIdx = GetHoveredIndex();
        cell->idx = posIdx;
        // push the other cells around
        for (auto& visible : ListW<HMUI::TableCell*>(table->_visibleCells)) {
            int idx = visible->idx;
            if (visible == cell)
                continue;
            if (idx == originalCellIdx) {
                visible->gameObject->active = false;
                continue;
            }
            if (originalCellIdx > posIdx && idx >= posIdx && idx < originalCellIdx)
                idx++;
            else if (originalCellIdx < posIdx && idx > originalCellIdx && idx <= posIdx)
                idx--;
            table->LayoutCellForIdx(visible, idx, 0);
        }
        transform->SetAsLastSibling();
    }
}

void LevelCell::OnDrop(UnityEngine::EventSystems::PointerEventData* eventData) {
    bool wasDragging = isDragging;
    if (isDragging)
        SetDragging(false);
    if (wasDragging) {
        auto table = GetTable();
        if (!table)
            return;
        int idx = GetHoveredIndex();
        table->LayoutCellForIdx(cell, idx, 0);
        auto handler = table->GetComponent<TableCallbacks*>();
        if (handler && handler->onCellDragReordered)
            handler->onCellDragReordered(originalCellIdx, idx);
        table->scrollView->HandleJoystickWasCenteredThisFrame();
    } else if (cell)
        cell->InternalToggle();
}
