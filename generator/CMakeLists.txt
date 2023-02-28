cmake_minimum_required(VERSION 3.0.0)

include(CTest)
enable_testing()

add_executable(generator 
    synchronous_generator.cpp
    synchronous_mock_generator.cpp
    main.cpp)

target_compile_definitions(generator PRIVATE ${HOST_DEFINES})
target_compile_options(generator PRIVATE ${HOST_COMPILE_OPTIONS})
target_include_directories(generator 
    PRIVATE 
        ${HOST_INCLUDES})
target_link_libraries(generator 
    PRIVATE 
        ast_parser
        macro_parser 
        utils

        clipp::clipp
        fmt::fmt-header-only)
target_link_options(generator PRIVATE ${HOST_LINK_EXE_OPTIONS})

set_property(TARGET generator PROPERTY COMPILE_PDB_NAME generator)

