#include "LinuxFullScreenDetector.h"
#include "../../core/Logger.h"
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#ifndef __APPLE__
#include <dlfcn.h>

namespace dhikr {

std::unique_ptr<IFullScreenDetector> IFullScreenDetector::Create() {
    return std::make_unique<LinuxFullScreenDetector>();
}

LinuxFullScreenDetector::LinuxFullScreenDetector() {
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    const char* session = std::getenv("XDG_SESSION_TYPE");
    if ((wayland && *wayland) || (session && strcmp(session, "wayland") == 0)) {
        m_isWayland = true;
        LOG_INFO("Linux window session: Wayland detected (fullscreen external query restricted)");
    } else {
        m_isWayland = false;
        LOG_INFO("Linux window session: X11 detected");
    }
}

LinuxFullScreenDetector::~LinuxFullScreenDetector() = default;

FullScreenState LinuxFullScreenDetector::CheckFullScreen() {
    if (m_isWayland) {
        // Wayland security model intentionally prohibits querying foreign window geometries
        return FullScreenState::Unsupported;
    }

    // Dynamic X11 inspection
    void* x11 = dlopen("libX11.so.6", RTLD_LAZY);
    if (!x11) {
        return FullScreenState::Unknown;
    }

    typedef void* (*XOpenDisplayFn)(const char*);
    typedef int   (*XCloseDisplayFn)(void*);
    typedef unsigned long (*XDefaultRootWindowFn)(void*);
    typedef unsigned long (*XInternAtomFn)(void*, const char*, int);
    typedef int   (*XGetWindowPropertyFn)(void*, unsigned long, unsigned long, long, long, int, unsigned long,
                                          unsigned long*, int*, unsigned long*, unsigned long*, unsigned char**);
    typedef int   (*XFreeFn)(void*);

    auto pXOpenDisplay        = (XOpenDisplayFn)dlsym(x11, "XOpenDisplay");
    auto pXCloseDisplay       = (XCloseDisplayFn)dlsym(x11, "XCloseDisplay");
    auto pXDefaultRootWindow  = (XDefaultRootWindowFn)dlsym(x11, "XDefaultRootWindow");
    auto pXInternAtom         = (XInternAtomFn)dlsym(x11, "XInternAtom");
    auto pXGetWindowProperty  = (XGetWindowPropertyFn)dlsym(x11, "XGetWindowProperty");
    auto pXFree               = (XFreeFn)dlsym(x11, "XFree");

    if (!pXOpenDisplay || !pXCloseDisplay || !pXDefaultRootWindow ||
        !pXInternAtom || !pXGetWindowProperty || !pXFree) {
        dlclose(x11);
        return FullScreenState::Unknown;
    }

    void* display = pXOpenDisplay(nullptr);
    if (!display) {
        dlclose(x11);
        return FullScreenState::Unknown;
    }

    unsigned long root = pXDefaultRootWindow(display);
    unsigned long netActive = pXInternAtom(display, "_NET_ACTIVE_WINDOW", 0);
    unsigned long netWmState = pXInternAtom(display, "_NET_WM_STATE", 0);
    unsigned long netWmStateFs = pXInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);

    FullScreenState result = FullScreenState::NotFullScreen;

    unsigned long actualType = 0;
    int actualFormat = 0;
    unsigned long nItems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* prop = nullptr;

    if (pXGetWindowProperty(display, root, netActive, 0, 1, 0, (unsigned long)33 /* XA_WINDOW */,
                            &actualType, &actualFormat, &nItems, &bytesAfter, &prop) == 0 && prop) {
        unsigned long activeWindow = *(unsigned long*)prop;
        pXFree(prop);

        if (activeWindow != 0) {
            unsigned char* stateProp = nullptr;
            if (pXGetWindowProperty(display, activeWindow, netWmState, 0, 1024, 0, (unsigned long)4 /* XA_ATOM */,
                                    &actualType, &actualFormat, &nItems, &bytesAfter, &stateProp) == 0 && stateProp) {
                unsigned long* atoms = (unsigned long*)stateProp;
                for (unsigned long i = 0; i < nItems; ++i) {
                    if (atoms[i] == netWmStateFs) {
                        result = FullScreenState::FullScreen;
                        break;
                    }
                }
                pXFree(stateProp);
            }
        }
    }

    pXCloseDisplay(display);
    dlclose(x11);
    return result;
}

} // namespace dhikr

#endif // !__APPLE__
#endif // !_WIN32
