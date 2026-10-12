#include "MacFullScreenDetector.h"

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <vector>

namespace dhikr {

std::unique_ptr<IFullScreenDetector> IFullScreenDetector::Create() {
    return std::make_unique<MacFullScreenDetector>();
}

FullScreenState MacFullScreenDetector::CheckFullScreen() {
    CFArrayRef windowList = CGWindowListCopyWindowInformation(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID
    );

    if (!windowList) {
        return FullScreenState::Unknown;
    }

    CFIndex count = CFArrayGetCount(windowList);
    CGRect mainDisplayBounds = CGDisplayBounds(CGMainDisplayID());
    FullScreenState state = FullScreenState::NotFullScreen;

    for (CFIndex i = 0; i < count; ++i) {
        CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex(windowList, i);
        if (!dict) continue;

        // Skip windows without normal layer
        CFNumberRef layerRef = (CFNumberRef)CFDictionaryGetValue(dict, kCGWindowLayer);
        int layer = 0;
        if (layerRef && CFNumberGetValue(layerRef, kCFNumberIntType, &layer)) {
            if (layer != 0) continue;
        }

        CFDictionaryRef boundsRef = (CFDictionaryRef)CFDictionaryGetValue(dict, kCGWindowBounds);
        if (boundsRef) {
            CGRect windowBounds;
            if (CGRectMakeWithDictionaryRepresentation(boundsRef, &windowBounds)) {
                if (CGRectEqualToRect(windowBounds, mainDisplayBounds)) {
                    state = FullScreenState::FullScreen;
                    break;
                }
            }
        }
    }

    CFRelease(windowList);
    return state;
}

} // namespace dhikr

#endif // __APPLE__
