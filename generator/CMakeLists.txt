cmake_minimum_required(VERSION 3.0.0)
project(idlparser VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(in_zone_synchronous_generator 
in_zone_synchronous_generator.cpp
main.cpp)

target_link_libraries(in_zone_synchronous_generator 
PUBLIC 
ast_parser
macro_parser 
utils

clipp::clipp
fmt::fmt)
set_property(TARGET in_zone_synchronous_generator PROPERTY COMPILE_PDB_NAME in_zone_synchronous_generator)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
