//
// Created by Paul Walker on 5/13/22.
//

#include "micro-clap-host.h"
#include <iostream>

#if MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace micro_clap_host
{
#if MAC
    clap_plugin_entry_t *entryFromClapPath(const std::filesystem::path &p)
    {
        auto ps = p.u8string();
        auto cs = CFStringCreateWithBytes(kCFAllocatorDefault, (uint8_t *)ps.c_str(), ps.size(), kCFStringEncodingUTF8, false);
        auto bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                       cs,
                                                       kCFURLPOSIXPathStyle,
                                                       true);

        auto bundle = CFBundleCreate(kCFAllocatorDefault, bundleURL);


        auto db = CFBundleGetDataPointerForName(bundle, CFSTR("clap_entry"));

        CFRelease(bundle);
        CFRelease(bundleURL);
        CFRelease(cs);

        return (clap_plugin_entry_t *)db;
    }
#endif
}