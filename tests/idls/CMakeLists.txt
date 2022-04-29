cmake_minimum_required(VERSION 3.12)


EnclaveMarshaller(
    example_shared
    example_shared.idl
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    example/example_shared.h
    example_shared_proxy.cpp
    example_shared_stub.cpp
    marshalled_tests
    dependencies
    include_paths
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
    )    

EnclaveMarshaller(
    example_import
    example_import.idl
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    example/example_import.h
    example_import_proxy.cpp
    example_import_stub.cpp
    marshalled_tests
    dependencies
        example_shared
    include_paths
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
    )

EnclaveMarshaller(
    example_interface
    example_interface.idl
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    example/example.h
    example_proxy.cpp
    example_stub.cpp
    marshalled_tests
    dependencies
        example_import
        example_shared
    include_paths
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_SOURCE_DIR}/submodules/idlparser/defines
    )