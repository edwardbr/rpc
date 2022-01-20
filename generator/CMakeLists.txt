cmake_minimum_required(VERSION 3.0.0)
project(idlparser VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(enclave_marshalling_generator 
enclave_marshalling_generator.cpp
main.cpp)

target_link_libraries(enclave_marshalling_generator 
PUBLIC 
ast_parser
macro_parser 
utils

clipp::clipp
fmt::fmt)
set_property(TARGET enclave_marshalling_generator PROPERTY COMPILE_PDB_NAME enclave_marshalling_generator)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
