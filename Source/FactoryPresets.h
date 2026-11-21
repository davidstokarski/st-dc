#pragma once
#include <st_shell/st_shell.h>

namespace stdc
{
/** Registers the built-in presets (called from the processor constructor). */
void registerFactoryPresets (st::PresetManager&);
}
