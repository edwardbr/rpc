#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

host_edl_library(
  marshal_test_edl_host
  enclave_marshal_test.edl
  ""
  OFF
  ""
  "")
set_property(TARGET marshal_test_edl_host PROPERTY COMPILE_PDB_NAME marshal_test_edl_host)

enclave_edl_library(
  marshal_test_edl_enclave
  enclave_marshal_test.edl
  ""
  OFF
  ""
  "")
set_property(TARGET marshal_test_edl_enclave PROPERTY COMPILE_PDB_NAME marshal_test_edl_enclave)
