cmake_minimum_required(VERSION 3.24)

enable_testing()

add_executable(generator 
                src/synchronous_generator.cpp 
                src/synchronous_mock_generator.cpp 
                src/interface_declaration_generator.cpp
                src/yas_generator.cpp 
                src/main.cpp 
                src/helpers.cpp
                src/sha3.c
                src/component_checksum.cpp
                src/fingerprint_generator.cpp)

target_compile_definitions(generator PRIVATE ${HOST_DEFINES})
target_compile_options(generator PRIVATE ${HOST_COMPILE_OPTIONS})
target_include_directories(generator PRIVATE ${HOST_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(
  generator
  PRIVATE ast_parser
          macro_parser
          utils
          args::args
          fmt::fmt-header-only)
target_link_options(generator PRIVATE ${HOST_LINK_EXE_OPTIONS})

set_property(TARGET generator PROPERTY COMPILE_PDB_NAME generator)
if(ENABLE_CLANG_TIDY)
  set_target_properties(generator PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()
