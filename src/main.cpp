#include "main.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "scotland2/shared/modloader.h"

static modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

extern "C" void setup(CModInfo* info) {
    info->id = MOD_ID;
    info->version = VERSION;
    modInfo.assign(*info);

    logger.info("Completed setup!");
}

extern "C" void late_load() {
    il2cpp_functions::Init();

    logger.info("Installing hooks...");
    logger.info("Installed all hooks!");
}
