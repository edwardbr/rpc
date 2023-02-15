cmake_minimum_required(VERSION 3.0.0)

option(RPC_STANDALONE "build the rpc on its own and not part of another repo" ON)

project(rpc VERSION 1.0.0)

if(RPC_STANDALONE)

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmakeconfigs")
    include(DependancyPrimer)

    add_subdirectory(submodules)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(FindEnclaveMarshaller)

add_subdirectory(generator)
add_subdirectory(rpc)
if(${RPC_BUILD_TEST})
add_subdirectory(tests)
endif()


INSTALL(FILES "${CMAKE_CURRENT_LIST_DIR}/cmake/FindEnclaveMarshaller.cmake"
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/rpc )