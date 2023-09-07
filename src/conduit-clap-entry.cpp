/*
 * Conduit - a series of demonstration and fun plugins
 *
 * Copyright 2023 Paul Walker and authors in github
 *
 * This file you are viewing now is released under the
 * MIT license, but the assembled program which results
 * from compiling it has GPL3 dependencies, so the total
 * program is a GPL3 program. More details to come.
 *
 * Basically before I give this to folks, document this bit and
 * replace these headers
 *
 */

/*
 * This file provides the `clap_plugin_entry` entry point required in the DLL for all
 * clap plugins. It also provides the basic functions for the resulting factory class
 * which generates the plugin. In a single plugin case, this really is just plumbing
 * through to expose polysynth::desc and create a polysynth plugin instance using
 * the helper classes.
 *
 * For more information on this mechanism, see include/clap/entry.h
 */

#include <iostream>
#include <cmath>
#include <cstring>

#include <clap/plugin.h>
#include <clap/factory/plugin-factory.h>

#include "conduit-shared/debug-helpers.h"

#include "polysynth/polysynth.h"
#include "polymetric-delay/polymetric-delay.h"
#include "chord-memory/chord-memory.h"

namespace sst::conduit::pluginentry
{

uint32_t clap_get_plugin_count(const clap_plugin_factory *f) { return 3; }
const clap_plugin_descriptor *clap_get_plugin_descriptor(const clap_plugin_factory *f, uint32_t w)
{
    CNDOUT << "Asking for clap plugin number " << w << std::endl;
    if (w == 0)
        return &polysynth::desc;
    if (w == 1)
        return &polymetric_delay::desc;
    if (w == 2)
        return &chord_memory::desc;

    CNDOUT << "Clap Plugin not found at " << w << std::endl;
    return nullptr;
}

static const clap_plugin *clap_create_plugin(const clap_plugin_factory *f, const clap_host *host,
                                             const char *plugin_id)
{
    CNDOUT << "Creating clap plugin " << plugin_id << std::endl;
    if (strcmp(plugin_id, polysynth::desc.id) == 0)
    {
        auto p = new polysynth::ConduitPolysynth(host);
        return p->clapPlugin();
    }
    if (strcmp(plugin_id, polymetric_delay::desc.id) == 0)
    {
        auto p = new polymetric_delay::ConduitPolymetricDelay(host);
        return p->clapPlugin();
    }
    if (strcmp(plugin_id, chord_memory::desc.id) == 0)
    {
        auto p = new chord_memory::ConduitChordMemory(host);
        return p->clapPlugin();
    }

    CNDOUT << "No plugin found; returning nullptr" << std::endl;
    return nullptr;
}

const struct clap_plugin_factory conduit_polysynth_factory = {
    sst::conduit::pluginentry::clap_get_plugin_count,
    sst::conduit::pluginentry::clap_get_plugin_descriptor,
    sst::conduit::pluginentry::clap_create_plugin,
};
static const void *get_factory(const char *factory_id)
{
    return (!strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID)) ? &conduit_polysynth_factory : nullptr;
}

// clap_init and clap_deinit are required to be fast, but we have nothing we need to do here
bool clap_init(const char *p) { return true; }
void clap_deinit() {}

} // namespace sst::conduit::pluginentry

extern "C"
{
    // clang-format off
    const CLAP_EXPORT struct clap_plugin_entry clap_entry = {
        CLAP_VERSION,
        sst::conduit::pluginentry::clap_init,
        sst::conduit::pluginentry::clap_deinit,
        sst::conduit::pluginentry::get_factory
    };
    // clang-format on
}
