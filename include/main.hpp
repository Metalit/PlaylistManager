#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

#include "HMUI/ViewController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "bsml/shared/BSML/Components/HotReloadFileWatcher.hpp"

inline void AddHotReload(HMUI::ViewController* host, char const* path) {
    auto watcher = host->get_gameObject()->AddComponent<BSML::HotReloadFileWatcher*>();
    watcher->host = host;
    watcher->filePath = fmt::format("/sdcard/{}.bsml", path);
    watcher->checkInterval = 5;
}
