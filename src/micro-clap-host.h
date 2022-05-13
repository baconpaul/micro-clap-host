//
// Created by Paul Walker on 5/13/22.
//

#ifndef MICRO_CLAP_HOST_MICRO_CLAP_HOST_H
#define MICRO_CLAP_HOST_MICRO_CLAP_HOST_H

#include <clap/entry.h>
#include <filesystem>

namespace micro_clap_host
{
    // This is a dlope/bundle/loadlibrary and is OS dependent. It is some of the
    // ONLY OS dependent code we have and is in resolve_entrypoint.cpp
    clap_plugin_entry_t *entryFromClapPath(const std::filesystem::path &p);
}

#endif // MICRO_CLAP_HOST_MICRO_CLAP_HOST_H
