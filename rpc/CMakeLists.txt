cmake_minimum_required(VERSION 3.0.0)

add_library(marshaller_enclave 
    include/rpc/marshaller.h
    include/rpc/basic_service_proxies.h
    include/rpc/marshaller.h
    include/rpc/proxy.h
    include/rpc/remote_pointer.h
    include/rpc/service.h
    include/rpc/stub.h
    src/proxy.cpp
    src/remote_pointer.cpp
    src/service.cpp
    src/stub.cpp
    src/error_codes.cpp
    src/version.cpp
    )

target_compile_definitions(marshaller_enclave 
    PRIVATE 
        ${ENCLAVE_DEFINES})
target_compile_options(marshaller_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
target_include_directories(marshaller_enclave 
    PUBLIC 
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include/rpc>"
    PRIVATE 
        ${ENCLAVE_LIBCXX_INCLUDES})
target_link_libraries(marshaller_enclave 
    PUBLIC 
    yas_common)
target_link_options(marshaller_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

set_property(TARGET marshaller_enclave PROPERTY COMPILE_PDB_NAME marshaller_enclave)

add_library(rpc::marshaller_enclave ALIAS marshaller_enclave)

################################################################################
add_library(marshaller_host
    include/rpc/marshaller.h
    include/rpc/basic_service_proxies.h
    include/rpc/marshaller.h
    include/rpc/proxy.h
    include/rpc/remote_pointer.h
    include/rpc/service.h
    include/rpc/stub.h
    src/proxy.cpp
    src/remote_pointer.cpp
    src/service.cpp
    src/stub.cpp
    src/error_codes.cpp
    src/version.cpp
    )


target_compile_definitions(marshaller_host PRIVATE ${HOST_DEFINES})

target_include_directories(marshaller_host 
    PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include/rpc>"
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


add_library(rpc::marshaller_host ALIAS marshaller_host)

install(DIRECTORY include/ DESTINATION include/rpc 
FILES_MATCHING 
  PATTERN "*.h*" 
  #PATTERN "private" EXCLUDE
)