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
  set(options)
  set(singleValueArgs mock)
  set(multiValueArgs
      dependencies
      link_libraries
      include_paths
      defines
      additional_headers)

  # split out multivalue variables
  cmake_parse_arguments(
    "params"
    "${options}"
    "${singleValueArgs}"
    "${multiValueArgs}"
    ${ARGN})

  cmake_path(
    APPEND
    base_dir
    ${idl}
    OUTPUT_VARIABLE
    idl)

  set(header_path ${sub_directory}/${name}.h)
  set(proxy_path ${sub_directory}/${name}_proxy.cpp)
  set(stub_path ${sub_directory}/${name}_stub.cpp)
  set(stub_header_path ${sub_directory}/${name}_stub.h)
  set(full_header_path ${output_path}/include/${header_path})
  set(full_proxy_path ${output_path}/src/${proxy_path})
  set(full_stub_path ${output_path}/src/${stub_path})
  set(full_stub_header_path ${output_path}/include/${stub_header_path})

  if(${DEBUG_RPC_GEN})
    message("RPCGenerate name ${name}")
    message("idl ${idl}")
    message("base_dir ${base_dir}")
    message("output_path ${output_path}")
    message("sub_directory ${sub_directory}")
    message("namespace ${namespace}")
    message("dependencies ${params_dependencies}")
    message("link_libraries ${params_link_libraries}")
    message("additional_headers ${params_additional_headers}")
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
  endif()

  if(EXISTS ENCLAVE_MARSHALLER_EXECUTABLE)
    set(ENCLAVE_MARSHALLER ${ENCLAVE_MARSHALLER_EXECUTABLE})
  else()
    set(ENCLAVE_MARSHALLER generator)
  endif()

  set(PATHS_PARAMS "")
  set(ADDITIONAL_HEADERS "")

  foreach(path ${params_include_paths})
    set(PATHS_PARAMS ${PATHS_PARAMS} --path "${path}")
  endforeach()

  foreach(define ${params_defines})
    set(PATHS_PARAMS ${PATHS_PARAMS} -D "${define}")
  endforeach()

  foreach(additional_headers ${params_additional_headers})
    set(ADDITIONAL_HEADERS ${ADDITIONAL_HEADERS} --additional_headers "${additional_headers}")
  endforeach()

  foreach(dep ${params_dependencies})
    if(TARGET ${dep}_generate)
      get_target_property(dep_base_dir ${dep}_generate base_dir)

      if(dep_base_dir)
        set(PATHS_PARAMS ${PATHS_PARAMS} --path "${dep_base_dir}")
      endif()
      if(BUILD_ENCLAVE)
        set(GENERATED_DEPENDANCIES ${GENERATED_DEPENDANCIES} ${dep}_generate)
      else()
        set(GENERATED_DEPENDANCIES ${GENERATED_DEPENDANCIES} ${dep} ${dep}_generate)
      endif()
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

  if(${DEBUG_RPC_GEN})
    message(
      "
    add_custom_command(OUTPUT ${full_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path}
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
    MAIN_DEPENDENCY ${idl}
    IMPLICIT_DEPENDS ${idl}
    DEPENDS ${GENERATED_DEPENDANCIES}
    COMMENT \"Running generator ${idl}\"
  )

  add_custom_target(${name}_idl_generate DEPENDS ${full_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path})

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
    OUTPUT ${full_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path}
    COMMAND
      ${ENCLAVE_MARSHALLER} --idl ${idl} --module_name ${name}_idl --output_path ${output_path} --header ${header_path}
      --proxy ${proxy_path} --stub ${stub_path} --stub_header ${stub_header_path} ${PATHS_PARAMS} ${ADDITIONAL_HEADERS}
    MAIN_DEPENDENCY ${idl}
    IMPLICIT_DEPENDS ${idl}
    DEPENDS ${GENERATED_DEPENDANCIES}
    COMMENT "Running generator ${idl}")

  add_custom_target(${name}_idl_generate DEPENDS ${full_header_path} ${full_proxy_path} ${full_stub_header_path}
                                                 ${full_stub_path})

  set_target_properties(${name}_idl_generate PROPERTIES base_dir ${base_dir})

  foreach(dep ${params_dependencies})
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

  if(BUILD_ENCLAVE)
    if(${DEBUG_RPC_GEN})
      message("
# #specify a host specific target
      add_library(${name}_idl_host STATIC
      ${full_header_path}
      ${full_stub_header_path}
      ${full_stub_path}
      ${full_proxy_path}
    )
    target_compile_definitions(${name}_idl_host PRIVATE ${HOST_DEFINES})
    target_include_directories(${name}_idl_host PUBLIC \"$<BUILD_INTERFACE:${output_path}>\" \"$<BUILD_INTERFACE:${output_path}/include>\" PRIVATE ${HOST_INCLUDES} ${params_include_paths})
    target_compile_options(${name}_idl_host PRIVATE ${HOST_COMPILE_OPTIONS})
    target_link_directories(${name}_idl_host PUBLIC ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_host PROPERTY COMPILE_PDB_NAME ${name}_idl_host)

    target_link_libraries(${name}_idl_host PUBLIC
      rpc::rpc_host
      yas_common
    )

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

    ###############################################################################
    # #and an enclave specific target
    add_library(${name}_idl_enclave STATIC
      ${full_header_path}
      ${full_stub_header_path}
      ${full_stub_path}
      ${full_proxy_path}
    )
    target_compile_definitions(${name}_idl_enclave PRIVATE ${ENCLAVE_DEFINES})
    target_include_directories(${name}_idl_enclave PUBLIC \"$<BUILD_INTERFACE:${output_path}>\" \"$<BUILD_INTERFACE:${output_path}/include>\" PRIVATE \"${output_path}/include\" ${ENCLAVE_LIBCXX_INCLUDES} ${params_include_paths})

    set(stripped_idl)
    cmake_path(GET idl STEM stripped_idl)
    install(DIRECTORY \"$<BUILD_INTERFACE:${output_path}>\" DESTINATION include/secretarium/idls)
    install(FILES     \"$<BUILD_INTERFACE:${idl}>\"         DESTINATION include/secretarium/idls/${stripped_idl})

    target_compile_options(${name}_idl_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
    target_link_directories(${name}_idl_enclave PRIVATE ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_enclave PROPERTY COMPILE_PDB_NAME ${name}_idl_enclave)

    target_link_libraries(${name}_idl_enclave PUBLIC
      rpc_enclave
      yas_common
    )

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

    ###############################################################################
    # #and an enclave specific target
    add_library(${name}_idl_enclave_v1 STATIC
      ${full_header_path}
      ${full_stub_header_path}
      ${full_stub_path}
      ${full_proxy_path}
    )
    target_compile_definitions(${name}_idl_enclave_v1
      PUBLIC
        NO_RPC_V2
      PRIVATE
        ${ENCLAVE_DEFINES})
    target_include_directories(${name}_idl_enclave_v1 PUBLIC \"$<BUILD_INTERFACE:${output_path}>\" \"$<BUILD_INTERFACE:${output_path}/include>\" PRIVATE ${ENCLAVE_LIBCXX_INCLUDES} ${params_include_paths})
    target_compile_options(${name}_idl_enclave_v1 PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
    target_link_directories(${name}_idl_enclave_v1 PRIVATE ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_enclave_v1 PROPERTY COMPILE_PDB_NAME ${name}_idl_enclave_v1)

    target_link_libraries(${name}_idl_enclave_v1 PUBLIC
      rpc_enclave_v1
      yas_common
    )

    add_dependencies(${name}_idl_enclave_v1 ${name}_idl_generate)

    foreach(dep ${params_dependencies})
      if(TARGET ${dep}_generate)
        add_dependencies(${name}_idl_enclave_v1 ${dep}_generate)
      else()
        message(\"target ${dep}_generate does not exist so skipped\")
      endif()
      target_link_libraries(${name}_idl_enclave_v1 PUBLIC ${dep}_enclave)
    endforeach()

    foreach(dep ${params_link_libraries})
      if(TARGET ${dep}_enclave)
        target_link_libraries(${name}_idl_enclave_v1 PRIVATE ${dep}_enclave)
      else()
        message(\"target ${dep}_enclave does not exist so skipped\")
      endif()
    endforeach()")
    endif()
    # #specify a host specific target
    add_library(${name}_idl_host STATIC ${full_header_path} ${full_stub_header_path} ${full_stub_path}
                                        ${full_proxy_path})
    target_compile_definitions(${name}_idl_host PRIVATE ${HOST_DEFINES})
    target_include_directories(
      ${name}_idl_host
      PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>"
      PRIVATE ${HOST_INCLUDES} ${params_include_paths})
    target_compile_options(${name}_idl_host PRIVATE ${HOST_COMPILE_OPTIONS})
    target_link_directories(${name}_idl_host PUBLIC ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_host PROPERTY COMPILE_PDB_NAME ${name}_idl_host)

    target_link_libraries(${name}_idl_host PUBLIC 
      rpc::rpc_host
      yas_common)

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

    # ##################################################################################################################
    # #and an enclave specific target
    add_library(${name}_idl_enclave STATIC ${full_header_path} ${full_stub_header_path} ${full_stub_path}
                                           ${full_proxy_path})
    target_compile_definitions(${name}_idl_enclave PRIVATE ${ENCLAVE_DEFINES})
    target_include_directories(
      ${name}_idl_enclave
      PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>"
      PRIVATE "${output_path}/include" ${ENCLAVE_LIBCXX_INCLUDES} ${params_include_paths})

    set(stripped_idl)
    cmake_path(GET idl STEM stripped_idl)
    install(DIRECTORY "$<BUILD_INTERFACE:${output_path}>" DESTINATION include/secretarium/idls)
    install(FILES "$<BUILD_INTERFACE:${idl}>" DESTINATION include/secretarium/idls/${stripped_idl})

    target_compile_options(${name}_idl_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
    target_link_directories(${name}_idl_enclave PRIVATE ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_enclave PROPERTY COMPILE_PDB_NAME ${name}_idl_enclave)

    target_link_libraries(${name}_idl_enclave PUBLIC 
      rpc_enclave
      yas_common)

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

    # ##################################################################################################################
    # #and an enclave specific target
    add_library(${name}_idl_enclave_v1 STATIC ${full_header_path} ${full_stub_header_path} ${full_stub_path}
                                              ${full_proxy_path})
    target_compile_definitions(
      ${name}_idl_enclave_v1
      PUBLIC NO_RPC_V2
      PRIVATE ${ENCLAVE_DEFINES})
    target_include_directories(
      ${name}_idl_enclave_v1
      PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>"
      PRIVATE ${ENCLAVE_LIBCXX_INCLUDES} ${params_include_paths})
    target_compile_options(${name}_idl_enclave_v1 PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
    target_link_directories(${name}_idl_enclave_v1 PRIVATE ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_idl_enclave_v1 PROPERTY COMPILE_PDB_NAME ${name}_idl_enclave_v1)

    target_link_libraries(${name}_idl_enclave_v1 PUBLIC 
      rpc_enclave_v1
      yas_common)

    add_dependencies(${name}_idl_enclave_v1 ${name}_idl_generate)

    foreach(dep ${params_dependencies})
      if(TARGET ${dep}_generate)
        add_dependencies(${name}_idl_enclave_v1 ${dep}_generate)
      else()
        message("target ${dep}_generate does not exist so skipped")
      endif()
      target_link_libraries(${name}_idl_enclave_v1 PUBLIC ${dep}_enclave)
    endforeach()

    foreach(dep ${params_link_libraries})
      if(TARGET ${dep}_enclave)
        target_link_libraries(${name}_idl_enclave_v1 PRIVATE ${dep}_enclave)
      else()
        message("target ${dep}_enclave does not exist so skipped")
      endif()
    endforeach()
  endif()
endfunction()
