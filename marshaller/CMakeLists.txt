cmake_minimum_required(VERSION 3.0.0)

add_library(marshaller 
    include/marshaller/marshaller.h
    src/marshaller.cpp)

target_include_directories(marshaller PUBLIC include/)
target_link_libraries(marshaller 
    PUBLIC 
    yas_common)
set_property(TARGET marshaller PROPERTY COMPILE_PDB_NAME marshaller)

