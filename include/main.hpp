#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

#include "HMUI/ViewController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "bsml/shared/BSML/Components/HotReloadFileWatcher.hpp"

#ifdef HOT_RELOAD
inline void AddHotReload(HMUI::ViewController* host, char const* path) {
    auto watcher = host->gameObject->AddComponent<BSML::HotReloadFileWatcher*>();
    watcher->host = host;
    watcher->filePath = fmt::format("/sdcard/{}.bsml", path);
    watcher->checkInterval = 5;
    watcher->Reload();
}
#define BSML_FILE(name) AddHotReload(this, #name)
#else
#define BSML_FILE(name) BSML::parse_and_construct(IncludedAssets::name##_bsml, transform, this)
#endif
