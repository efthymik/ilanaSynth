// Which build this is: the git hash and the date CMake configured it
// (cmake/BuildInfo.cmake), shown in the logo hover and Settings, and saved
// in the state ("build"). A build without CMake's header (or without git)
// says "unknown".
#pragma once

#include <juce_core/juce_core.h>

#if __has_include("BuildInfoGenerated.h")
 #include "BuildInfoGenerated.h"
#endif

#ifndef ILANA_BUILD_HASH
 #define ILANA_BUILD_HASH "unknown"
#endif
#ifndef ILANA_BUILD_DATE
 #define ILANA_BUILD_DATE "unknown"
#endif

namespace ilana
{
    inline juce::String buildHash() { return ILANA_BUILD_HASH; }
    inline juce::String buildDate() { return ILANA_BUILD_DATE; }
    // "a1b2c3d · 2026-11-08"
    inline juce::String buildLabel() { return buildHash() + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + buildDate(); }
}
