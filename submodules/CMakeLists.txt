#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

if(RPC_STANDALONE)
  if(WIN32)
    if(${BUILD_TYPE} STREQUAL "release")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MD")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MDd")
    endif()
  endif()

  set(FMT_INSTALL ON)
  add_subdirectory(fmt)
  # fmt CMakeLists.txt defines a PUBLIC_HEADER property which triggers a warning when used as a subproject
  set_property(TARGET fmt PROPERTY PUBLIC_HEADER)

  add_subdirectory(args)
  add_library(args::args ALIAS args)
  add_subdirectory(googletest)

  add_library(yas_common INTERFACE)

  target_include_directories(yas_common INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/yas/include>")

  add_subdirectory(idlparser)

  if(BUILD_TEST)
    message("spdlog this is used in tests only and does not need to be installed for other users")
    set(SPDLOG_FMT_EXTERNAL
        ON
        CACHE BOOL "external fmt" FORCE)
    set(SPDLOG_NO_EXCEPTIONS
        ON
        CACHE BOOL "no exceptions" FORCE)
    set(SPDLOG_INSTALL OFF)
    add_subdirectory(spdlog)
  endif(BUILD_TEST)
  
  # if(BUILD_COROUTINE)
    set(CARES_STATIC    $<NOT:${LIBCORO_BUILD_SHARED_LIBS}> CACHE INTERNAL "")
    set(CARES_SHARED    ${LIBCORO_BUILD_SHARED_LIBS} CACHE INTERNAL "")
    set(CARES_INSTALL   OFF CACHE INTERNAL "")

    # set(c-ares_DIR "${CMAKE_SOURCE_DIR}/submodules/c-ares")
    #   list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/submodules/c-ares")
    add_subdirectory(c-ares)
    add_subdirectory(libcoro)
  # endif()
endif()
