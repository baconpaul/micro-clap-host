//
// Created by Paul Walker on 5/13/22.
//

#ifndef MICRO_CLAP_HOST_MICRO_CLAP_HOST_H
#define MICRO_CLAP_HOST_MICRO_CLAP_HOST_H

#include <filesystem>
#include <cassert>
#include <unordered_map>
#include <list>

#include <clap/entry.h>
#include <clap/host.h>
#include <clap/events.h>

#include <clap/ext/params.h>
#include <clap/ext/audio-ports.h>

namespace micro_clap_host
{
// This is a dlope/bundle/loadlibrary and is OS dependent. It is some of the
// ONLY OS dependent code we have and is in resolve_entrypoint.cpp
clap_plugin_entry_t *entryFromClapPath(const std::filesystem::path &p);

clap_host_t *createMicroHost();

struct audiothread_userdata;

// An interface that, on the audio thread, we can call
// to generate audio thread events like notes or modulations
struct event_generator
{
    virtual ~event_generator() = default;
    virtual void process(audiothread_userdata *, uint32_t nSamples) = 0;
};

struct audiothread_userdata
{
    const clap_plugin_t *plugin;
    bool isStarted{false};
    int inPorts, outPorts;
    clap_audio_buffer *inBuffers, *outBuffers;

    clap_input_events_t inEvents;
    clap_output_events_t outEvents;

    std::unordered_map<uint32_t, clap_param_info> paramInfo;
    std::unordered_map<uint32_t, double> initialParamValues;
    std::list<std::unique_ptr<event_generator>> generators;
};

clap_process_status audiothread_operate(audiothread_userdata *, uint32_t nSamples,
                                        double streamTime);

struct micro_input_events
{
    static constexpr int max_evt_size = 10 * 1024;
    static constexpr int max_events = 4096;
    uint8_t data[max_evt_size * max_events];
    uint32_t sz{0};

    static void setup(clap_input_events *evt)
    {
        evt->ctx = new micro_input_events();
        evt->size = size;
        evt->get = get;
    }
    static void destroy(clap_input_events *evt) { delete (micro_input_events *)evt->ctx; }

    static uint32_t size(const clap_input_events *e)
    {
        auto mie = static_cast<micro_input_events *>(e->ctx);
        return mie->sz;
    }

    static const clap_event_header_t *get(const clap_input_events *e, uint32_t index)
    {
        auto mie = static_cast<micro_input_events *>(e->ctx);
        assert(index >= 0);
        assert(index < max_events);
        uint8_t *ptr = &(mie->data[index * max_evt_size]);
        return reinterpret_cast<clap_event_header_t *>(ptr);
    }

    template <typename T> static void push(clap_input_events *e, const T &t)
    {
        auto mie = static_cast<micro_input_events *>(e->ctx);
        assert(t.header.size <= max_evt_size);
        assert(mie->sz < max_events - 1);
        uint8_t *ptr = &(mie->data[mie->sz * max_evt_size]);
        memcpy(ptr, &t, t.header.size);
        mie->sz++;
    }

    static void reset(clap_input_events *e)
    {
        auto mie = static_cast<micro_input_events *>(e->ctx);
        mie->sz = 0;
    }
};

struct micro_output_events
{
    static constexpr int max_evt_size = 10 * 1024;
    static constexpr int max_events = 4096;
    uint8_t data[max_evt_size * max_events];
    uint32_t sz{0};

    static void setup(clap_output_events *evt)
    {
        evt->ctx = new micro_output_events();
        evt->try_push = try_push;
    }
    static void destroy(clap_output_events *evt) { delete (micro_output_events *)evt->ctx; }

    static bool try_push(const struct clap_output_events *list, const clap_event_header_t *event)
    {
        auto mie = static_cast<micro_output_events *>(list->ctx);
        if (mie->sz >= max_events || event->size >= max_evt_size)
            return false;

        uint8_t *ptr = &(mie->data[mie->sz * max_evt_size]);
        memcpy(ptr, event, event->size);
        mie->sz++;
        return true;
    }

    static uint32_t size(clap_output_events *e)
    {
        auto mie = static_cast<micro_output_events *>(e->ctx);
        return mie->sz;
    }

    static void reset(clap_output_events *e)
    {
        auto mie = static_cast<micro_output_events *>(e->ctx);
        mie->sz = 0;
    }
};

} // namespace micro_clap_host

#endif // MICRO_CLAP_HOST_MICRO_CLAP_HOST_H
