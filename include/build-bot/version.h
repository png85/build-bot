// -*- C++ -*-
#ifndef BUILD_BOT_VERSION_H
#define BUILD_BOT_VERSION_H

namespace dsn {
namespace build_bot {
    namespace version {
        extern static const uint32_t MAJOR;
        extern static const uint32_t MINOR;
        extern static const uint32_t PATCH;

        extern static const char GIT_SHA1[];
        extern static const char GIT_REFSPEC[];
    }
}
}

#endif // BUILD_BOT_VERSION_H
