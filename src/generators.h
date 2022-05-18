//
// Created by Paul Walker on 5/14/22.
//

#ifndef MICRO_CLAP_HOST_GENERATORS_H
#define MICRO_CLAP_HOST_GENERATORS_H

#include "micro-clap-host.h"
#include <random>
#include <array>
#include <algorithm>
#include <utility>

namespace micro_clap_host
{
namespace generators
{
struct random_notes : micro_clap_host::event_generator
{
    uint32_t currentSample{0};
    std::array<int32_t, 127> noteIds;
    uint32_t numOn{0};
    std::default_random_engine generator{2112};
    std::uniform_real_distribution<float> distribution{0.f, 1.f};
    int32_t noteid{0};
    static constexpr int maxPoly = 4;

    random_notes()
    {
        for (auto &n : noteIds)
            n = -1;
    }

    inline float rn() { return distribution(generator); }
    void process(audiothread_userdata *aud, uint32_t nSamples) override
    {
        // assume sr like 44100 so 4410 or 4000 is about 1/10th of a second.
        // Randomly start a note about every that often but 4096 makes the math
        // faster so
        if ((currentSample & (4096 - 1)) > ((currentSample + nSamples) & (4096 - 1)))
        {
            if (rn() > 0.6)
            {
                // int chan = 0, port=0;
                auto evtType = CLAP_EVENT_NOTE_ON;
                auto postEvent = true;

                int nid = noteid++;
                int key = -1;
                if (numOn == 0 || (rn() > 0.5 && numOn < maxPoly))
                {
                    // NOTE ON
                    // int n = floor(rn() * 127);
                    int n = floor(rn() * 36) + 48; // 4 octaves mid keyboard
                    if (noteIds[n] >= 0)
                    {
                        postEvent = false;
                    }
                    else
                    {
                        noteIds[n] = nid;
                        numOn++;
                        key = n;
                    }
                }
                else
                {
                    evtType = CLAP_EVENT_NOTE_OFF;
                    // NOTE OFF
                    int which = std::clamp((uint32_t)(rn() * numOn), 0U, numOn - 1);

                    for (int i = 0; i < 127; ++i)
                    {
                        if (noteIds[i] >= 0)
                        {
                            if (which == 0)
                            {
                                key = i;
                                break;
                            }
                            which--;
                        }
                    }
                    if (key >= 0)
                    {
                        nid = noteIds[key];
                        noteIds[key] = -1;
                        numOn--;
                    }
                    else
                    {
                        postEvent = false;
                    }
                }
                if (postEvent)
                {
                    auto evt = clap_event_note();
                    evt.header.size = sizeof(clap_event_note);
                    evt.header.type = (uint16_t)evtType;
                    evt.header.time = 0; // we could scatter it throughout i guess
                    evt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    evt.header.flags = 0;
                    evt.port_index = 0;
                    evt.channel = 0;
                    evt.key = key;
                    evt.note_id = nid;
                    evt.velocity = 1.0;

                    micro_clap_host::micro_input_events::push(&(aud->inEvents), evt);
                }
            }
        }
        currentSample += nSamples;
    }
};

// sawtooth every 48k samples or so
struct sawtooth_01_param : micro_clap_host::event_generator
{
    uint32_t param_id;
    double depth;
    uint32_t currentSample{0};

    sawtooth_01_param(uint32_t pid, double d) : param_id(pid), depth(d) {}

    void process(audiothread_userdata *aud, uint32_t nSamples) override
    {
        for (auto s = 0U; s < nSamples; s += 64)
        {
            auto ts = (currentSample + s) % 48000;
            double phs = ts / 48000.0;
            double mod = phs * depth;

            double val = aud->initialParamValues[param_id] + mod;

            auto valset = clap_event_param_value();
            valset.header.size = sizeof(clap_event_param_value);
            valset.header.type = (uint16_t)CLAP_EVENT_PARAM_VALUE;
            valset.header.time = 0;
            valset.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            valset.header.flags = 0;
            valset.param_id = param_id;
            valset.note_id = -1;
            valset.port_index = -1;
            valset.channel = -1;
            valset.key = -1;
            valset.value = val;
            valset.cookie = aud->paramInfo[param_id].cookie;

            micro_clap_host::micro_input_events::push(&(aud->inEvents), valset);
        }

        currentSample += nSamples;
    }
};
} // namespace generators
} // namespace micro_clap_host

#endif // MICRO_CLAP_HOST_GENERATORS_H
