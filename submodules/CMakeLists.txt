cmake_minimum_required(VERSION 3.0.0)

if(RPC_STANDALONE)
  if(RPC_BUILD_SGX)
    if(WIN32)
      if(${BUILD_TYPE} STREQUAL "release")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MD")
      else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MDd")
      endif()
    endif()
  endif()

  message("fmt")
  set(FMT_INSTALL ON)
  add_subdirectory(fmt)
  add_subdirectory(clipp)

  if(RPC_BUILD_SGX)
    # this is needed as the linker symbols are different against the global settings
    add_subdirectory(googletest_for_enclaves)
  else()
    add_subdirectory(googletest)
  endif()

  add_library(yas_common INTERFACE)

  target_include_directories(yas_common
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/yas/include>"
  )

  add_subdirectory(idlparser)

  set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "external fmt" FORCE)
  set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "no exceptions" FORCE)
  set(SPDLOG_INSTALL ON)
  add_subdirectory(spdlog)
endif()