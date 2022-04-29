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
          )

  set(multiValueArgs dependencies include_paths)

  #split out multivalue variables
  cmake_parse_arguments("params" "" "" "${multiValueArgs}" ${ARGN})          

  cmake_path(APPEND base_dir ${idl} OUTPUT_VARIABLE idl)
  message("idl1 ${idl}")



#[[  message("EnclaveMarshaller name ${name}")
  message("idl ${idl}")
  message("base_dir ${base_dir}")
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

  foreach (dep ${params_dependencies})
    get_target_property(dep_base_dir ${dep}_generate base_dir)
    if(dep_base_dir)
      message("dep_base_dir ${dep_base_dir}")
      set(PATHS_PARAMS ${PATHS_PARAMS} "--path" "\"${dep_base_dir}\"")
    endif()
  endforeach()  

  if(NOT ${namespace} STREQUAL "")
    set(PATHS_PARAMS "${PATHS_PARAMS} --namespace \"${namespace}\"")
  endif()

  message("
    add_custom_target(${name}_generate
    COMMAND ${ENCLAVE_MARSHALLER} 
      --idl ${idl} 
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
    DEPENDS ${idl} ${params_dependencies}
    BYPRODUCTS ${output_path}/${header}  ${output_path}/${proxy} ${output_path}/${stub})

    set_target_properties(${name}_generate PROPERTIES base_dir ${base_dir})

    add_library(${name} INTERFACE)
    target_include_directories(${name} 
      INTERFACE 
        \"$<BUILD_INTERFACE:${output_path}/include>\")    

    if(DEFINED params_dependencies)
      target_link_libraries(${name} INTERFACE ${params_dependencies})
    endif()  
    add_dependencies(${name} ${name}_generate)  
    ")


  add_custom_target(${name}_generate
    COMMAND ${ENCLAVE_MARSHALLER} 
      --idl ${idl} 
      --output_path ${output_path}
      --header ${header}
      --proxy ${proxy}
      --stub ${stub}
      ${PATHS_PARAMS}
    DEPENDS ${idl} ${params_dependencies}
    BYPRODUCTS ${output_path}/${header}  ${output_path}/${proxy} ${output_path}/${stub})

    set_target_properties(${name}_generate PROPERTIES base_dir ${base_dir})

    add_library(${name} INTERFACE)
    target_include_directories(${name} 
      INTERFACE 
        "$<BUILD_INTERFACE:${output_path}/include>")    

    if(DEFINED params_dependencies)
      target_link_libraries(${name} INTERFACE ${params_dependencies})
    endif()  
    add_dependencies(${name} ${name}_generate)    
endfunction()