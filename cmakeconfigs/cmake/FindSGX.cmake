# Copyright 2021 Secretarium Ltd <contact@secretarium.org>

# FindPackage cmake file for Intel SGX SDK

cmake_minimum_required(VERSION 2.8.8)
include(CMakeParseArguments)

set(SGX_FOUND "NO")

if(EXISTS SGX_DIR)
  set(SGX_PATH ${SGX_DIR})
elseif(EXISTS SGX_ROOT)
  set(SGX_PATH ${SGX_ROOT})
elseif(EXISTS $ENV{SGX_SDK})
  set(SGX_PATH $ENV{SGX_SDK})
elseif(EXISTS $ENV{SGX_DIR})
  set(SGX_PATH $ENV{SGX_DIR})
elseif(EXISTS $ENV{SGX_ROOT})
  set(SGX_PATH $ENV{SGX_ROOT})
else()
	if(NOT WIN32)
		message("linux")
		set(SGX_PATH "/opt/intel/sgxsdk")
	else()
		message("windows")
		set(SGX_PATH "C:/PROGRA~2/Intel/INTELS~1")
	endif()
endif()

if(NOT WIN32)
  set(SGX_COMMON_CFLAGS -m64)
  set(SGX_LIBRARY_PATH ${SGX_PATH}/lib64)
  set(SGX_ENCLAVE_SIGNER ${SGX_PATH}/bin/x64/sgx_sign)
  set(SGX_EDGER8R ${SGX_PATH}/bin/x64/sgx_edger8r)
else()
  set(SGX_BINARIES ${SGX_PATH}/bin/win32/Release)
  #set(SGX_COMMON_CFLAGS -m32)
  if((${SGX_MODE} STREQUAL "prerelease") OR (${SGX_MODE} STREQUAL "release")) #not simulation nor debug nor "optimized debug" nor "optimized simulation"
    set(SGX_LIBRARY_PATH ${SGX_PATH}/bin/x64/Release)
  else()
    set(SGX_LIBRARY_PATH ${SGX_PATH}/bin/x64/Debug)
  endif()
  set(SGX_ENCLAVE_SIGNER ${SGX_BINARIES}/sgx_sign.exe)
  set(SGX_EDGER8R ${SGX_BINARIES}/sgx_edger8r.exe)
endif()

if(NOT WIN32)
	find_path(SGX_LIBRARY_DIR libsgx_urts.so "${SGX_LIBRARY_PATH}" NO_DEFAULT_PATH)
else()
	find_path(SGX_LIBRARY_DIR_TMP sgx_urts.lib "${SGX_LIBRARY_PATH}" NO_DEFAULT_PATH)
  file(TO_NATIVE_PATH ${SGX_LIBRARY_DIR_TMP} SGX_LIBRARY_DIR)
endif()
find_path(SGX_INCLUDE_DIR sgx.h "${SGX_PATH}/include" NO_DEFAULT_PATH)

if(SGX_INCLUDE_DIR AND SGX_LIBRARY_DIR)
  set(SGX_FOUND "YES")
  if(NOT WIN32)
    set(SGX_INCLUDE_DIR "${SGX_PATH}/include" CACHE PATH "Intel SGX include directory" FORCE)
    set(SGX_TLIBC_INCLUDE_DIR "${SGX_INCLUDE_DIR}/tlibc" CACHE PATH "Intel SGX tlibc include directory" FORCE)
    set(SGX_LIBCXX_INCLUDE_DIR "${SGX_INCLUDE_DIR}/libcxx" CACHE PATH "Intel SGX libcxx include directory" FORCE)
  else()
    set(SGX_INCLUDE_DIR "${SGX_PATH}/include" CACHE PATH "Intel SGX include directory" FORCE)
    set(SGX_TLIBC_INCLUDE_DIR "${SGX_INCLUDE_DIR}/tlibc" CACHE PATH "Intel SGX tlibc include directory" FORCE)
    set(SGX_LIBCXX_INCLUDE_DIR "${SGX_INCLUDE_DIR}/libc++" CACHE PATH "Intel SGX tlibc include directory" FORCE)
    set(SGX_LIBSTDCXX_INCLUDE_DIR "${SGX_INCLUDE_DIR}/stdc++" CACHE PATH "Intel SGX libcxx include directory" FORCE)
  endif()
  set(ENCLAVE_SGX_INCLUDE_DIRS ${SGX_INCLUDE_DIR} ${SGX_TLIBC_INCLUDE_DIR} ${SGX_LIBCXX_INCLUDE_DIR} ${SGX_LIBSTDCXX_INCLUDE_DIR})
  set(HOST_SGX_INCLUDE_DIRS ${SGX_INCLUDE_DIR} )
  mark_as_advanced(SGX_INCLUDE_DIR SGX_LIBRARY_DIR SGX_TLIBC_INCLUDE_DIR SGX_LIBC_INCLUDE_DIR SGX_LIBCXX_INCLUDE_DIR SGX_LIBSTDCXX_INCLUDE_DIR)
  message(STATUS "Found Intel SGX SDK.")
