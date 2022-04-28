cmake_minimum_required(VERSION 3.0.0)

include(CTest)
enable_testing()

add_executable(in_zone_synchronous_generator 
    in_zone_synchronous_generator.cpp
    main.cpp)

target_compile_definitions(in_zone_synchronous_generator PRIVATE ${HOST_DEFINES})
target_compile_options(in_zone_synchronous_generator PRIVATE ${HOST_COMPILE_OPTIONS})
target_include_directories(in_zone_synchronous_generator 
    PRIVATE 
        ${HOST_INCLUDES})
target_link_libraries(in_zone_synchronous_generator 
    PUBLIC 
        ast_parser
        macro_parser 
        utils

        clipp::clipp
        fmt::fmt-header-only
    PRIVATE
        ${HOST_LIBRARIES})
target_link_options(in_zone_synchronous_generator PRIVATE ${HOST_LINK_EXE_OPTIONS})

set_property(TARGET in_zone_synchronous_generator PROPERTY COMPILE_PDB_NAME in_zone_synchronous_generator)

