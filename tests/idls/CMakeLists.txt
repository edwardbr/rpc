cmake_minimum_required(VERSION 3.24)

RPCGenerate(
  example_shared
  example_shared/example_shared.idl
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/generated
  example_shared
  ""
  mock example_shared/example_shared_mock.h
  dependencies
  include_paths ${CMAKE_CURRENT_SOURCE_DIR}/.)

RPCGenerate(
  example_import
  example_import/example_import.idl
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/generated
  example_import
  ""
  mock example_import/example_import_mock.h
  dependencies example_shared_idl
  include_paths ${CMAKE_CURRENT_SOURCE_DIR}/.)

RPCGenerate(
  example
  example/example.idl
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/generated
  example
  ""
  mock example/example_mock.h
  dependencies example_import_idl example_shared_idl
  include_paths ${CMAKE_CURRENT_SOURCE_DIR}/.)
