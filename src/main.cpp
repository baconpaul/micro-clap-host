#include <iostream>
#include <filesystem>

#include "micro-clap-host.h"
#include "generators.h"

#include "RtAudio.h"

#include <clap/plugin-factory.h>
#include <clap/ext/params.h>
#include <clap/ext/audio-ports.h>

#include <unordered_map>

// in audio-thread.cpp at the bottom
extern int rtaudioToClap(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                         double streamTime, RtAudioStreamStatus status, void *userData);

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
    std::cout << "Clap Version        : " << version.major << "." << version.minor << "."
              << version.revision << std::endl;

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
    while (f[0])
    {
        std::cout << pre << f[0];
        pre = ", ";
        f++;
    }
    std::cout << std::endl;

    // Now lets make an instance
    auto host = micro_clap_host::createMicroHost();
    auto inst = fac->create_plugin(fac, host, desc->id);

    micro_clap_host::audiothread_userdata aud;
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

        for (int i = 0; i < pc; ++i)
        {
            clap_param_info_t inf;
            inst_param->get_info(inst, i, &inf);
            std::cout << i << " " << inf.module << " " << inf.name << " (id=0x" << std::hex
                      << inf.id << std::dec << ")" << std::endl;
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

    std::cout << "Starting audio stream on default output " << audioinfo.name << " at "
              << audioinfo.sampleRates[0] << std::endl;

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
        aud.inBuffers = new clap_audio_buffer_t[aud.inPorts];
        for (int i = 0; i < aud.inPorts; ++i)
        {
            aud.inBuffers[i].data32 = (float **)malloc(2 * sizeof(float *));
            aud.inBuffers[i].data32[0] = (float *)malloc(bufferFrames * sizeof(float));
            aud.inBuffers[i].data32[1] = (float *)malloc(bufferFrames * sizeof(float));
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
    micro_clap_host::micro_output_events::setup(&(aud.outEvents));

    if (aud.outPorts)
    {
        aud.outBuffers = new clap_audio_buffer_t[aud.outPorts];
        for (int i = 0; i < aud.outPorts; ++i)
        {
            aud.outBuffers[i].data32 = (float **)malloc(2 * sizeof(float *));
            aud.outBuffers[i].data32[0] = (float *)malloc(bufferFrames * sizeof(float));
            aud.outBuffers[i].data32[1] = (float *)malloc(bufferFrames * sizeof(float));

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

    // Finally set up our generators
    aud.generators.push_back(std::make_unique<micro_clap_host::generators::random_notes>());

    audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &rtaudioToClap,
                     (void *)&aud);
    audio.startStream();

    char input;
    std::cout << "\nPlaying ... press <enter> a few times to quit.\n";
    std::cin.get(input);

    std::cout << "Cleaning Up" << std::endl;
    audio.stopStream();

    if (audio.isStreamOpen())
        audio.closeStream();

    // cleanup that memory
    if (aud.inPorts)
    {
        for (int i=0; i<aud.inPorts; ++i)
        {
            free(aud.inBuffers[i].data32[0]);
            free(aud.inBuffers[i].data32[1]);
            free(aud.inBuffers[i].data32);
        }
        delete[] aud.inBuffers;
    }

    if (aud.outPorts)
    {
        for (int i=0; i<aud.outPorts; ++i)
        {
            free(aud.outBuffers[i].data32[0]);
            free(aud.outBuffers[i].data32[1]);
            free(aud.outBuffers[i].data32);
        }
        delete[] aud.outBuffers;
    }

    micro_clap_host::micro_input_events::destroy(&(aud.inEvents));
    micro_clap_host::micro_output_events::destroy(&(aud.outEvents));

    inst->deactivate(inst);
    inst->destroy(inst);

    entry->deinit();
}