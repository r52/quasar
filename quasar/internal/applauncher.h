#pragma once

#include "api/extension_types.h"

quasar_ext_info_t* applauncher_load(void);

void               applauncher_destroy(quasar_ext_info_t* info);
