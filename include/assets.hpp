#pragma once

#include <string_view>

#include "beatsaber-hook/shared/utils/typedefs.h"

struct IncludedAsset {

    IncludedAsset(uint8_t* start, uint8_t* end) : array(reinterpret_cast<Array<uint8_t>*>(start)) {
        array->klass = nullptr;
        array->monitor = nullptr;
        array->bounds = nullptr;
        array->max_length = end - start - 32;
        *(end - 1) = '\0';
    }

    operator ArrayW<uint8_t>() const {
        init();
        return array;
    }

    operator std::string_view() const { return {reinterpret_cast<char*>(array->_values), array->get_Length()}; }

    operator std::span<uint8_t>() const { return {array->_values, array->get_Length()}; }

    void init() const {
        if (!array->klass)
            array->klass = classof(Array<uint8_t>*);
    }

   private:
    Array<uint8_t>* array;
};

#define DECLARE_FILE(name)                       \
    extern "C" uint8_t _binary_##name##_start[]; \
    extern "C" uint8_t _binary_##name##_end[];   \
    const IncludedAsset name {_binary_##name##_start, _binary_##name##_end};

#define PNG_SPRITE(name) \
    BSML::Utilities::LoadSpriteRaw(static_cast<ArrayW<uint8_t>>(IncludedAssets::name##_png))

namespace IncludedAssets {
    DECLARE_FILE(allsongs_bsml)
    DECLARE_FILE(clear_download_png)
    DECLARE_FILE(clear_highlight_png)
    DECLARE_FILE(clear_sync_png)
    DECLARE_FILE(delete_png)
    DECLARE_FILE(download_png)
    DECLARE_FILE(edit_png)
    DECLARE_FILE(link_png)
    DECLARE_FILE(options_png)
    DECLARE_FILE(playlistgrid_bsml)
    DECLARE_FILE(playlistinfo_bsml)
    DECLARE_FILE(playlistsongs_bsml)
    DECLARE_FILE(reset_png)
    DECLARE_FILE(save_png)
    DECLARE_FILE(save_edit_png)
    DECLARE_FILE(sync_png)
    DECLARE_FILE(unlink_png)
}

#undef DECLARE_FILE
