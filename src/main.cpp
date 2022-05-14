#include <iostream>
#include <filesystem>

#include "micro-clap-host.h"

#include "RtAudio.h"

#include <clap/plugin-factory.h>
#include <clap/ext/params.h>
#include <clap/ext/audio-ports.h>

#include <unordered_map>

struct rtaudio_userdata
{
    const clap_plugin_t *plugin;
    bool isStarted{false};
    int inPorts, outPorts;
    clap_audio_buffer *inBuffers, *outBuffers;

    clap_input_events_t inEvents;
    clap_output_events_t outEvents;

    double priorTime{-1};

    std::unordered_map<uint32_t, clap_param_info> paramInfo;
};

int processLoop( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
        double streamTime, RtAudioStreamStatus status, void *userData )
{
    auto aud = (rtaudio_userdata *)userData;

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
    // check if something can be set
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
    process.frames_count = nBufferFrames;
    process.transport = nullptr; // we do need to fix this



    process.audio_inputs = aud->inBuffers;
    process.audio_inputs_count = aud->inPorts;
    process.audio_outputs = aud->outBuffers;
    process.audio_outputs_count = aud->outPorts;

    process.in_events = &(aud->inEvents);
    process.out_events = &(aud->outEvents);

    aud->plugin->process(aud->plugin, &process);

    micro_clap_host::micro_input_events::reset(&(aud->inEvents));

    // RTAudio has interleaved data
    float *buffer = (float *)(outputBuffer);
    for (int i=0; i<nBufferFrames; ++i)
    {
        *buffer = aud->outBuffers->data32[0][i];
        buffer ++;
        *buffer = aud->outBuffers->data32[1][i];
        buffer ++;
    }

    aud->priorTime = streamTime;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "USAGE: " << argv[0] << " path-to-clap" << std::endl;
        exit(1);
    }
    std::string clap = argv[1];
    auto clapPath = std::filesystem::path(clap);

    if (!(std::filesystem::is_directory(clapPath) || std::filesystem::is_regular_file(clapPath)))
    {
        std::cout << "Your file '" << clap << "' is neither a bundle nor a file" << std::endl;
        exit(2);
    }

    std::cout << "Loading clap        : " << clap << std::endl;

    auto entry = micro_clap_host::entryFromClapPath(clapPath);

    if (!entry)
    {
        std::cout << "Got a null entru " << std::endl;
        exit(3);
    }

    auto version = entry->clap_version;
    std::cout << "Clap Version        : " << version.major << "." << version.minor << "." << version.revision << std::endl;

    entry->init(clap.c_str());

    auto fac = (clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    auto plugin_count = fac->get_plugin_count(fac);
    if (plugin_count <= 0)
    {
        std::cout << "Plugin factory has no plugins" << std::endl;
        exit(4);
    }

    // FIXME - what about multiplugin? for now just grab 0
    auto desc = fac->get_plugin_descriptor(fac, 0);

    std::cout << "Plugin description: \n"
              << "   name     : " << desc->name << "\n"
              << "   version  : " << desc->version << "\n"
              << "   id       : " << desc->id << "\n"
              << "   desc     : " << desc->description << "\n"
              << "   features : ";
    auto f = desc->features;
    auto pre = std::string();
    while( f[0] )
    {
        std::cout << pre   << f[0];
        pre = ", ";
        f++;
    }
    std::cout << std::endl;

    // Now lets make an instance
    auto host = micro_clap_host::createMicroHost();
    auto inst = fac->create_plugin(fac, host, desc->id);

    rtaudio_userdata aud;
    aud.plugin = inst;

    // At this point we need a samplerate so get our audio info
    RtAudio audio;
    auto audioout = audio.getDefaultOutputDevice();
    auto audioinfo = audio.getDeviceInfo(audioout);

    inst->init(inst);
    inst->activate(inst, audioinfo.preferredSampleRate, 32, 4096);

    auto inst_param = (clap_plugin_params_t *)inst->get_extension(inst, CLAP_EXT_PARAMS);
    if (inst_param)
    {
        auto pc = inst_param->count(inst);
        std::cout << "Plugin has " << pc << " params " << std::endl;


        for (int i=0; i<pc; ++i)
        {
            clap_param_info_t inf;
            inst_param->get_info(inst, i, &inf);
            std::cout << i << " " << inf.module << " " << inf.name << " (id=0x" << std::hex << inf.id << std::dec << ")" << std::endl;
            aud.paramInfo[inf.id] = inf;
        }

    }
    else
    {
        std::cout << "No Parameters Available" << std::endl;
    }

    auto inst_ports = (clap_plugin_audio_ports_t *)inst->get_extension(inst, CLAP_EXT_AUDIO_PORTS);
    int inPorts{0}, outPorts{0};
    if (inst_ports)
    {
        inPorts = inst_ports->count(inst, true);
        outPorts = inst_ports->count(inst, false);

        // For now fail out if a port isn't stereo
        for (int i = 0; i < inPorts; ++i)
        {
            clap_audio_port_info_t inf;
            inst_ports->get(inst, i, true, &inf);
            if (inf.channel_count != 2)
                std::cout << "ERROR - need channel count 2" << std::endl;
        }
        for (int i = 0; i < outPorts; ++i)
        {
            clap_audio_port_info_t inf;
            inst_ports->get(inst, i, false, &inf);
            if (inf.channel_count != 2)
                std::cout << "ERROR - need channel count 2" << std::endl;
        }
    }
    else
    {
        std::cout << "No ports extension" << std::endl;
    }

    std::cout << "Starting audio stream on default output "
              << audioinfo.name << " at " << audioinfo.sampleRates[0] << std::endl;

    RtAudio::StreamParameters parameters;
    parameters.deviceId = audio.getDefaultOutputDevice();
    parameters.nChannels = 2;
    parameters.firstChannel = 0;
    unsigned int sampleRate = audioinfo.preferredSampleRate;
    unsigned int bufferFrames = 256; // 256 sample frames


    aud.inPorts = inPorts;
    aud.outPorts = outPorts;

    if (aud.inPorts)
    {
        aud.inBuffers = (clap_audio_buffer_t *)malloc(aud.inPorts * sizeof(clap_audio_buffer_t));
        for (int i=0; i<aud.inPorts; ++i)
        {
            aud.inBuffers[i].data32 = (float **)malloc(2 * sizeof(float *));
            aud.inBuffers[i].data32[0]  = (float *)malloc(bufferFrames * sizeof(float));
            aud.inBuffers[i].data32[1]  = (float *)malloc(bufferFrames * sizeof(float));
            aud.inBuffers[i].data64 = nullptr;
            aud.inBuffers[i].channel_count = 2;
            aud.inBuffers[i].latency = 0;
            aud.inBuffers[i].constant_mask = 0;
            memset(aud.inBuffers[i].data32[0], 0, bufferFrames * sizeof(float));
            memset(aud.inBuffers[i].data32[1], 0, bufferFrames * sizeof(float));
        }
    }
    else
    {
        aud.inBuffers = nullptr;
    }

    micro_clap_host::micro_input_events::setup(&(aud.inEvents));

    if (aud.outPorts)
    {
        aud.outBuffers = (clap_audio_buffer_t *)malloc(aud.outPorts * sizeof(clap_audio_buffer_t));
        for (int i=0; i<aud.outPorts; ++i)
        {
            aud.outBuffers[i].data32 = (float **)malloc(2 * sizeof(float *));
            aud.outBuffers[i].data32[0]  = (float *)malloc(bufferFrames * sizeof(float));
            aud.outBuffers[i].data32[1]  = (float *)malloc(bufferFrames * sizeof(float));

            aud.outBuffers[i].data64 = nullptr;
            aud.outBuffers[i].channel_count = 2;
            aud.outBuffers[i].latency = 0;
            aud.outBuffers[i].constant_mask = 0;
            memset(aud.outBuffers[i].data32[0], 0, bufferFrames * sizeof(float));
            memset(aud.outBuffers[i].data32[1], 0, bufferFrames * sizeof(float));
        }
    }
    else
    {
        aud.outBuffers = nullptr;
    }

    audio.openStream( &parameters, NULL, RTAUDIO_FLOAT32,
                      sampleRate, &bufferFrames, &processLoop, (void *)&aud );
    audio.startStream();

    char input;
    std::cout << "\nPlaying ... press <enter> a few times to quit.\n";
    std::cin.get( input );


    std::cout << "Cleaning Up" << std::endl;
    audio.stopStream();

    if ( audio.isStreamOpen() ) audio.closeStream();

    // cleanup that memory
    std::cout << "DEALLOC" << std::endl;

    micro_clap_host::micro_input_events::destroy(&(aud.inEvents));

    inst->deactivate(inst);
    inst->destroy(inst);

    entry->deinit();
}