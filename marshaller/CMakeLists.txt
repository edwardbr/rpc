cmake_minimum_required(VERSION 3.0.0)

add_library(marshaller_enclave 
    include/marshaller/marshaller.h
    src/marshaller.cpp
    src/remote_pointer.cpp)

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
    src/marshaller.cpp
    src/remote_pointer.cpp
    )


target_compile_definitions(marshaller_host PRIVATE ${HOST_DEFINES})

target_include_directories(marshaller_host 
    PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>"
    PRIVATE 
    ${HOST_INCLUDES})

target_link_libraries(marshaller_host 
    PRIVATE
        yas_common
    PUBLIC 
        ${HOST_LIBRARIES})

target_compile_options(marshaller_host PRIVATE ${HOST_COMPILE_OPTIONS})
target_link_options(marshaller_host PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(marshaller_host PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET marshaller_host PROPERTY COMPILE_PDB_NAME marshaller_host)     

