// Cross-platform compatibility for Microsoft-specific functionality
// Copyright (c) 2025 Edward Boggis-Rolfe
// All rights reserved.

#pragma once

#include <cstdlib>
#include <memory>

// Platform detection
#ifdef _MSC_VER
    #define MSVC_COMPAT_WINDOWS 1
    #include <crtdbg.h>
    #include <malloc.h>
#else
    #define MSVC_COMPAT_WINDOWS 0
    #include <cstdlib>
    #ifdef __has_include
        #if __has_include(<malloc.h>)
            #include <malloc.h>
        #endif
    #endif
#endif

// Microsoft-specific function compatibility
#if !MSVC_COMPAT_WINDOWS

// Provide _aligned_malloc/_aligned_free for non-Windows platforms
inline void* _aligned_malloc(size_t size, size_t alignment) {
    #ifdef _ISOC11_SOURCE
        return aligned_alloc(alignment, size);
    #elif defined(__GNUC__) || defined(__clang__)
        void* ptr = nullptr;
        if (posix_memalign(&ptr, alignment, size) == 0) {
            return ptr;
        }
        return nullptr;
    #else
        // Fallback implementation
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            return nullptr; // Invalid alignment
        }

        const size_t offset = alignment - 1 + sizeof(void*);
        void* original = malloc(size + offset);
        if (!original) return nullptr;

        void* aligned = reinterpret_cast<void*>(
            (reinterpret_cast<uintptr_t>(original) + offset) & ~(alignment - 1)
        );

        // Store original pointer for _aligned_free
        reinterpret_cast<void**>(aligned)[-1] = original;
        return aligned;
    #endif
}

inline void _aligned_free(void* ptr) {
    if (!ptr) return;

    #ifdef _ISOC11_SOURCE
        free(ptr);
    #elif defined(__GNUC__) || defined(__clang__)
        free(ptr);
    #else
        // For fallback implementation, retrieve original pointer
        void* original = reinterpret_cast<void**>(ptr)[-1];
        free(original);
    #endif
}

// CRT debugging stubs for non-Windows
#define _CRT_WARN 0
#define _CRT_ERROR 1
#define _CRT_ASSERT 2

#define _CRTDBG_MODE_FILE 1
#define _CRTDBG_MODE_DEBUG 2
#define _CRTDBG_MODE_WNDW 4

#define _CRTDBG_FILE_STDERR ((void*)-1)
#define _CRTDBG_FILE_STDOUT ((void*)-2)

inline int _CrtSetReportMode(int reportType, int reportMode) {
    // Stub implementation - return previous mode (assume file mode)
    (void)reportType; (void)reportMode;
    return _CRTDBG_MODE_FILE;
}

inline void* _CrtSetReportFile(int reportType, void* reportFile) {
    // Stub implementation - return previous file
    (void)reportType; (void)reportFile;
    return _CRTDBG_FILE_STDERR;
}

#define _WRITE_ABORT_MSG 1
#define _CALL_REPORTFAULT 2

inline unsigned int _set_abort_behavior(unsigned int flags, unsigned int mask) {
    // Stub implementation - return previous behavior
    (void)flags; (void)mask;
    return 0;
}

inline int _CrtDumpMemoryLeaks() {
    // Stub implementation - no memory leaks detected
    return 0;
}

#endif // !MSVC_COMPAT_WINDOWS

// Microsoft-specific macros compatibility
#ifndef _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    #define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
#endif

// C++17 inline variable compatibility
#if __cplusplus >= 201703L && !defined(_INLINE_VAR)
    #define _INLINE_VAR inline constexpr
#elif !defined(_INLINE_VAR)
    #define _INLINE_VAR constexpr
#endif

// MSVC warning pragma compatibility
#ifndef _MSC_VER
    #define __pragma(x)
    #define _Pragma(x)
    #ifndef __analysis_assume
        #define __analysis_assume(x)
    #endif
#endif