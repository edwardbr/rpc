cmake_minimum_required(VERSION 3.24)

if(RPC_STANDALONE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)


    project(enclave_marshaller VERSION 0.1.0)

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmakeconfigs")
    include(DependancyPrimer)

    add_subdirectory(submodules)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(FindRPCGenerator)

add_subdirectory(generator)
add_subdirectory(rpc)
if(${BUILD_TEST})
add_subdirectory(tests)
endif()


install(FILES "${CMAKE_CURRENT_LIST_DIR}/cmake/FindRPCGenerator.cmake"
        DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake/rpc )
