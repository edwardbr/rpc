function(EnclaveMarshaller
  name
  idl
  base_dir
  output_path
  header
  proxy
  stub
  namespace

  # multivalue expects string "dependencies"
  # multivalue expects string "include_paths"
  # multivalue expects string "defines"
  # optional_val mock
)
  set(options)
  set(singleValueArgs mock)
  set(multiValueArgs dependencies include_paths defines)

  # split out multivalue variables
  cmake_parse_arguments("params" "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

  cmake_path(APPEND base_dir ${idl} OUTPUT_VARIABLE idl)

  set(full_header_path ${output_path}/include/${header})
  set(full_proxy_header_path ${output_path}/src/${proxy}.h)
  set(full_proxy_path ${output_path}/src/${proxy})
  set(full_stub_header_path ${output_path}/src/${stub}.h)
  set(full_stub_path ${output_path}/src/${stub})

  if(${DEBUG_RPC_GEN})
    message("EnclaveMarshaller name ${name}")
    message("idl ${idl}")
    message("base_dir ${base_dir}")
    message("output_path ${output_path}")
    message("header ${header}")
    message("proxy ${proxy}")
    message("stub ${stub}")
    message("namespace ${namespace}")
    message("dependencies ${params_dependencies}")
    message("paths ${params_include_paths}")
    message("defines ${params_defines}")
    message("mock ${params_mock}")
    message("full_header_path ${full_header_path}")
    message("full_proxy_header_path ${full_proxy_header_path}")
    message("full_proxy_path ${full_proxy_path}")
    message("full_stub_header_path ${full_stub_header_path}")
    message("full_stub_path ${full_stub_path}")
  endif()

  if(EXISTS ENCLAVE_MARSHALLER_EXECUTABLE)
    set(ENCLAVE_MARSHALLER ${ENCLAVE_MARSHALLER_EXECUTABLE})
  else()
    set(ENCLAVE_MARSHALLER generator)
  endif()

  set(PATHS_PARAMS "")

  foreach(path ${params_include_paths})
    set(PATHS_PARAMS ${PATHS_PARAMS} --path "${path}")
  endforeach()

  foreach(define ${params_defines})
    set(PATHS_PARAMS ${PATHS_PARAMS} -D "${define}")
  endforeach()

  foreach(dep ${params_dependencies})
    get_target_property(dep_base_dir ${dep}_generate base_dir)

    if(dep_base_dir)
      set(PATHS_PARAMS ${PATHS_PARAMS} --path "${dep_base_dir}")
    endif()
    set(GENERATED_DEPENDANCIES ${GENERATED_DEPENDANCIES} ${dep}_generate)
  endforeach()

  if(NOT ${namespace} STREQUAL "")
    set(PATHS_PARAMS ${PATHS_PARAMS} --namespace "${namespace}")
  endif()

  if(DEFINED params_mock AND NOT ${params_mock} STREQUAL "")
    set(PATHS_PARAMS ${PATHS_PARAMS} --mock "${params_mock}")
  endif()

  if(${DEBUG_RPC_GEN})
    message("
    add_custom_command(OUTPUT ${full_header_path} ${full_proxy_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path}
    COMMAND ${ENCLAVE_MARSHALLER}
      --idl ${idl}
      --module_name ${name}
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
    MAIN_DEPENDENCY ${idl}
    IMPLICIT_DEPENDS ${idl}
    DEPENDS ${GENERATED_DEPENDANCIES}
    COMMENT \"Running generator ${idl}\"
  )

  message(${name}_generate)
  add_custom_target(${name}_generate DEPENDS ${full_header_path}  ${full_proxy_path})

  set_target_properties(${name}_generate PROPERTIES base_dir ${base_dir})

  add_library(${name} INTERFACE)
  add_dependencies(${name} ${name}_generate)
  target_include_directories(${name} INTERFACE \"${output_path}\")

  if(DEFINED params_dependencies)
    target_link_libraries(${name} INTERFACE ${params_dependencies})
  endif()
")
  endif()

  add_custom_command(OUTPUT ${full_header_path} ${full_proxy_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path}
    COMMAND ${ENCLAVE_MARSHALLER}
    --idl ${idl}
    --module_name ${name}
    --output_path ${output_path}
    --header ${header}
    --proxy ${proxy}
    --stub ${stub}
    ${PATHS_PARAMS}
    MAIN_DEPENDENCY ${idl}
    IMPLICIT_DEPENDS ${idl}
    DEPENDS ${GENERATED_DEPENDANCIES}
    COMMENT "Running generator ${idl}"
  )

  message(${name}_generate)
  add_custom_target(${name}_generate DEPENDS ${full_header_path} ${full_proxy_header_path} ${full_proxy_path} ${full_stub_header_path} ${full_stub_path})

  set_target_properties(${name}_generate PROPERTIES base_dir ${base_dir})

  foreach(dep ${params_dependencies})
    message("add_dependencies(${name}_generate ${dep}_generate)")
    add_dependencies(${name}_generate ${dep}_generate)
  endforeach()

  #[[add_library(${name} STATIC
    ${full_header_path}
    ${full_stub_header_path}
    ${full_stub_path}
    ${full_proxy_header_path}
    ${full_proxy_path}
  )

  set_property(TARGET ${name}_host PROPERTY COMPILE_PDB_NAME ${name})

  target_link_libraries(${name} PUBLIC
    rpc_host
  )
  add_dependencies(${name} ${name}_generate)

  foreach(dep ${params_dependencies})
    add_dependencies(${name} ${dep}_generate)
  endforeach()

  target_include_directories(${name} PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>")

  if(DEFINED params_dependencies)
    target_link_libraries(${name} PUBLIC ${params_dependencies})
  endif()]]
  if(BUILD_ENCLAVE)
    # #specify a host specific target
    add_library(${name}_host STATIC
      ${full_header_path}
      ${full_stub_header_path}
      ${full_stub_path}
      ${full_proxy_header_path}
      ${full_proxy_path}
    )
    target_compile_definitions(${name}_host PRIVATE ${HOST_DEFINES})
    target_include_directories(${name}_host PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>")
    target_compile_options(${name}_host PRIVATE ${HOST_COMPILE_OPTIONS})
    target_link_directories(${name}_host PUBLIC ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_host PROPERTY COMPILE_PDB_NAME ${name}_host)

    target_link_libraries(${name}_host PUBLIC
      rpc_host
      yas_common
    )

    add_dependencies(${name}_host ${name}_generate)

    foreach(dep ${params_dependencies})
      add_dependencies(${name}_host ${dep}_generate)
      target_link_libraries(${name}_host PUBLIC ${dep}_host)
    endforeach()

    # #and an enclave specific target
    add_library(${name}_enclave STATIC
      ${full_header_path}
      ${full_stub_header_path}
      ${full_stub_path}
      ${full_proxy_header_path}
      ${full_proxy_path}
    )
    target_compile_definitions(${name}_enclave PRIVATE ${ENCLAVE_DEFINES})
    target_include_directories(${name}_enclave PUBLIC "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>" PRIVATE ${ENCLAVE_LIBCXX_INCLUDES})
    target_compile_options(${name}_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
    target_link_directories(${name}_enclave PRIVATE ${SGX_LIBRARY_PATH})
    set_property(TARGET ${name}_enclave PROPERTY COMPILE_PDB_NAME ${name}_enclave)

    target_link_libraries(${name}_enclave PUBLIC
      rpc_enclave
      yas_common
    )

    add_dependencies(${name}_enclave ${name}_generate)

    foreach(dep ${params_dependencies})
      add_dependencies(${name}_enclave ${dep}_generate)
      target_link_libraries(${name}_enclave PUBLIC ${dep}_enclave)
    endforeach()
  endif()
endfunction()
