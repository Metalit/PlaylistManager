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
        extern "C" uint8_t _binary_bsml_allsongs_bsml_start[];
        extern "C" uint8_t _binary_bsml_allsongs_bsml_end[];
        extern "C" uint8_t _binary_bsml_playlistgrid_bsml_start[];
        extern "C" uint8_t _binary_bsml_playlistgrid_bsml_end[];
        extern "C" uint8_t _binary_bsml_playlistinfo_bsml_start[];
        extern "C" uint8_t _binary_bsml_playlistinfo_bsml_end[];
        extern "C" uint8_t _binary_bsml_playlistsongs_bsml_start[];
        extern "C" uint8_t _binary_bsml_playlistsongs_bsml_end[];
        extern "C" uint8_t _binary_icons_clear_download_png_start[];
        extern "C" uint8_t _binary_icons_clear_download_png_end[];
        extern "C" uint8_t _binary_icons_clear_highlight_png_start[];
        extern "C" uint8_t _binary_icons_clear_highlight_png_end[];
        extern "C" uint8_t _binary_icons_clear_sync_png_start[];
        extern "C" uint8_t _binary_icons_clear_sync_png_end[];
        extern "C" uint8_t _binary_icons_delete_png_start[];
        extern "C" uint8_t _binary_icons_delete_png_end[];
        extern "C" uint8_t _binary_icons_download_png_start[];
        extern "C" uint8_t _binary_icons_download_png_end[];
        extern "C" uint8_t _binary_icons_edit_png_start[];
        extern "C" uint8_t _binary_icons_edit_png_end[];
        extern "C" uint8_t _binary_icons_link_png_start[];
        extern "C" uint8_t _binary_icons_link_png_end[];
        extern "C" uint8_t _binary_icons_options_png_start[];
        extern "C" uint8_t _binary_icons_options_png_end[];
        extern "C" uint8_t _binary_icons_reset_png_start[];
        extern "C" uint8_t _binary_icons_reset_png_end[];
        extern "C" uint8_t _binary_icons_save_png_start[];
        extern "C" uint8_t _binary_icons_save_png_end[];
        extern "C" uint8_t _binary_icons_save_edit_png_start[];
        extern "C" uint8_t _binary_icons_save_edit_png_end[];
        extern "C" uint8_t _binary_icons_sync_png_start[];
        extern "C" uint8_t _binary_icons_sync_png_end[];
        extern "C" uint8_t _binary_icons_unlink_png_start[];
        extern "C" uint8_t _binary_icons_unlink_png_end[];
    }

    // bsml/allsongs.bsml
    DECLARE_ASSET_NS(bsml, allsongs_bsml, bsml_allsongs_bsml);
    // bsml/playlistgrid.bsml
    DECLARE_ASSET_NS(bsml, playlistgrid_bsml, bsml_playlistgrid_bsml);
    // bsml/playlistinfo.bsml
    DECLARE_ASSET_NS(bsml, playlistinfo_bsml, bsml_playlistinfo_bsml);
    // bsml/playlistsongs.bsml
    DECLARE_ASSET_NS(bsml, playlistsongs_bsml, bsml_playlistsongs_bsml);
    // icons/clear_download.png
    DECLARE_ASSET_NS(icons, clear_download_png, icons_clear_download_png);
    // icons/clear_highlight.png
    DECLARE_ASSET_NS(icons, clear_highlight_png, icons_clear_highlight_png);
    // icons/clear_sync.png
    DECLARE_ASSET_NS(icons, clear_sync_png, icons_clear_sync_png);
    // icons/delete.png
    DECLARE_ASSET_NS(icons, delete_png, icons_delete_png);
    // icons/download.png
    DECLARE_ASSET_NS(icons, download_png, icons_download_png);
    // icons/edit.png
    DECLARE_ASSET_NS(icons, edit_png, icons_edit_png);
    // icons/link.png
    DECLARE_ASSET_NS(icons, link_png, icons_link_png);
    // icons/options.png
    DECLARE_ASSET_NS(icons, options_png, icons_options_png);
    // icons/reset.png
    DECLARE_ASSET_NS(icons, reset_png, icons_reset_png);
    // icons/save.png
    DECLARE_ASSET_NS(icons, save_png, icons_save_png);
    // icons/save_edit.png
    DECLARE_ASSET_NS(icons, save_edit_png, icons_save_edit_png);
    // icons/sync.png
    DECLARE_ASSET_NS(icons, sync_png, icons_sync_png);
    // icons/unlink.png
    DECLARE_ASSET_NS(icons, unlink_png, icons_unlink_png);
}
