function(EnclaveMarshaller 
          name
          idl
          output_path
          header
          proxy
          stub
          namespace
          #dependencies multivalue
          #include_paths multivalue
          )

  set(multiValueArgs dependencies include_paths)

  #split out multivalue variables
  cmake_parse_arguments("params" "" "" "${multiValueArgs}" ${ARGN})          

  message("EnclaveMarshaller name ${name}")
  message("dependencies ${params_dependencies}")
  message("idl ${idl}")
  message("output_path ${output_path}")
  message("header ${header}")
  message("proxy ${proxy}")
  message("stub ${stub}")
  message("paths ${params_include_paths}")
  message("namespace ${namespace}")

  # Test if including from FindFlatBuffers
  if(EXISTS ENCLAVE_MARSHALLER_EXECUTABLE)
    set(ENCLAVE_MARSHALLER ${ENCLAVE_MARSHALLER_EXECUTABLE})
  else()
    set(ENCLAVE_MARSHALLER in_zone_synchronous_generator)
  endif()

  set(PATHS_PARAMS)
  foreach (path ${params_include_paths})
    set(PATHS_PARAMS "${PATHS_PARAMS} --path ${path}")
  endforeach()
  separate_arguments(PATHS_PARAMS WINDOWS_COMMAND "${PATHS_PARAMS}")

#[[  message("  add_custom_command(
    OUTPUT ${output_path}/${header}  ${output_path}/${proxy} ${output_path}/${stub}
    COMMAND ${ENCLAVE_MARSHALLER} 
      --idl ${idl} 
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
      --namespace ${namespace}
    DEPENDS ${dependencies} ${idl})")]]

  add_custom_command(
    OUTPUT ${output_path}/${header}  ${output_path}/${proxy} ${output_path}/${stub}
    COMMAND ${ENCLAVE_MARSHALLER} 
      --idl ${idl} 
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
      --namespace ${namespace}
    DEPENDS ${dependencies} ${idl})

  add_custom_target(${name}
    DEPENDS ${output_path}/${header}  ${output_path}/${proxy} ${output_path}/${stub} ${dependencies})

endfunction()