// -*- C++ -*-
#ifndef BUILD_BOT_VERSION_H
#define BUILD_BOT_VERSION_H

#include <cstdint>

namespace dsn {
namespace build_bot {
    namespace version {
        extern const uint32_t MAJOR;
        extern const uint32_t MINOR;
        extern const uint32_t PATCH;

        extern const char GIT_SHA1[];
        extern const char GIT_REFSPEC[];
    }
}
}

#endif // BUILD_BOT_VERSION_H
