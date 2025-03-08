#pragma once

#include "metacore/shared/assets.hpp"

#define DECLARE_ASSET(name, binary)       \
    const IncludedAsset name {            \
        Externs::_binary_##binary##_start, \
        Externs::_binary_##binary##_end    \
    };

#define DECLARE_ASSET_NS(namespaze, name, binary) \
    namespace namespaze { DECLARE_ASSET(name, binary) }

namespace IncludedAssets {
    namespace Externs {
        extern "C" uint8_t _binary_allsongs_bsml_start[];
        extern "C" uint8_t _binary_allsongs_bsml_end[];
        extern "C" uint8_t _binary_clear_download_png_start[];
        extern "C" uint8_t _binary_clear_download_png_end[];
        extern "C" uint8_t _binary_clear_highlight_png_start[];
        extern "C" uint8_t _binary_clear_highlight_png_end[];
        extern "C" uint8_t _binary_clear_sync_png_start[];
        extern "C" uint8_t _binary_clear_sync_png_end[];
        extern "C" uint8_t _binary_delete_png_start[];
        extern "C" uint8_t _binary_delete_png_end[];
        extern "C" uint8_t _binary_download_png_start[];
        extern "C" uint8_t _binary_download_png_end[];
        extern "C" uint8_t _binary_edit_png_start[];
        extern "C" uint8_t _binary_edit_png_end[];
        extern "C" uint8_t _binary_link_png_start[];
        extern "C" uint8_t _binary_link_png_end[];
        extern "C" uint8_t _binary_options_png_start[];
        extern "C" uint8_t _binary_options_png_end[];
        extern "C" uint8_t _binary_playlistgrid_bsml_start[];
        extern "C" uint8_t _binary_playlistgrid_bsml_end[];
        extern "C" uint8_t _binary_playlistinfo_bsml_start[];
        extern "C" uint8_t _binary_playlistinfo_bsml_end[];
        extern "C" uint8_t _binary_playlistsongs_bsml_start[];
        extern "C" uint8_t _binary_playlistsongs_bsml_end[];
        extern "C" uint8_t _binary_reset_png_start[];
        extern "C" uint8_t _binary_reset_png_end[];
        extern "C" uint8_t _binary_save_png_start[];
        extern "C" uint8_t _binary_save_png_end[];
        extern "C" uint8_t _binary_save_edit_png_start[];
        extern "C" uint8_t _binary_save_edit_png_end[];
        extern "C" uint8_t _binary_sync_png_start[];
        extern "C" uint8_t _binary_sync_png_end[];
        extern "C" uint8_t _binary_unlink_png_start[];
        extern "C" uint8_t _binary_unlink_png_end[];
    }

    // allsongs.bsml
    DECLARE_ASSET(allsongs_bsml, allsongs_bsml);
    // clear_download.png
    DECLARE_ASSET(clear_download_png, clear_download_png);
    // clear_highlight.png
    DECLARE_ASSET(clear_highlight_png, clear_highlight_png);
    // clear_sync.png
    DECLARE_ASSET(clear_sync_png, clear_sync_png);
    // delete.png
    DECLARE_ASSET(delete_png, delete_png);
    // download.png
    DECLARE_ASSET(download_png, download_png);
    // edit.png
    DECLARE_ASSET(edit_png, edit_png);
    // link.png
    DECLARE_ASSET(link_png, link_png);
    // options.png
    DECLARE_ASSET(options_png, options_png);
    // playlistgrid.bsml
    DECLARE_ASSET(playlistgrid_bsml, playlistgrid_bsml);
    // playlistinfo.bsml
    DECLARE_ASSET(playlistinfo_bsml, playlistinfo_bsml);
    // playlistsongs.bsml
    DECLARE_ASSET(playlistsongs_bsml, playlistsongs_bsml);
    // reset.png
    DECLARE_ASSET(reset_png, reset_png);
    // save.png
    DECLARE_ASSET(save_png, save_png);
    // save_edit.png
    DECLARE_ASSET(save_edit_png, save_edit_png);
    // sync.png
    DECLARE_ASSET(sync_png, sync_png);
    // unlink.png
    DECLARE_ASSET(unlink_png, unlink_png);
}
