cmake_minimum_required(VERSION 3.12)

EnclaveMarshaller(
    example_interface
    ${CMAKE_CURRENT_SOURCE_DIR}/example_interface.idl
    ${CMAKE_BINARY_DIR}/generated
    example/example.h
    example_proxy.cpp
    example_stub.cpp
    marshalled_tests
    include_paths
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
        .
    dependencies
        in_zone_synchronous_generator
    )