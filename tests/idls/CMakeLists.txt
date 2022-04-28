cmake_minimum_required(VERSION 3.12)


EnclaveMarshaller(
    example_shared
    ${CMAKE_CURRENT_SOURCE_DIR}/example_shared.idl
    ${CMAKE_BINARY_DIR}/generated
    example/example_shared.h
    example_shared_proxy.cpp
    example_shared_stub.cpp
    marshalled_tests
    include_paths
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
    dependencies
        in_zone_synchronous_generator
    )    

EnclaveMarshaller(
    example_import
    ${CMAKE_CURRENT_SOURCE_DIR}/example_import.idl
    ${CMAKE_BINARY_DIR}/generated
    example/example_import.h
    example_import_proxy.cpp
    example_import_stub.cpp
    marshalled_tests
    include_paths
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
    dependencies
        in_zone_synchronous_generator
        example_shared
    )
    
EnclaveMarshaller(
    example_interface
    ${CMAKE_CURRENT_SOURCE_DIR}/example_interface.idl
    ${CMAKE_BINARY_DIR}/generated
    example/example.h
    example_proxy.cpp
    example_stub.cpp
    marshalled_tests
    include_paths
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
    dependencies
        in_zone_synchronous_generator
        example_import
        example_shared
    )