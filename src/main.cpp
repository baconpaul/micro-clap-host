#include <iostream>
#include <filesystem>

#include "micro-clap-host.h"

#include <clap/plugin-factory.h>

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

    std::cout << "Loading clap : " << clap << std::endl;

    auto entry = micro_clap_host::entryFromClapPath(clapPath);

    if (!entry)
    {
        std::cout << "Got a null entru " << std::endl;
        exit(3);
    }

    std::cout << "Factory is " << entry << std::endl;

    auto version = entry->clap_version;
    std::cout << "Clap Version is " << version.major << "." << version.minor << "." << version.revision << std::endl;

    entry->init(clap.c_str());

    auto fac = (clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    std::cout << "FAC is " << fac << std::endl;
    auto plugin_count = fac->get_plugin_count(fac);
    if (plugin_count <= 0)
    {
        std::cout << "Plugin factory has no plugins" << std::endl;
        exit(4);
    }

    // FIXME - what about multiplugin? for now just grab 0
    auto desc = fac->get_plugin_descriptor(fac, 0);

    std::cout << "Plugin is " << desc->name << std::endl;


    entry->deinit();
}