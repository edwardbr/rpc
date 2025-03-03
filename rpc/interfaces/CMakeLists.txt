#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

RPCGenerate(
  rpc_types
  rpc/rpc_types.idl
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/generated
  rpc
  ""
  include_paths ${CMAKE_CURRENT_SOURCE_DIR}/.
  generated_install_dir ${GENERATED_INSTALL_DIR}
  no_include_rpc_headers ON )