endif()

if(SGX_FOUND)
  set(SGX_TCXX_LIB sgx_tcxx)
  set(SGX_TSTDC_LIB sgx_tstdc)
  set(SGX_TCRYPTO_LIB sgx_tcrypto)
  if(SGX_HW)
    set(SGX_URTS_LIB sgx_urts)
    set(SGX_USVC_LIB sgx_uae_service)
    set(SGX_TRTS_LIB sgx_trts)
    set(SGX_TSVC_LIB sgx_tservice)
  else()
    set(SGX_URTS_LIB sgx_urts_sim)
    set(SGX_USVC_LIB sgx_uae_service_sim)
    set(SGX_TRTS_LIB sgx_trts_sim)
    set(SGX_TSVC_LIB sgx_tservice_sim)
  endif()

  set(SGX_ENCLAVE_LIBS 
    ${SGX_TCXX_LIB}.lib
    ${SGX_TRTS_LIB}.lib
    ${SGX_TSTDC_LIB}.lib
    ${SGX_TSVC_LIB}.lib
    ${SGX_TCRYPTO_LIB}.lib
    ${SGX_HEADER_ONLY_LIBS})

  set(ENCLAVE_INC_FLAGS -I${SGX_INCLUDE_DIR} -I${SGX_TLIBC_INCLUDE_DIR} -I${SGX_LIBCXX_INCLUDE_DIR})
  set(ENCLAVE_C_FLAGS "${SGX_COMMON_CFLAGS} ${ENCLAVE_INC_FLAGS}")
  set(ENCLAVE_CXX_FLAGS ${ENCLAVE_C_FLAGS})
  set(APP_INC_FLAGS "-I${SGX_INCLUDE_DIR}")
  set(APP_C_FLAGS "${SGX_COMMON_CFLAGS} ${APP_INC_FLAGS}")
  set(APP_CXX_FLAGS "${APP_C_FLAGS}")  

  function(_build_trusted_edl_header PARENT_TARGET edl SEARCH_PATHS use_prefix)

    get_filename_component(EDL_NAME ${edl} NAME_WE)
    get_filename_component(EDL_ABSPATH ${edl} ABSOLUTE)
    set(EDL_T_C "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_t.h")
    set(EDL_T_H "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_t.h")
    set(EDL_TT_H "${CMAKE_CURRENT_BINARY_DIR}/trusted/${EDL_NAME}_t.h")
    list(APPEND SEARCH_PATHS ${SGX_INCLUDE_DIR})
    
    if(NOT WIN32)
      string(REPLACE ";" ":" SEARCH_PATHS "${SEARCH_PATHS}")
    endif()
    if(${use_prefix})
      set(USE_PREFIX "--use-prefix")
    endif()
    
    add_custom_target(
      ${target}-${EDL_NAME}-edlhdr ALL
      ${SGX_EDGER8R} ${USE_PREFIX} --cpp --trusted --header-only "${EDL_ABSPATH}" --search-path "${SEARCH_PATHS}"
      COMMAND cmake -E copy_if_different ${EDL_T_H} ${EDL_TT_H}
      BYPRODUCTS ${EDL_TT_H}
      DEPENDS ${EDL_ABSPATH}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    set_property(
      DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
      ${EDL_T_H} ${EDL_TT_H})

      add_dependencies(${PARENT_TARGET} ${target}-${EDL_NAME}-edlhdr)

  endfunction()

  function(add_trusted_header target)
    set(optionArgs USE_PREFIX)
    set(oneValueArgs EDL LDSCRIPT)
    set(multiValueArgs SRCS HEADER_ONLY_LIBS EDL_SEARCH_PATHS)

    cmake_parse_arguments("SGX" "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_filename_component(EDL_NAME ${SGX_EDL} NAME_WE)
    get_filename_component(EDL_ABSPATH ${SGX_EDL} ABSOLUTE)
    set(EDL_T_H "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_t.h")
    set(EDL_TT_H "${CMAKE_CURRENT_BINARY_DIR}/trusted/${EDL_NAME}_t.h")
    
    list(APPEND SGX_EDL_SEARCH_PATHS ${SGX_INCLUDE_DIR})
    
    if(NOT WIN32)
      string(REPLACE ";" ":" SGX_EDL_SEARCH_PATHS "${SGX_EDL_SEARCH_PATHS}")
    endif()
    if(${SGX_USE_PREFIX})
      set(USE_PREFIX "--use-prefix")
    endif()
    
    add_custom_command(
      OUTPUT ${EDL_TT_H}
      BYPRODUCTS ${EDL_T_H}
      COMMAND ${SGX_EDGER8R} ${USE_PREFIX} --cpp --trusted --header-only "${EDL_ABSPATH}" --search-path "${SGX_EDL_SEARCH_PATHS}"
      COMMAND cmake -E copy_if_different ${EDL_T_H} ${EDL_TT_H}
      DEPENDS ${EDL_ABSPATH}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_custom_target(${target}-create-header ALL DEPENDS ${EDL_TT_H})
    add_library(${target} INTERFACE)
    add_dependencies(${target} ${target}-create-header)
    target_include_directories(${target} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>" )

    set_property(
      DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
      ${EDL_T_H} ${EDL_TT_H})

  endfunction()

  function(add_untrusted_header target)
    set(optionArgs USE_PREFIX)
    set(oneValueArgs EDL LDSCRIPT)
    set(multiValueArgs SRCS HEADER_ONLY_LIBS EDL_SEARCH_PATHS)

    cmake_parse_arguments("SGX" "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_filename_component(EDL_NAME ${SGX_EDL} NAME_WE)
    get_filename_component(EDL_ABSPATH ${SGX_EDL} ABSOLUTE)
    set(EDL_T_H "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.h")
    set(EDL_TT_H "${CMAKE_CURRENT_BINARY_DIR}/untrusted/${EDL_NAME}_u.h")
    
    list(APPEND SGX_EDL_SEARCH_PATHS ${SGX_INCLUDE_DIR})
    
    if(NOT WIN32)
      string(REPLACE ";" ":" SGX_EDL_SEARCH_PATHS "${SGX_EDL_SEARCH_PATHS}")
    endif()
    if(${SGX_USE_PREFIX})
      set(USE_PREFIX "--use-prefix")
    endif()
    
    add_custom_command(
      OUTPUT ${EDL_TT_H}
      BYPRODUCTS ${EDL_T_H}
      COMMAND ${SGX_EDGER8R} ${USE_PREFIX} --cpp --untrusted --header-only "${EDL_ABSPATH}" --search-path "${SGX_EDL_SEARCH_PATHS}"
      COMMAND cmake -E copy_if_different ${EDL_T_H} ${EDL_TT_H}
      DEPENDS ${EDL_ABSPATH}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_custom_target(${target}-create-header ALL DEPENDS ${EDL_TT_H})
    add_library(${target} INTERFACE)
    add_dependencies(${target} ${target}-create-header)
    target_include_directories(${target} INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

    set_property(
      DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
      ${EDL_T_H} ${EDL_TT_H})

  endfunction()

  function(_build_trusted_edl_headers PARENT_TARGET edl SEARCH_PATHS use_prefix)
    set(els ${edl})
    separate_arguments(els)

    foreach(tgt ${els})
      _build_trusted_edl_header(${PARENT_TARGET} ${tgt} ${SEARCH_PATHS} ${use_prefix})
    endforeach()
  endfunction()

  # build trusted static library to be linked into enclave library
  function(add_trusted_library target)
    set(optionArgs USE_PREFIX)
    set(oneValueArgs EDL LDSCRIPT)
    set(multiValueArgs SRCS HEADER_ONLY_LIBS EDL_SEARCH_PATHS)

    cmake_parse_arguments("SGX" "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_library(${target} STATIC ${SGX_SRCS})
    if(NOT "${SGX_EDL}" STREQUAL "")
      _build_trusted_edl_headers(${target} ${SGX_EDL} "${SGX_EDL_SEARCH_PATHS}" ${SGX_USE_PREFIX})
    endif()

    set_target_properties(${target} PROPERTIES COMPILE_FLAGS
                                               ${ENCLAVE_CXX_FLAGS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    
    target_link_libraries(${target} PRIVATE ${SGX_HEADER_ONLY_LIBS})
  endfunction()

  function(enclave_edl_library target edl edl_search_paths use_prefix edl_include_path)
    get_filename_component(EDL_NAME ${edl} NAME_WE)
    get_filename_component(EDL_ABSPATH ${edl} ABSOLUTE)
    set(EDL_T_C "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_t.c")
    set(EDL_TT_C "${CMAKE_CURRENT_BINARY_DIR}/trusted/${EDL_NAME}_t.c")
    set(EDL_T_H "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_t.h")
    set(EDL_TT_H "${CMAKE_CURRENT_BINARY_DIR}/trusted/${EDL_NAME}_t.h")

    set(SEARCH_PATHS "")
    foreach(path ${edl_search_paths})
      get_filename_component(ABSPATH ${path} ABSOLUTE)
      list(APPEND SEARCH_PATHS "${ABSPATH}")
    endforeach()
    list(APPEND SEARCH_PATHS ${SGX_INCLUDE_DIR})
    if(NOT WIN32)
      string(REPLACE ";" ":" SEARCH_PATHS "${SEARCH_PATHS}" )
    endif()
    if(${use_prefix})
      set(USE_PREFIX "--use-prefix")
    endif()
      
    add_custom_command(
      OUTPUT ${EDL_TT_H} ${EDL_TT_C}
      BYPRODUCTS ${EDL_T_H} ${EDL_T_C}
      COMMAND ${SGX_EDGER8R} ${USE_PREFIX} --cpp --trusted "${EDL_ABSPATH}" --search-path "${SEARCH_PATHS}" 
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      COMMAND cmake -E copy_if_different ${EDL_T_H} ${EDL_TT_H}
      COMMAND cmake -E copy_if_different ${EDL_T_C} ${EDL_TT_C}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_library(${target} STATIC ${EDL_TT_H} ${EDL_TT_C})
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${ENCLAVE_C_FLAGS})
    set(INCLUDE_PATHS ${CMAKE_CURRENT_BINARY_DIR} ${ENCLAVE_SGX_INCLUDE_DIRS} ${edl_include_path})
    separate_arguments(INCLUDE_PATHS)
    target_include_directories(${target} 
      PUBLIC 
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>" 
      PRIVATE 
        ${INCLUDE_PATHS} 
        ${ENCLAVE_LIBCXX_INCLUDES}
    )
    target_compile_definitions(${target} PRIVATE ${ENCLAVE_DEFINES})
    target_compile_options(${target} PRIVATE ${ENCLAVE_COMPILE_OPTIONS})

    set_property(
      DIRECTORY
      APPEND
      PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
      ${EDL_T_H}
      ${EDL_TT_H}
      ${EDL_T_C}
      ${EDL_TT_C})
  endfunction()


  function(host_edl_library target edl edl_search_paths use_prefix edl_include_path)
    get_filename_component(EDL_NAME ${edl} NAME_WE)
    get_filename_component(EDL_ABSPATH ${edl} ABSOLUTE)
    set(EDL_U_C "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.cpp")
    set(EDL_UT_C "${CMAKE_CURRENT_BINARY_DIR}/untrusted/${EDL_NAME}_u.cpp")
    set(EDL_U_H "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.h")
    set(EDL_UT_H "${CMAKE_CURRENT_BINARY_DIR}/untrusted/${EDL_NAME}_u.h")

    set(SEARCH_PATHS "")
    foreach(path ${edl_search_paths})
      get_filename_component(ABSPATH ${path} ABSOLUTE)
      list(APPEND SEARCH_PATHS "${ABSPATH}")
    endforeach()
    list(APPEND SEARCH_PATHS ${SGX_INCLUDE_DIR})

    if(NOT WIN32)
      string(REPLACE ";" ":" SEARCH_PATHS "${SEARCH_PATHS}" )
    endif()
    if(${use_prefix})
      set(USE_PREFIX "--use-prefix")
    endif()
      
    add_custom_command(
      OUTPUT ${EDL_UT_H} ${EDL_UT_C}
      BYPRODUCTS ${EDL_U_H} ${EDL_U_C}
      COMMAND ${SGX_EDGER8R} ${USE_PREFIX} --cpp --untrusted "${EDL_ABSPATH}" --search-path "${SEARCH_PATHS}" 
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      COMMAND cmake -E copy_if_different ${EDL_U_H} ${EDL_UT_H}
      COMMAND cmake -E copy_if_different ${EDL_U_C} ${EDL_UT_C}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_library(${target} STATIC ${EDL_UT_H} ${EDL_UT_C})
    #set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${ENCLAVE_C_FLAGS})
    set(INCLUDE_PATHS ${CMAKE_CURRENT_BINARY_DIR} ${HOST_SGX_INCLUDE_DIRS} ${edl_include_path})
    separate_arguments(INCLUDE_PATHS)
    target_include_directories(${target} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>" PRIVATE ${INCLUDE_PATHS})
    target_compile_definitions(${target} PRIVATE ${HOST_DEFINES})
    target_compile_options(${target} PRIVATE ${HOST_COMPILE_OPTIONS})

    set_property(
      DIRECTORY
      APPEND
      PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
      ${EDL_U_H}
      ${EDL_UT_H}
      ${EDL_U_C}
      ${EDL_UT_C})
  endfunction()
  

  # build enclave shared library
  function(add_enclave_library target)
    set(optionArgs USE_PREFIX)
    set(oneValueArgs EDL_IMPL LDSCRIPT)
    set(multiValueArgs SRCS TRUSTED_LIBS HEADER_ONLY_LIBS EDL EDL_SEARCH_PATHS EDL_INCLUDE_PATH)
    cmake_parse_arguments("SGX" "${optionArgs}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    if(NOT "${SGX_EDL_IMPL}" STREQUAL "")
      if("${SGX_EDL_SEARCH_PATHS}" STREQUAL "")
        message(
          FATAL_ERROR
            "${target}: SGX enclave edl file search paths are not provided!")
      endif()
      if(NOT "${SGX_LDSCRIPT}" STREQUAL "")
        get_filename_component(LDS_ABSPATH ${SGX_LDSCRIPT} ABSOLUTE)
        set(LDSCRIPT_FLAG "-Wl,--version-script=${LDS_ABSPATH}")
      endif()
      add_library(${target} SHARED ${SGX_SRCS})
      enclave_edl_library(${target}-edlobj ${SGX_EDL_IMPL} "${SGX_EDL_SEARCH_PATHS}" ${SGX_USE_PREFIX} "${SGX_EDL_INCLUDE_PATH}")
      target_link_libraries(${target} PRIVATE ${target}-edlobj ${SGX_TRUSTED_LIBS} ${SGX_HEADER_ONLY_LIBS})
    else()
      add_library(${target} SHARED ${SGX_SRCS})
    endif()

    if(NOT "${SGX_EDL}" STREQUAL "")
      _build_trusted_edl_headers(${target} ${SGX_EDL} "${SGX_EDL_SEARCH_PATHS}" ${SGX_USE_PREFIX})
    endif()

    set_target_properties(${target} PROPERTIES COMPILE_FLAGS
                                               ${ENCLAVE_CXX_FLAGS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    set(TLIB_LIST "")
    foreach(TLIB ${SGX_TRUSTED_LIBS})
      string(APPEND TLIB_LIST "$<TARGET_FILE:${TLIB}> ")
      add_dependencies(${target} ${TLIB})
    endforeach()

    if(NOT WIN32)
    set(SGX_LINKER_FLAGS "${SGX_COMMON_CFLAGS} \
      -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L${SGX_LIBRARY_PATH} \
      -Wl,--whole-archive  -l${SGX_TSTDC_LIB} -l${SGX_TRTS_LIB} -l${SGX_TSVC_LIB}  -l${SGX_TCRYPTO_LIB}  -l${SGX_TCXX_LIB} -Wl,--no-whole-archive \
      -Wl,--start-group ${TLIB_LIST} -l${SGX_URTS_LIB} -lsgx_tkey_exchange -Wl,--end-group \
      -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
      -Wl,-pie,-eenclave_entry -Wl,--export-dynamic \
      ${LDSCRIPT_FLAG} \
      -Wl,--defsym,__ImageBase=0")

      target_link_libraries(
        ${target}
        PRIVATE
        ${SGX_HEADER_ONLY_LIBS}
        ${SGX_LINKER_FLAGS})
    else()
      target_link_directories(${target} PRIVATE ${SGX_LIBRARY_PATH})

        target_link_options(${target} 
          PRIVATE  
            /NOENTRY 
            /NXCOMPAT 
            #/PDB:${CMAKE_CURRENT_BINARY_DIR}/${target}.pdb 
            /DYNAMICBASE 
            /DEBUG 
            /DLL  
            /OPT:REF 
            /OPT:ICF 
            /NOLOGO 
            /NODEFAULTLIB 
            /TLBID:1 )
    endif()
  endfunction()

  # sign the enclave, according to configurations one-step or two-step signing
  # will be performed. default one-step signing output enclave name is
  # target.signed.so, change it with OUTPUT option.
  function(enclave_sign target)
    set(oneValueArgs KEY CONFIG OUTPUT)
    cmake_parse_arguments("SGX" "" "${oneValueArgs}" "" ${ARGN})
    if("${SGX_CONFIG}" STREQUAL "")
      message(FATAL_ERROR "${target}: SGX enclave config is not provided!")
    endif()
    if("${SGX_KEY}" STREQUAL "")
      if(SGX_HW OR SGX_MODE STREQUAL "release")
        message(FATAL_ERROR "Private key used to sign enclave is not provided!")
      endif()
    else()
      if(SGX_MODE STREQUAL "release")
        if("${SGX_RELEASE_KEY_DIR}" STREQUAL "")
          message(FATAL_ERROR "Variable SGX_RELEASE_KEY_DIR not set for finding release signatures")
        endif()
        set(KEY_ABSPATH ${SGX_RELEASE_KEY_DIR}/${SGX_KEY})
      else()
        get_filename_component(KEY_ABSPATH ${SGX_KEY} ABSOLUTE)
      endif()
    endif()
    if("${SGX_OUTPUT}" STREQUAL "")
      if(WIN32)
        set(OUTPUT_NAME "${target}.signed.dll")
      else()
      set(OUTPUT_NAME "${target}.signed.so")
      endif()
    else()
      set(OUTPUT_NAME ${SGX_OUTPUT})
    endif()

    get_filename_component(CONFIG_ABSPATH ${SGX_CONFIG} ABSOLUTE)

    get_target_property(OUTPUT_DIR ${target} LIBRARY_OUTPUT_DIRECTORY)
    set(${target}_sign_OUTPUT_NAME ${OUTPUT_DIR}/${OUTPUT_NAME} CACHE STRING "signed target file name")
    if(SGX_HW AND SGX_MODE STREQUAL "release")
      add_custom_target(
        ${target}_sign ALL
        DEPENDS ${target}
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "First step ${SGX_ENCLAVE_SIGNER} signing $<TARGET_FILE:${target}>"
        COMMAND ${SGX_ENCLAVE_SIGNER} gendata -config ${CONFIG_ABSPATH} -enclave "$<TARGET_FILE:${target}>" -out "$<TARGET_FILE_DIR:${target}>/${target}_hash.hex"
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Second step openssl signing"
        COMMAND ${CMAKE_SOURCE_DIR}/3rdparty/bin/openssl.exe dgst -sha256 -out "$<TARGET_FILE_DIR:${target}>/${target}.signed.hex" -sign ${KEY_ABSPATH} -keyform PEM "$<TARGET_FILE_DIR:${target}>/${target}_hash.hex"
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Third step ${SGX_ENCLAVE_SIGNER} signing"
        COMMAND ${SGX_ENCLAVE_SIGNER} catsig -enclave "$<TARGET_FILE:${target}>" -out "$<TARGET_FILE_DIR:${target}>/${target}.signed.dll" -key $pk -sig "$<TARGET_FILE_DIR:${target}>/${target}.signed.hex" -unsigned "$<TARGET_FILE_DIR:${target}>/${target}_hash.hex" -config "${SGX_CONFIG}"
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Signing compelte"
        USES_TERMINAL
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    else()
      message("COMMAND ${SGX_ENCLAVE_SIGNER} sign -key ${KEY_ABSPATH} -config ${CONFIG_ABSPATH} -enclave ${target} -out ${target}/${OUTPUT_NAME}")
      add_custom_target(${target}_sign ALL
        COMMAND ${SGX_ENCLAVE_SIGNER} sign -key ${KEY_ABSPATH} -config ${CONFIG_ABSPATH} -enclave $<TARGET_FILE:${target}> -out $<TARGET_FILE_DIR:${target}>/${OUTPUT_NAME}
        USES_TERMINAL
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    endif()

    set(CLEAN_FILES
        "$<TARGET_FILE_DIR:${target}>/${OUTPUT_NAME};$<TARGET_FILE_DIR:${target}>/${target}_hash.hex"
    )
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
                                           "${CLEAN_FILES}")
  endfunction()

  function(_build_untrusted_edl_header PARENT_TARGET edl SEARCH_PATHS use_prefix)
    get_filename_component(EDL_NAME ${edl} NAME_WE)
    get_filename_component(EDL_ABSPATH ${edl} ABSOLUTE)
    set(EDL_T_H "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.h")
    set(EDL_TT_H "${CMAKE_CURRENT_BINARY_DIR}/untrusted/${EDL_NAME}_u.h")
    list(APPEND SEARCH_PATHS ${SGX_INCLUDE_DIR})
    if(NOT WIN32)
      string(REPLACE ";" ":" SEARCH_PATHS "${SEARCH_PATHS}")
    endif()
    if(${use_prefix})
      set(USE_PREFIX "--use-prefix")
    endif()

    add_custom_target(
      ${target}-${EDL_NAME}-edlhdr ALL
      ${SGX_EDGER8R} ${USE_PREFIX} --cpp --untrusted --header-only "${EDL_ABSPATH}" --search-path "${SEARCH_PATHS}"
      COMMAND cmake -E copy_if_different ${EDL_T_H} ${EDL_TT_H}
      BYPRODUCTS ${EDL_TT_H}
      DEPENDS ${EDL_ABSPATH}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    set_property(
      DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
      ${EDL_T_H} ${EDL_TT_H})

      add_dependencies(${PARENT_TARGET} ${target}-${EDL_NAME}-edlhdr)

  endfunction()

  function(_build_untrusted_edl_headers PARENT_TARGET edl SEARCH_PATHS use_prefix)
    set(els ${edl})
    separate_arguments(els)

    foreach(tgt ${els})
      _build_untrusted_edl_header(${PARENT_TARGET} ${tgt} ${SEARCH_PATHS} ${use_prefix})
    endforeach()
  endfunction()

  function(_build_untrusted_edl_impl PARENT_TARGET SGX_EDL_IMPL SEARCH_PATHS use_prefix HEADER_ONLY_LIBS EDL_INCLUDE_PATHS)
    list(APPEND SEARCH_PATHS ${SGX_INCLUDE_DIR})
    if(NOT WIN32)
      string(REPLACE ";" ":" SEARCH_PATHS "${SEARCH_PATHS}")
    endif()
    foreach(EDL ${SGX_EDL_IMPL})
      get_filename_component(EDL_NAME ${EDL} NAME_WE)
      get_filename_component(EDL_ABSPATH ${EDL} ABSOLUTE)
      set(EDL_U_C "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.c")
      set(EDL_U_H "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.h")
      set(EDL_UU_C "${CMAKE_CURRENT_BINARY_DIR}/untrusted/${EDL_NAME}_u.c")
      set(EDL_UU_H "${CMAKE_CURRENT_BINARY_DIR}/untrusted/${EDL_NAME}_u.h")
      
      if(${use_prefix})
        set(USE_PREFIX "--use-prefix")
      endif()

      add_custom_target(
        ${PARENT_TARGET}-${EDL_NAME}-edlhdr ALL
        COMMAND ${SGX_EDGER8R} ${USE_PREFIX} --cpp --untrusted "${EDL_ABSPATH}" --search-path "${SEARCH_PATHS}"
        COMMAND cmake -E copy_if_different ${EDL_U_H} ${EDL_UU_H}
        COMMAND cmake -E copy_if_different ${EDL_U_C} ${EDL_UU_C}
        BYPRODUCTS ${EDL_UU_H} ${EDL_UU_C}
        DEPENDS ${EDL_ABSPATH}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

      set_property(
        DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
        ${EDL_U_H} ${EDL_UU_H} ${EDL_U_C} ${EDL_UU_C})
      
      add_dependencies(${PARENT_TARGET} ${PARENT_TARGET}-${EDL_NAME}-edlhdr)

      add_library(${PARENT_TARGET}-${EDL_NAME}-edlc STATIC ${EDL_UU_H} ${EDL_UU_C})
      target_include_directories(${PARENT_TARGET}-${EDL_NAME}-edlc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/untrusted" "${SGX_INCLUDE_DIR}" "${EDL_INCLUDE_PATHS}")
      target_link_libraries(${PARENT_TARGET}-${EDL_NAME}-edlc PRIVATE ${HEADER_ONLY_LIBS})
      
      list(APPEND EDL_EDLCS ${PARENT_TARGET}-${EDL_NAME}-edlc)
      set(EDL_EDLCS ${EDL_EDLCS} PARENT_SCOPE)
    endforeach()
  endfunction()

  function(add_untrusted_library target mode)
    set(optionArgs USE_PREFIX)
    set(oneValueArgs EDL LDSCRIPT)
    set(multiValueArgs SRCS HEADER_ONLY_LIBS EDL_SEARCH_PATHS EDL_IMPL EDL_INCLUDE_PATHS)
    cmake_parse_arguments("SGX" "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT "${mode}" STREQUAL "")
    set(mode STATIC)
    endif()
    add_library(${target} ${mode} ${SGX_SRCS})
    if(NOT "${SGX_EDL}" STREQUAL "")
      _build_untrusted_edl_headers(${target} ${SGX_EDL} "${SGX_EDL_SEARCH_PATHS}" ${SGX_USE_PREFIX})
    endif()
    
    set(EDL_EDLCS "")
    if(NOT "${SGX_EDL_IMPL}" STREQUAL "")
      _build_untrusted_edl_impl(${target} ${SGX_EDL_IMPL} "${SGX_EDL_SEARCH_PATHS}" ${SGX_USE_PREFIX} "${SGX_HEADER_ONLY_LIBS}" "${SGX_EDL_INCLUDE_PATHS}")
    endif()

    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${APP_CXX_FLAGS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} /usr/include ${SGX_INCLUDE_DIR})
    target_link_libraries(${target}  PUBLIC ${EDL_EDLCS} PRIVATE ${SGX_HEADER_ONLY_LIBS})
  endfunction()

  function(add_untrusted_executable_simple target)
    set(optionArgs USE_PREFIX)
    set(multiValueArgs SRCS HEADER_ONLY_LIBS EDL_SEARCH_PATHS EDL EDL_IMPL EDL_INCLUDE_PATHS)
    cmake_parse_arguments("SGX" "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target} ${SGX_SRCS})
    if(NOT "${SGX_EDL}" STREQUAL "")
      _build_untrusted_edl_headers(${target} ${SGX_EDL} "${SGX_EDL_SEARCH_PATHS}" ${SGX_USE_PREFIX})
    endif()
    
    set(EDL_EDLCS "")
    if(NOT "${SGX_EDL_IMPL}" STREQUAL "")
      _build_untrusted_edl_impl(${target} ${SGX_EDL_IMPL} "${SGX_EDL_SEARCH_PATHS}" ${SGX_USE_PREFIX} "${SGX_HEADER_ONLY_LIBS}" "${SGX_EDL_INCLUDE_PATHS}")
    endif()

    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${APP_CXX_FLAGS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} /usr/include ${SGX_INCLUDE_DIR})
    target_link_libraries(${target} PUBLIC ${EDL_EDLCS} PRIVATE ${SGX_HEADER_ONLY_LIBS})
  endfunction()

  function(add_untrusted_executable target)
    set(optionArgs USE_PREFIX)
    set(multiValueArgs SRCS EDL EDL_SEARCH_PATHS)
    cmake_parse_arguments("SGX" "${optionArgs}" "" "${multiValueArgs}" ${ARGN})
    if("${SGX_EDL}" STREQUAL "")
      message(FATAL_ERROR "SGX enclave edl file is not provided!")
    endif()
    if("${SGX_EDL_SEARCH_PATHS}" STREQUAL "")
      message(FATAL_ERROR "SGX enclave edl file search paths are not provided!")
    endif()

    set(EDL_U_SRCS "")
    foreach(EDL ${SGX_EDL})
      get_filename_component(EDL_NAME ${EDL} NAME_WE)
      get_filename_component(EDL_ABSPATH ${EDL} ABSOLUTE)
      set(EDL_U_C "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.c")
      set(SEARCH_PATHS "")
      foreach(path ${SGX_EDL_SEARCH_PATHS})
        get_filename_component(ABSPATH ${path} ABSOLUTE)
        list(APPEND SEARCH_PATHS "${ABSPATH}")
      endforeach()
      list(APPEND SEARCH_PATHS ${SGX_INCLUDE_DIR})
      if(NOT WIN32)
        string(REPLACE ";" ":" SEARCH_PATHS "${SEARCH_PATHS}")
      endif()
      if(${SGX_USE_PREFIX})
        set(USE_PREFIX "--use-prefix")
      endif()
      add_custom_command(
        OUTPUT ${EDL_U_C}
        COMMAND ${SGX_EDGER8R} ${USE_PREFIX} --cpp --untrusted "${EDL_ABSPATH}" --search-path "${SEARCH_PATHS}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

      list(APPEND EDL_U_SRCS ${EDL_U_C})
    endforeach()

    add_executable(${target} ${SGX_SRCS} ${EDL_U_SRCS})
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${APP_CXX_FLAGS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(
      ${target}
      "${SGX_COMMON_CFLAGS} \
                                         -L${SGX_LIBRARY_PATH} \
                                         -l${SGX_URTS_LIB} \
                                         -l${SGX_USVC_LIB} \
                                         -lsgx_ukey_exchange \
                                         -lpthread")

    set_property(
      DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
                                "${CMAKE_CURRENT_BINARY_DIR}/${EDL_NAME}_u.h")
  endfunction()

else(SGX_FOUND)
  message(WARNING "Intel SGX SDK not found!")
  if(SGX_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find Intel SGX SDK!")
  endif()
endif(SGX_FOUND)
