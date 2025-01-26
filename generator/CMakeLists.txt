cmake_minimum_required(VERSION 3.24)

enable_testing()

add_executable(generator 
                synchronous_generator.cpp 
                synchronous_mock_generator.cpp 
                interface_declaration_generator.cpp
                yas_generator.cpp 
                main.cpp 
                helpers.cpp
                sha3.c
                component_checksum.cpp
                fingerprint_generator.cpp)

target_compile_definitions(generator PRIVATE ${HOST_DEFINES})
target_compile_options(generator PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_PEDANTIC})
target_include_directories(generator PRIVATE ${HOST_INCLUDES})
target_link_libraries(
  generator
  PRIVATE ast_parser
          macro_parser
          utils
          clipp::clipp
          fmt::fmt-header-only)
target_link_options(generator PRIVATE ${HOST_LINK_EXE_OPTIONS})

set_property(TARGET generator PROPERTY COMPILE_PDB_NAME generator)
if(ENABLE_CLANG_TIDY)
  set_target_properties(generator PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()
