#include "version_info.h"
#include "firmware_version.h"

void initVersionInfo() {}

VersionInfoSnapshot getVersionInfo() {
    return {
        firmwareVersion(),
        firmwareVersion(),
        "",
        "",
        false,
        true,
        true,
        false
    };
}
