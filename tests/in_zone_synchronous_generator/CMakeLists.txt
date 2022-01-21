cmake_minimum_required(VERSION 3.0.0)
project(idlparser VERSION 0.1.0)

include(CTest)
enable_testing()

EnclaveMarshaller(
    example_interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../idls/example_interface.idl
    ${CMAKE_BINARY_DIR}/generated
    example/example.h
    example_proxy.cpp
    example_stub.cpp
    secretarium::marshalled_foo
    include_paths
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
        .
    dependencies
        in_zone_synchronous_generator
    )

add_executable(test_in_zone_synchronous_generator 
    main.cpp
    )

target_include_directories(test_in_zone_synchronous_generator PRIVATE ${CMAKE_BINARY_DIR}/generated/include/ ${CMAKE_BINARY_DIR}/generated/src)
target_link_libraries(test_in_zone_synchronous_generator 
    PUBLIC 
    marshaller

    yas_common)
set_property(TARGET test_in_zone_synchronous_generator PROPERTY COMPILE_PDB_NAME test_in_zone_synchronous_generator)

add_dependencies(test_in_zone_synchronous_generator example_interface)

