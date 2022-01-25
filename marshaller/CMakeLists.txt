cmake_minimum_required(VERSION 3.0.0)

add_library(marshaller_enclave 
    include/marshaller/marshaller.h
    src/marshaller.cpp)

target_compile_definitions(marshaller_enclave 
    PRIVATE 
        ${ENCLAVE_DEFINES})
target_compile_options(marshaller_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
target_include_directories(marshaller_enclave 
    PUBLIC 
        include/
    PRIVATE 
        ${ENCLAVE_LIBCXX_INCLUDES})
target_link_libraries(marshaller_enclave 
    PUBLIC 
    yas_common)
target_link_options(marshaller_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

set_property(TARGET marshaller_enclave PROPERTY COMPILE_PDB_NAME marshaller_enclave)


################################################################################
add_library(marshaller_host
    include/marshaller/marshaller.h
    src/marshaller.cpp)

target_include_directories(marshaller_host PUBLIC include/)
target_link_libraries(marshaller_host 
    PUBLIC 
    yas_common)
set_property(TARGET marshaller_host PROPERTY COMPILE_PDB_NAME marshaller_host)

