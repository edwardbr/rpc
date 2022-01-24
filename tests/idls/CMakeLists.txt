# Copyright 2021 Secretarium Ltd <contact@secretarium.org>

cmake_minimum_required(VERSION 3.12)

EnclaveMarshaller(
    example_interface
    ${CMAKE_CURRENT_SOURCE_DIR}/example_interface.idl
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