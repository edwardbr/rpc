cmake_minimum_required(VERSION 3.0.0)

add_library(rpc_enclave 
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

target_compile_definitions(rpc_enclave 
    PRIVATE 
        ${ENCLAVE_DEFINES})
target_compile_options(rpc_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
target_include_directories(rpc_enclave 
    PUBLIC 
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include/rpc>"
    PRIVATE 
        ${ENCLAVE_LIBCXX_INCLUDES})
target_link_libraries(rpc_enclave 
    PRIVATE 
    yas_common)
target_link_options(rpc_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

set_property(TARGET rpc_enclave PROPERTY COMPILE_PDB_NAME rpc_enclave)

add_library(rpc::rpc_enclave ALIAS rpc_enclave)

################################################################################
add_library(rpc_host
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


target_compile_definitions(rpc_host PRIVATE ${HOST_DEFINES})

target_include_directories(rpc_host 
    PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include/rpc>"
    PRIVATE 
    ${HOST_INCLUDES})

target_link_libraries(rpc_host 
    PRIVATE        
        yas_common
        ${HOST_LIBRARIES})

target_compile_options(rpc_host PRIVATE ${HOST_COMPILE_OPTIONS})
target_link_options(rpc_host PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(rpc_host PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET rpc_host PROPERTY COMPILE_PDB_NAME rpc_host)     


add_library(rpc::rpc_host ALIAS rpc_host)

install(DIRECTORY include/ DESTINATION include/rpc 
FILES_MATCHING 
  PATTERN "*.h*" 
  #PATTERN "private" EXCLUDE
)