#[[
  Copyright (c) 2025 Edward Boggis-Rolfe
  All rights reserved.
]]
cmake_minimum_required(VERSION 3.24)

# Build individual shared_ptr/weak_ptr tests with resolved dependencies
# These tests now have access to test infrastructure headers

# Tests with resolved dependencies that should now build
set(BUILDABLE_TESTS
    # P0718R2_atomic_smart_ptrs                     # C++20 only - atomic<shared_ptr> specializations
    # P1020R1_smart_pointer_for_overwrite           # C++20 only - make_shared_for_overwrite not in C++17
    # P0674R1_make_shared_for_arrays                # C++20 only - make_shared<T[]> not in C++17
    # P1135R6_atomic_wait                           # C++20 only - atomic<shared_ptr> specializations
    # LWG3018_shared_ptr_function                   # shared_ptr function tests disable as we are not going to implement that
    # Dev10_498944_enable_shared_from_this_auto_ptr # not interested in auto_ptr

    Dev09_196243_tr1_enable_shared_from_this_ops  # enable_shared_from_this tests
    Dev10_445289_make_shared                      # make_shared tests
    Dev10_654977_655012_shared_ptr_move          # shared_ptr move semantics
    Dev11_1066589_shared_ptr_atomic_deadlock     # shared_ptr atomic operations
    Dev10_851347_weak_ptr_virtual_inheritance     # uses Microsoft private members
    
    
    Dev09_153419_tr1_allocators
    Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer
    Dev11_0376122_grand_theft_bind
    Dev11_0920385_list_sort_allocator
    P2162R2_std_visit_for_derived_classes_from_variant
    # VSO_0000000_has_static_rtti
    VSO_0000000_initialize_everything
    VSO_0105317_expression_sfinae
    
    
    Dev10_847656_shared_ptr_is_convertible
    Dev10_500860_overloaded_address_of
    Dev10_635436_shared_ptr_reset
    Dev10_722102_shared_ptr_nullptr
    Dev11_0000000_tuple_cat
    Dev11_0000000_user_defined_literals
    Dev11_0535636_functional_overhaul
    GH_002299_implicit_sfinae_constraints
    GH_005315_destructor_tombstones
    # P0414R2_shared_ptr_for_arrays
    P0433R2_deduction_guides
    P0513R0_poisoning_the_hash
    # P1132R7_out_ptr
    # P2286R8_text_formatting_formattable
    # P2404R3_move_only_types_for_comparison_concepts
    VSO_0000000_instantiate_iterators_misc
)

foreach(test_name ${BUILDABLE_TESTS})
    set(test_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}")
    if(EXISTS "${test_dir}")
        # Find source files
        file(GLOB test_sources "${test_dir}/*.cpp" "${test_dir}/*.c")

        if(test_sources)
            add_executable(${test_name} ${test_sources})

            target_include_directories(${test_name}
                PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/include  # STL test infrastructure
            )

            target_compile_definitions(${test_name} PRIVATE
                _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS  # Suppress MS-specific warnings
            )

            # Force include compatibility headers for Microsoft-specific functions
            if(NOT MSVC)
                target_compile_options(${test_name} PRIVATE
                    "-include${CMAKE_CURRENT_SOURCE_DIR}/include/msvc_compat.hpp"
                )
            endif()

            # No target_link_libraries needed - these are isolated STL tests

            target_compile_features(${test_name} PRIVATE cxx_std_17)

            # Force C++17 standard (override any C++20 settings)
            set_target_properties(${test_name} PROPERTIES CXX_STANDARD 17)

            add_test(NAME ${test_name} COMMAND ${test_name})
            set_tests_properties(${test_name} PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
endforeach()
