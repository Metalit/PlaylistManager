#include "customtypes/playlistgrid.hpp"

#include "UnityEngine/UI/LayoutRebuilder.hpp"
#include "assets.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "customtypes/gridcell.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "playlistcore/shared/PlaylistCore.hpp"

DEFINE_TYPE(PlaylistManager, PlaylistGrid);

using namespace PlaylistManager;

static constexpr int minCols = 5;
static constexpr int maxCols = 7;
static constexpr int cellSize = 14;
static constexpr int cellGap = 1;

inline constexpr int roundUpDiv(int a, int b) {
    return (a + b - 1) / b;
}

constexpr int calcParts(int length) {
    return (length + cellGap) / (cellSize + cellGap);
}

constexpr int calcCols(int height, int cellNum) {
    int rowCount = calcParts(height);
    return std::clamp(roundUpDiv(cellNum, rowCount), minCols, maxCols);
}

constexpr float calcWidth(int colCount) {
    return colCount * cellSize + (colCount - 1) * cellGap;
}

constexpr int calcClosePart(float pos, int max) {
    pos += cellGap / (float) 2;
    int rounded = pos / (cellSize + cellGap);
    return std::clamp(rounded, 0, max);
}

constexpr UnityEngine::Vector3 calcOffset(UnityEngine::Vector3 pos, UnityEngine::Rect rect) {
    pos.x -= rect.m_XMin;
    pos.y -= rect.m_YMin;
    return pos;
}

void PlaylistGrid::OnEnable() {
    name = "PlaylistGrid";
    rectTransform->anchorMin = {0.5, 0.5};
    rectTransform->anchorMax = {0.5, 0.5};
    rectTransform->sizeDelta = {std::max(calcWidth(minCols), rectTransform->sizeDelta.x), 80};
}

void PlaylistGrid::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (firstActivation)
        BSML_FILE(playlistgrid);
    else
        Refresh();
    if (scroller)
        scroller->ScrollTo(0, false);
}

void PlaylistGrid::PostParse() {
    if (!grid || !addCell)
        return;
    grid->constraint = UnityEngine::UI::GridLayoutGroup::Constraint::FixedColumnCount;
    gridCells = ListW<GridCell*>::New();
    for (auto image : addCell->GetChild(0)->GetComponentsInChildren<HMUI::ImageView*>()) {
        image->_skew = 0;
        image->__Refresh();
    }
    Refresh();
}

void PlaylistGrid::OnDestroy() {
    instance = nullptr;
}

PlaylistGrid* PlaylistGrid::GetInstance() {
    if (!instance)
        instance = BSML::Helpers::CreateViewController<PlaylistGrid*>();
    return instance;
}

void PlaylistGrid::Refresh() {
    if (!grid || !addCell || !fakeCell || !scrollBar)
        return;

    auto playlists = PlaylistCore::GetLoadedPlaylists();
    int size = playlists.size();
    float height = rectTransform->sizeDelta.y;
    int cols = calcCols(height, size + 1);
    grid->constraintCount = cols;
    int rows = roundUpDiv(size + 1, cols);
    bool overflow = rows > calcParts(height);
    rectTransform->sizeDelta = {calcWidth(cols) + (overflow ? 8 : 0), height};
    scrollBar->anchoredPosition = {calcWidth(cols) / 2 + 1, 0};
    scrollBar->gameObject->active = overflow;

    usedCells = 0;
    for (int i = 0; i < playlists.size(); i++) {
        auto& playlist = playlists[i];
        auto cell = GetCell(playlist->name, PlaylistCore::GetCoverImage(playlist));
        auto transform = cell->transform;
        cell->onDrag = [this, transform, size]() mutable {
            fakeCell->gameObject->active = true;
            int idx = GetIndexForPos(transform->localPosition);
            fakeCell->SetSiblingIndex(std::clamp(idx, 0, size - 1));
        };
        cell->onEndDrag = [this, i, transform, size, playlist]() mutable {
            fakeCell->SetAsLastSibling();
            fakeCell->gameObject->active = false;
            transform->SetSiblingIndex(i);

            int idx = GetIndexForPos(transform->localPosition);
            idx = std::clamp(idx, 0, size - 1);
            logger.debug("end drag {} {}", playlist->name, idx);

            PlaylistCore::MovePlaylist(playlist, idx);
            Refresh();
            Manager::SetShouldReload();
        };
        cell->onClick = [this, playlist]() {
            Manager::SelectPlaylist(playlist);
        };
    }
    for (int i = usedCells; i < gridCells.size(); i++)
        gridCells[i]->gameObject->active = false;
    addCell->SetAsLastSibling();
    fakeCell->SetAsLastSibling();

    if (grid->isActiveAndEnabled)
        grid->StartCoroutine(grid->DelayedSetDirty(grid->rectTransform));
}

void PlaylistGrid::addClicked() {
    Manager::BeginAddition();
}

int PlaylistGrid::GetIndexForPos(UnityEngine::Vector3 pos) {
    if (!grid)
        return 0;
    pos = calcOffset(pos, grid->rectTransform->rect);
    auto size = grid->rectTransform->sizeDelta;
    auto cols = calcParts(size.x);
    auto rows = calcParts(size.y);
    int col = calcClosePart(pos.x, cols - 1);
    int row = calcClosePart(pos.y, rows - 1);
    // pos y goes up, grid goes down
    row = rows - row - 1;
    return col + row * cols;
}

GridCell* PlaylistGrid::GetCell(StringW name, UnityEngine::Sprite* image) {
    int idx = usedCells++;
    if (idx < gridCells.size()) {
        auto ret = gridCells[idx];
        ret->SetData(name, image);
        ret->gameObject->active = true;
        return ret;
    }
    auto ret = GridCell::Create(grid->rectTransform, name, image);
    gridCells->Add(ret);
    return ret;
}
