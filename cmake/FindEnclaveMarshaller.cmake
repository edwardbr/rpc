function(EnclaveMarshaller
          name
          idl
          base_dir
          output_path
          header
          proxy
          stub
          namespace
          #multivalue expects string "dependencies"
          #multivalue expects string "include_paths"
          #multivalue expects string "defines"
          #optional_val mock
          )

  set(options )
  set(singleValueArgs mock)
  set(multiValueArgs dependencies include_paths defines)

  #split out multivalue variables
  cmake_parse_arguments("params" "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

  set(tmp_path ${base_dir}/${idl})
  file(TO_CMAKE_PATH ${tmp_path} idl)
  #cmake_path(APPEND base_dir ${idl} OUTPUT_VARIABLE idl)

  set(full_header_path ${output_path}/include/${header})
  set(full_proxy_path ${output_path}/src/${proxy})
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
    message("full_proxy_path ${full_proxy_path}")
    message("full_stub_path ${full_stub_path}")
  endif()

  if(EXISTS ENCLAVE_MARSHALLER_EXECUTABLE)
    set(ENCLAVE_MARSHALLER ${ENCLAVE_MARSHALLER_EXECUTABLE})
  else()
    set(ENCLAVE_MARSHALLER generator)
  endif()

  set(PATHS_PARAMS "")
  foreach (path ${params_include_paths})
    set(PATHS_PARAMS ${PATHS_PARAMS} --path "${path}")
  endforeach()

  foreach (define ${params_defines})
    set(PATHS_PARAMS ${PATHS_PARAMS} -D "${define}")
  endforeach()

  foreach (dep ${params_dependencies})
    get_target_property(dep_base_dir ${dep}_generate base_dir)
    if(dep_base_dir)
      set(PATHS_PARAMS ${PATHS_PARAMS} --path "${dep_base_dir}")
    endif()
  endforeach()

  if(NOT ${namespace} STREQUAL "")
    set(PATHS_PARAMS ${PATHS_PARAMS} --namespace "${namespace}")
  endif()

  if(DEFINED params_mock AND NOT ${params_mock} STREQUAL "")
    set(PATHS_PARAMS ${PATHS_PARAMS} --mock "${params_mock}")
  endif()

  if(${DEBUG_RPC_GEN})
    message("
    add_custom_command(OUTPUT ${full_header_path}  ${full_proxy_path} ${full_stub_path}
    COMMAND ${ENCLAVE_MARSHALLER}
      --idl ${idl}
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
    MAIN_DEPENDENCY ${idl}
    IMPLICIT_DEPENDS ${idl}
    DEPENDS ${params_dependencies}
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

  add_custom_command(OUTPUT ${full_header_path}  ${full_proxy_path} ${full_stub_path}
    COMMAND ${ENCLAVE_MARSHALLER}
      --idl ${idl}
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
    MAIN_DEPENDENCY ${idl}
    IMPLICIT_DEPENDS ${idl}
    DEPENDS ${params_dependencies}
    COMMENT "Running generator ${idl}"
  )

  message(${name}_generate)
  add_custom_target(${name}_generate DEPENDS ${full_header_path}  ${full_proxy_path} ${full_stub_path})

  set_target_properties(${name}_generate PROPERTIES base_dir ${base_dir})

  add_library(${name} INTERFACE)
  add_dependencies(${name} ${name}_generate)

  foreach (dep ${params_dependencies})
    add_dependencies(${name} ${dep}_generate)
  endforeach()

  target_include_directories(${name} INTERFACE "$<BUILD_INTERFACE:${output_path}>" "$<BUILD_INTERFACE:${output_path}/include>")

  if(DEFINED params_dependencies)
    target_link_libraries(${name} INTERFACE ${params_dependencies})
  endif()
endfunction()
