function(EnclaveMarshaller 
          name
          idl
          output_path
          header
          proxy
          stub
          namespace
          #multivalue expects string "dependencies"
          #multivalue expects string "include_paths"
          )

  set(multiValueArgs dependencies include_paths)


  #split out multivalue variables
  cmake_parse_arguments("params" "" "" "${multiValueArgs}" ${ARGN})          

#[[  message("EnclaveMarshaller name ${name}")
  message("idl ${idl}")
  message("output_path ${output_path}")
  message("header ${header}")
  message("proxy ${proxy}")
  message("stub ${stub}")
  message("namespace ${namespace}")
  message("dependencies ${params_dependencies}")
  message("paths ${params_include_paths}")]]

  # Test if including from FindFlatBuffers
  if(EXISTS ENCLAVE_MARSHALLER_EXECUTABLE)
    set(ENCLAVE_MARSHALLER ${ENCLAVE_MARSHALLER_EXECUTABLE})
  else()
    set(ENCLAVE_MARSHALLER in_zone_synchronous_generator)
  endif()

  set(PATHS_PARAMS)
  foreach (path ${params_include_paths})
    set(PATHS_PARAMS ${PATHS_PARAMS} "--path" "\"${path}\"")
  endforeach()

  if(NOT ${namespace} STREQUAL "")
    set(PATHS_PARAMS "${PATHS_PARAMS} --namespace \"${namespace}\"")
  endif()

  message("PATHS_PARAMS ${PATHS_PARAMS}")


  #[[message("add_custom_target(${name}
    COMMAND ${ENCLAVE_MARSHALLER} 
      --idl ${idl} 
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
    DEPENDS ${idl}
    BYPRODUCTS ${output_path}/${header}  ${output_path}/${proxy} ${output_path}/${stub})
    
    add_dependencies(${name} ${params_dependencies})    
    ")]]

    add_custom_target(${name}_generate
    COMMAND ${ENCLAVE_MARSHALLER} 
      --idl ${idl} 
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
    DEPENDS ${idl}
    BYPRODUCTS ${output_path}/${header}  ${output_path}/${proxy} ${output_path}/${stub})


    add_library(${name} INTERFACE)
    target_include_directories(${name} 
      INTERFACE 
        "$<BUILD_INTERFACE:${output_path}/include")    

    if(DEFINED params_dependencies)
      target_link_libraries(${name} INTERFACE ${params_dependencies})
    endif()  
    add_dependencies(${name} ${name}_generate)    
endfunction()