#pragma once

#include "GlobalNamespace/LevelPackDetailViewController.hpp"
#include "GlobalNamespace/StandardLevelDetailViewController.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"

namespace Shortcuts {
    void RefreshPlaylistList();

    void RefreshLevelShortcuts(GlobalNamespace::StandardLevelDetailViewController* levelDetail);

    void RefreshPackShortcuts(GlobalNamespace::LevelPackDetailViewController* packDetail);

    void RefreshCreateShortcuts(GlobalNamespace::LevelFilteringNavigationController* packsView);

    void Invalidate();
}
