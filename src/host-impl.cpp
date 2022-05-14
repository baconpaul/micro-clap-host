//
// Created by Paul Walker on 5/13/22.
//

#include <iostream>
#include "micro-clap-host.h"

namespace micro_clap_host
{

const void *get_extension(const struct clap_host *host, const char *eid)
{
    std::cout << "Requesting Extension " << eid << std::endl;
    return nullptr;
}

void request_restart(const struct clap_host *h) {}

void request_process(const struct clap_host *h) {}

void request_callback(const struct clap_host *h) {}

static clap_host micro_host_static{
    CLAP_VERSION_INIT, nullptr,       "MicroHost",     "BaconPaul for now", "https://baconpaul.org",
    "0.0.0",           get_extension, request_restart, request_process,     request_callback};

clap_host_t *createMicroHost() { return &micro_host_static; }
} // namespace micro_clap_host