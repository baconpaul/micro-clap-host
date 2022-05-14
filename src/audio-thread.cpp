//
// Created by Paul Walker on 5/14/22.
//

#include "micro-clap-host.h"
#include "RtAudio.h"

namespace micro_clap_host
{
clap_process_status audiothread_operate(audiothread_userdata *aud, uint32_t nSamples,
                                        double streamTime)
{
    for (const auto &g : aud->generators)
        g->process(aud, nSamples);

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

    // micro_clap_host::micro_input_events::push(&(aud->inEvents), valset);

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

    auto outE = micro_clap_host::micro_output_events::size(&(aud->outEvents));
    if (outE > 0)
    {
        // std::cout << "Ignoring output events. Count=" << outE << std::endl;
    }
    micro_clap_host::micro_output_events::reset(&(aud->outEvents));
    micro_clap_host::micro_input_events::reset(&(aud->inEvents));

    return res;
}

} // namespace micro_clap_host

int rtaudioToClap(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                  double streamTime, RtAudioStreamStatus status, void *userData)
{
    auto aud = (micro_clap_host::audiothread_userdata *)userData;
    auto clapstatus = micro_clap_host::audiothread_operate(aud, nBufferFrames, streamTime);

    // RTAudio has interleaved data
    float *buffer = (float *)(outputBuffer);
    for (int i = 0; i < nBufferFrames; ++i)
    {
        *buffer = aud->outBuffers->data32[0][i];
        buffer++;
        *buffer = aud->outBuffers->data32[1][i];
        buffer++;
    }

    if (clapstatus == CLAP_PROCESS_CONTINUE)
        return 0;
    else
        return 0;
}