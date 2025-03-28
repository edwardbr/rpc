function(
  RPCGenerate
  name
  idl
  base_dir
  output_path
  sub_directory
  namespace

  # multivalue expects string "dependencies" multivalue expects string "link_libraries" multivalue expects string
  # "include_paths" multivalue expects string "defines" multivalue expects string "additional_headers" optional_val mock
)
  set(options suppress_catch_stub_exceptions)
  set(singleValueArgs mock install_dir)
  set(multiValueArgs
    dependencies
    link_libraries
    include_paths
    defines
    additional_headers
    rethrow_stub_exception
    additional_stub_header)

  # split out multivalue variables
  cmake_parse_arguments(
    "params"
    "${options}"
    "${singleValueArgs}"
    "${multiValueArgs}"
    ${ARGN})

  # keep relative path of idl for install, else use only the file name
  cmake_path(IS_RELATIVE idl idl_is_relative)

  if(${idl_is_relative})
    cmake_path(GET idl PARENT_PATH idl_relative_dir)
    cmake_path(
      APPEND
      base_dir
      ${idl}
      OUTPUT_VARIABLE
      idl)
  else()
    set(idl_relative_dir .)
  endif()

  get_filename_component(base_file_name ${idl} NAME_WE)
  set(header_path ${sub_directory}/${base_file_name}.h)
  set(proxy_path ${sub_directory}/${base_file_name}_proxy.cpp)
  set(stub_path ${sub_directory}/${base_file_name}_stub.cpp)
  set(stub_header_path ${sub_directory}/${base_file_name}_stub.h)
  set(full_header_path ${output_path}/include/${header_path})
  set(full_proxy_path ${output_path}/src/${proxy_path})
  set(full_stub_path ${output_path}/src/${stub_path})
  set(full_stub_header_path ${output_path}/include/${stub_header_path})
  set(yas_path ${sub_directory}/yas/${base_file_name}.cpp)
  set(full_yas_path ${output_path}/src/${yas_path})

  if(${DEBUG_RPC_GEN})
    message("RPCGenerate name ${name}")
    message("idl ${idl}")
    message("base_dir ${base_dir}")
    message("output_path ${output_path}")
    message("sub_directory ${sub_directory}")
    message("namespace ${namespace}")
    message("suppress_catch_stub_exceptions ${params_suppress_catch_stub_exceptions}")
    message("install_dir ${params_install_dir}")
    message("dependencies ${params_dependencies}")
    message("link_libraries ${params_link_libraries}")
    message("additional_headers ${params_additional_headers}")
    message("rethrow_stub_exception ${params_rethrow_stub_exception}")
    message("additional_stub_header ${params_additional_stub_header}")
    message("paths ${params_include_paths}")
    message("defines ${params_defines}")
    message("mock ${params_mock}")
    message("header_path ${header_path}")
    message("proxy_path ${proxy_path}")
    message("stub_path ${stub_path}")
    message("stub_header_path ${stub_header_path}")
    message("full_header_path ${full_header_path}")
    message("full_proxy_path ${full_proxy_path}")
    message("full_stub_path ${full_stub_path}")
    message("full_stub_header_path ${full_stub_header_path}")
    message("yas_path ${yas_path}")
    message("full_yas_path ${full_yas_path}")
  endif()

  if(EXISTS ENCLAVE_MARSHALLER_EXECUTABLE)
    set(ENCLAVE_MARSHALLER ${ENCLAVE_MARSHALLER_EXECUTABLE})
  else()
    set(ENCLAVE_MARSHALLER generator)
  endif()

  set(PATHS_PARAMS "")
  set(ADDITIONAL_HEADERS "")
  set(RETHROW_STUB_EXCEPTION "")
  set(ADDITIONAL_STUB_HEADER "")
  set(GENERATED_DEPENDANCIES generator)

  foreach(path ${params_include_paths})
    set(PATHS_PARAMS ${PATHS_PARAMS} --path "${path}")
  endforeach()

  foreach(define ${params_defines})
    set(PATHS_PARAMS ${PATHS_PARAMS} -D "${define}")
  endforeach()

  foreach(additional_headers ${params_additional_headers})
    set(ADDITIONAL_HEADERS ${ADDITIONAL_HEADERS} --additional_headers "${additional_headers}")
  endforeach()

  foreach(rethrow ${params_rethrow_stub_exception})
    set(RETHROW_STUB_EXCEPTION ${RETHROW_STUB_EXCEPTION} --rethrow_stub_exception "${rethrow}")
  endforeach()

  foreach(stub_header ${params_additional_stub_header})
    set(ADDITIONAL_STUB_HEADER ${ADDITIONAL_STUB_HEADER} --additional_stub_header "${stub_header}")
  endforeach()

  foreach(dep ${params_dependencies})
    if(TARGET ${dep}_generate)
      get_target_property(dep_base_dir ${dep}_generate base_dir)

      if(dep_base_dir)
        set(PATHS_PARAMS ${PATHS_PARAMS} --path "${dep_base_dir}")
      endif()

      set(GENERATED_DEPENDANCIES ${GENERATED_DEPENDANCIES} ${dep}_generate)
      else()
      message("target ${dep}_generate does not exist so skipped")
    endif()
  endforeach()

  if(NOT ${namespace} STREQUAL "")
    set(PATHS_PARAMS ${PATHS_PARAMS} --namespace "${namespace}")
  endif()

  if(DEFINED params_mock AND NOT ${params_mock} STREQUAL "")
    set(PATHS_PARAMS ${PATHS_PARAMS} --mock "${params_mock}")
  endif()

  if(${params_suppress_catch_stub_exceptions})
    set(PATHS_PARAMS ${PATHS_PARAMS} --suppress_catch_stub_exceptions)
  endif()

  if(${DEBUG_RPC_GEN})
    message(
      "
    add_custom_command(OUTPUT ${full_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path} ${full_yas_path}
      COMMAND ${ENCLAVE_MARSHALLER}
      --idl ${idl}
      --module_name ${name}_idl
      --output_path ${output_path}
      --header ${header_path}
      --proxy ${proxy_path}
      --stub ${stub_path}
      --stub_header ${stub_header_path}
      ${PATHS_PARAMS}
      ${ADDITIONAL_HEADERS}
      ${RETHROW_STUB_EXCEPTION}
      ${ADDITIONAL_STUB_HEADER}
      MAIN_DEPENDENCY ${idl}
      IMPLICIT_DEPENDS ${idl}
      DEPENDS ${GENERATED_DEPENDANCIES}
      COMMENT \"Running generator ${idl}\"
    )

    add_custom_target(${name}_idl_generate DEPENDS ${full_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path} ${full_yas_path})

    set_target_properties(${name}_idl_generate PROPERTIES base_dir ${base_dir})

    foreach(dep ${params_dependencies})
      if(TARGET ${dep}_generate)
        add_dependencies(${name}_idl_generate ${dep}_generate)
      else()
        message(\"target ${dep}_generate does not exist so skipped\")
      endif()
    endforeach()

    foreach(dep ${params_link_libraries})
      if (TARGET ${dep})
        add_dependencies(${name}_idl_generate ${dep})
      else()
        message(\"target ${dep} does not exist so skipped\")
      endif()
    endforeach()
  ")
  endif()

  add_custom_command(
    OUTPUT ${full_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path} ${full_yas_path}
    COMMAND
    ${ENCLAVE_MARSHALLER} --idl ${idl} --module_name ${name}_idl --output_path ${output_path} --header ${header_path}
    --proxy ${proxy_path} --stub ${stub_path} --stub_header ${stub_header_path} ${PATHS_PARAMS} ${ADDITIONAL_HEADERS} ${RETHROW_STUB_EXCEPTION} ${ADDITIONAL_STUB_HEADER}
    MAIN_DEPENDENCY ${idl}
    IMPLICIT_DEPENDS ${idl}
    DEPENDS ${GENERATED_DEPENDANCIES}
    COMMENT "Running generator ${idl}")

  add_custom_target(${name}_idl_generate DEPENDS ${full_header_path} ${full_proxy_path} ${full_stub_header_path}
    ${full_stub_path} ${full_yas_path})

  set_target_properties(${name}_idl_generate PROPERTIES base_dir ${base_dir})

  foreach(dep ${params_dependencies})
  if(${DEBUG_RPC_GEN})
    message("dep ${dep}")
  endif()
    if(TARGET ${dep}_generate)
      add_dependencies(${name}_idl_generate ${dep}_generate)
    else()
      message("target ${dep}_generate does not exist so skipped")
    endif()
  endforeach()

  foreach(dep ${params_link_libraries})
    if(TARGET ${dep})
      add_dependencies(${name}_idl_generate ${dep})
    else()
      message("target ${dep} does not exist so skipped")
    endif()
  endforeach()

    if(${DEBUG_RPC_GEN})
      message(
        "
    # #specify a host specific target
    add_library(${name}_idl_host STATIC ${full_header_path} ${full_stub_header_path} ${full_stub_path}
      ${full_proxy_path} ${full_yas_path})
    target_compile_definitions(${name}_idl_host PRIVATE ${HOST_DEFINES})
    target_include_directories(
      ${name}_idl_host
      PUBLIC \"$<BUILD_INTERFACE:${output_path}>\" \"$<BUILD_INTERFACE:${output_path}/include>\"
      PRIVATE ${HOST_INCLUDES} ${params_include_paths})
    target_compile_options(${name}_idl_host PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
    target_link_directories(${name}_idl_host PUBLIC ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_host PROPERTY COMPILE_PDB_NAME ${name}_idl_host)

    target_link_libraries(${name}_idl_host PUBLIC rpc::rpc_host yas_common)

    add_dependencies(${name}_idl_host ${name}_idl_generate)

    foreach(dep ${params_dependencies})
      if(TARGET ${dep}_generate)
        add_dependencies(${name}_idl_host ${dep}_generate)
      else()
        message(\"target ${dep}_generate does not exist so skipped\")
      endif()

      target_link_libraries(${name}_idl_host PUBLIC ${dep}_host)
    endforeach()

    foreach(dep ${params_link_libraries})
      if(TARGET ${dep}_host)
        target_link_libraries(${name}_idl_host PRIVATE ${dep}_host)
      else()
        message(\"target ${dep}_host does not exist so skipped\")
      endif()
    endforeach()

    if(BUILD_ENCLAVE)
      # ##################################################################################################################
      # #and an enclave specific target
      add_library(${name}_idl_enclave STATIC ${full_header_path} ${full_stub_header_path} ${full_stub_path}
        ${full_proxy_path} ${full_yas_path})
      target_compile_definitions(${name}_idl_enclave PRIVATE ${ENCLAVE_DEFINES})
      target_include_directories(
        ${name}_idl_enclave
        PUBLIC \"$<BUILD_INTERFACE:${output_path}>\" \"$<BUILD_INTERFACE:${output_path}/include>\"
        PRIVATE \"${output_path}/include\" ${ENCLAVE_LIBCXX_INCLUDES} ${params_include_paths})

      target_compile_options(${name}_idl_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS} ${WARN_OK})
      target_link_directories(${name}_idl_enclave PRIVATE ${SGX_LIBRARY_PATH})
      set_property(TARGET ${name}_idl_enclave PROPERTY COMPILE_PDB_NAME ${name}_idl_enclave)

      target_link_libraries(${name}_idl_enclave PUBLIC rpc::rpc_enclave yas_common)

      add_dependencies(${name}_idl_enclave ${name}_idl_generate)

      foreach(dep ${params_dependencies})
        if(TARGET ${dep}_generate)
          add_dependencies(${name}_idl_enclave ${dep}_generate)
        else()
          message(\"target ${dep}_generate does not exist so skipped\")
        endif()

        target_link_libraries(${name}_idl_enclave PUBLIC ${dep}_enclave)
      endforeach()

      foreach(dep ${params_link_libraries})
        if(TARGET ${dep}_enclave)
          target_link_libraries(${name}_idl_enclave PRIVATE ${dep}_enclave)
        else()
          message(\"target ${dep}_enclave does not exist so skipped\")
        endif()
      endforeach()
    endif()

    if(params_install_dir)
      install(DIRECTORY \"$<BUILD_INTERFACE:${output_path}/include/${sub_directory}>\" DESTINATION ${params_install_dir}/interfaces/include)
      install(DIRECTORY \"$<BUILD_INTERFACE:${output_path}/src/${sub_directory}>\" DESTINATION ${params_install_dir}/interfaces/src)
      install(DIRECTORY \"$<BUILD_INTERFACE:${output_path}/check_sums/${sub_directory}>\" DESTINATION ${params_install_dir}/interfaces/check_sums)
      install(FILES \"$<BUILD_INTERFACE:${idl}>\" DESTINATION ${params_install_dir}/${idl_relative_dir})
    endif()
    ")
    endif()

    # #specify a host specific target
    add_library(${name}_idl_host STATIC ${full_header_path} ${full_stub_header_path} ${full_stub_path}
      ${full_proxy_path} ${full_yas_path})
    target_compile_definitions(${name}_idl_host PRIVATE ${HOST_DEFINES})
    target_include_directories(
      ${name}_idl_host
      PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>"
      PRIVATE "${output_path}/include" ${HOST_INCLUDES} ${params_include_paths})
    target_compile_options(${name}_idl_host PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
    target_link_directories(${name}_idl_host PUBLIC ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_host PROPERTY COMPILE_PDB_NAME ${name}_idl_host)

    target_link_libraries(${name}_idl_host PUBLIC rpc::rpc_host yas_common)

    add_dependencies(${name}_idl_host ${name}_idl_generate)

    foreach(dep ${params_dependencies})
      if(TARGET ${dep}_generate)
        add_dependencies(${name}_idl_host ${dep}_generate)
      else()
        message("target ${dep}_generate does not exist so skipped")
      endif()

      target_link_libraries(${name}_idl_host PUBLIC ${dep}_host)
    endforeach()

    foreach(dep ${params_link_libraries})
      if(TARGET ${dep}_host)
        target_link_libraries(${name}_idl_host PRIVATE ${dep}_host)
      else()
        message("target ${dep}_host does not exist so skipped")
      endif()
    endforeach()

    if(BUILD_ENCLAVE)
      # ##################################################################################################################
      # #and an enclave specific target
      add_library(${name}_idl_enclave STATIC ${full_header_path} ${full_stub_header_path} ${full_stub_path}
        ${full_proxy_path} ${full_yas_path})
      target_compile_definitions(${name}_idl_enclave PRIVATE ${ENCLAVE_DEFINES})
      target_include_directories(
        ${name}_idl_enclave
        PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>"
        PRIVATE "${output_path}/include" ${ENCLAVE_LIBCXX_INCLUDES} ${params_include_paths})

      target_compile_options(${name}_idl_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS} ${WARN_OK})
      target_link_directories(${name}_idl_enclave PRIVATE ${SGX_LIBRARY_PATH})
      set_property(TARGET ${name}_idl_enclave PROPERTY COMPILE_PDB_NAME ${name}_idl_enclave)

      target_link_libraries(${name}_idl_enclave PUBLIC rpc::rpc_enclave yas_common)

      add_dependencies(${name}_idl_enclave ${name}_idl_generate)

      foreach(dep ${params_dependencies})
        if(TARGET ${dep}_generate)
          add_dependencies(${name}_idl_enclave ${dep}_generate)
        else()
          message("target ${dep}_generate does not exist so skipped")
        endif()

        target_link_libraries(${name}_idl_enclave PUBLIC ${dep}_enclave)
      endforeach()

      foreach(dep ${params_link_libraries})
        if(TARGET ${dep}_enclave)
          target_link_libraries(${name}_idl_enclave PRIVATE ${dep}_enclave)
        else()
          message("target ${dep}_enclave does not exist so skipped")
        endif()
      endforeach()
    endif()

    if(params_install_dir)
      install(DIRECTORY "$<BUILD_INTERFACE:${output_path}/include/${sub_directory}>" DESTINATION ${params_install_dir}/interfaces/include)
      install(DIRECTORY "$<BUILD_INTERFACE:${output_path}/src/${sub_directory}>" DESTINATION ${params_install_dir}/interfaces/src)
      install(DIRECTORY "$<BUILD_INTERFACE:${output_path}/check_sums/${sub_directory}>" DESTINATION ${params_install_dir}/interfaces/check_sums)
      install(FILES "$<BUILD_INTERFACE:${idl}>" DESTINATION ${params_install_dir}/${idl_relative_dir})
    endif()
endfunction()
