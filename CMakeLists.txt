cmake_minimum_required(VERSION 3.0.0)

project(rpc VERSION 1.0.0)

option(RPC_STANDALONE "build the rpc on its own and not part of another repo" ON)

if(RPC_STANDALONE)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/.git")
    # Update submodules as needed
        option(GIT_SUBMODULE "Check submodules during build" ON)
        if(GIT_SUBMODULE)
            message(STATUS "Submodule init")
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule init
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            RESULT_VARIABLE GIT_SUBMOD_INIT_RESULT)
            if(NOT GIT_SUBMOD_INIT_RESULT EQUAL "0")
                message(FATAL_ERROR "submodule init")
            endif()
            message(STATUS "Submodule update")
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            RESULT_VARIABLE GIT_SUBMOD_RESULT)
            if(NOT GIT_SUBMOD_RESULT EQUAL "0")
                message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
            endif()
        endif()
    endif()

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