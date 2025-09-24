// Cross-platform compatibility for crtdbg.h
// Copyright (c) 2025 Edward Boggis-Rolfe
// All rights reserved.

#pragma once

#ifdef _MSC_VER
    // On Windows, use the real crtdbg.h
    #include_next <crtdbg.h>
#else
    // On non-Windows platforms, provide compatibility definitions
    #include "msvc_compat.hpp"
#endif