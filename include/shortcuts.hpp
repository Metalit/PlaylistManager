#pragma once

#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/LevelPackDetailViewController.hpp"
#include "GlobalNamespace/StandardLevelDetailViewController.hpp"

namespace Shortcuts {
    void RefreshPlaylistList();

    void RefreshLevelShortcuts(GlobalNamespace::StandardLevelDetailViewController* levelDetail);

    void RefreshPackShortcuts(GlobalNamespace::LevelPackDetailViewController* packDetail);

    void RefreshCreateShortcuts(GlobalNamespace::LevelFilteringNavigationController* packsView);

    void Invalidate();
}
