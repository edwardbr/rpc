cmake_minimum_required(VERSION 3.12)

if(RPC_STANDALONE)
  set(FMT_INSTALL ON)
  add_subdirectory(fmt)
  add_subdirectory(clipp)
  add_subdirectory(googletest)

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