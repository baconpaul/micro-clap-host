//
// Created by Paul Walker on 5/14/22.
//

#include "micro-clap-host.h"

namespace micro_clap_host
{
clap_process_status audiothread_operate(audiothread_userdata *aud, uint32_t nSamples,
                                        double streamTime)
{
    int ptt2 = (int)(aud->priorTime * 2);
    int ttt2 = (int)(streamTime * 2);

    if (ptt2 != ttt2)
    {
        auto evt = clap_event_note();
        evt.header.size = sizeof(clap_event_note);
        evt.header.type = (uint16_t)CLAP_EVENT_NOTE_ON;
        evt.header.time = 0;
        evt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        evt.header.flags = 0;
        evt.port_index = 0;
        evt.channel = 0;
        evt.key = 60;
        evt.velocity = 1.0;

        if (ttt2 % 2 == 1)
        {
            evt.header.type = (uint16_t)CLAP_EVENT_NOTE_OFF;
        }

        micro_clap_host::micro_input_events::push(&(aud->inEvents), evt);
    }

    // I happen to know (from the printout) that surge param 0xa661c071 is
    // the sine oscillator pitch so lets just mod that. A more robust host should
    // check if something can be set or mod
    auto valset = clap_event_param_value();
    valset.header.size = sizeof(clap_event_param_value);
    valset.header.type = (uint16_t)CLAP_EVENT_PARAM_VALUE;
    valset.header.time = 0;
    valset.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    valset.header.flags = 0;
    valset.param_id = 0xa661c071;
    valset.note_id = -1;
    valset.port_index = -1;
    valset.channel = -1;
    valset.key = -1;
    valset.value = streamTime - (int)streamTime;
    valset.cookie = aud->paramInfo[0xa661c071].cookie;

    micro_clap_host::micro_input_events::push(&(aud->inEvents), valset);

    if (!aud->isStarted)
    {
        aud->plugin->start_processing(aud->plugin);
        aud->isStarted = true;
    }

    clap_process_t process;
    process.steady_time = -1;
    process.frames_count = nSamples;
    process.transport = nullptr; // we do need to fix this

    process.audio_inputs = aud->inBuffers;
    process.audio_inputs_count = aud->inPorts;
    process.audio_outputs = aud->outBuffers;
    process.audio_outputs_count = aud->outPorts;

    process.in_events = &(aud->inEvents);
    process.out_events = &(aud->outEvents);

    auto res = aud->plugin->process(aud->plugin, &process);

    micro_clap_host::micro_input_events::reset(&(aud->inEvents));

    aud->priorTime = streamTime;
    return res;
}
}
