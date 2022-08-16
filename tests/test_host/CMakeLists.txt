cmake_minimum_required(VERSION 3.0.0)


add_executable(marshal_test_host 
    main.cpp
    host_telemetry_service.cpp
    )

target_compile_definitions(marshal_test_host PRIVATE ${HOST_DEFINES})

target_include_directories(marshal_test_host 
  PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>"
  PRIVATE 
    ${HOST_INCLUDES})

target_link_libraries(marshal_test_host 
  PUBLIC 
    marshal_test_edl_host
    common_host
    example_interface
    
    rpc::marshaller_host 
    yas_common    
    fmt::fmt-header-only
    GTest::gtest_main
    GTest::gmock_main

    spdlog::spdlog_header_only


    ${HOST_LIBRARIES})

target_compile_options(marshal_test_host PRIVATE ${HOST_COMPILE_OPTIONS})
target_link_options(marshal_test_host PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(marshal_test_host PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET marshal_test_host PROPERTY COMPILE_PDB_NAME marshal_test_host) 



