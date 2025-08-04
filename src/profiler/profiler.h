#ifndef ARISE_PROFILER_H
#define ARISE_PROFILER_H

#include "profiler/cpu.h"
#include "profiler/gpu.h"

#if defined(ARISE_USE_PROFILING) && defined(ARISE_USE_TRACY)

#include <tracy/Tracy.hpp>

#define PROFILE_FRAME()                                         FrameMark

// memory tracking
#define PROFILE_ALLOC(ptr, size)                                TracyAlloc(ptr, size)
#define PROFILE_FREE(ptr)                                       TracyFree(ptr)

#define PROFILE_PLOT(name, value)                               TracyPlot(name, value)

#define PROFILE_MESSAGE(text, size)                             TracyMessage(text, size)
#define PROFILE_MESSAGE_L(text)                                 TracyMessageL(text)
#define PROFILE_VALUE(text, value)                              TracyMessageL(text ": " + std::to_string(value))

// text annotations
#define PROFILE_TEXT(text)                                      ZoneText(text, strlen(text))
#define PROFILE_NAME(name)                                      ZoneName(name, strlen(name))

#define PROFILE_APP_INFO(name, size)                            TracyAppInfo(name, size)

#define PROFILE_FRAME_IMAGE(image, width, height, offset, flip) FrameImage(image, width, height, offset, flip)

#else
#define PROFILE_FRAME()
#define PROFILE_ALLOC(ptr, size)
#define PROFILE_FREE(ptr)
#define PROFILE_PLOT(name, value)
#define PROFILE_MESSAGE(text, size)
#define PROFILE_MESSAGE_L(text)
#define PROFILE_TEXT(text)
#define PROFILE_NAME(name)
#define PROFILE_APP_INFO(name, size)

#endif

#endif  // ARISE_PROFILER_H
