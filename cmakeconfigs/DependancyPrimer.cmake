# formatted using cmake-format
cmake_minimum_required(VERSION 3.12)

if(NOT DEPENDANCIES_LOADED)
  # if this is loaded in a parent module then dont do this
  set(DEPENDANCIES_LOADED ON)


  message("BUILD_TYPE ${BUILD_TYPE}")
  message("CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")
  message("SGX_MODE ${SGX_MODE}")
  message("SGX_HW ${SGX_HW}")
  message("SGX_KEY ${SGX_KEY}")
  message("RPC_BUILD_SGX ${RPC_BUILD_SGX}")
  message("RPC_BUILD_TEST ${RPC_BUILD_TEST}")
  message("RPC_USE_LOGGING ${RPC_USE_LOGGING}")

  if(${RPC_BUILD_SGX} STREQUAL ON)
    set(RPC_BUILD_SGX_FLAG RPC_BUILD_SGX)
  else()
    set(RPC_BUILD_SGX_FLAG)
  endif()

  set(WARNING_LEVEL 3)
  set(WARNINGS_AS_ERRORS ON)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(ENCLAVE_TARGET "SGX")

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

  if(CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY
      ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
  else()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  endif()

  # ############################################################################
  # reset and apply cmake separate enclave and host compile flags
  if(RPC_BUILD_TEST)
    set(BUILD_TEST_FLAG RPC_BUILD_TEST)
  else()
    set(BUILD_TEST_FLAG)
  endif()

  if(${RPC_BUILD_SGX} STREQUAL ON)
    if(WIN32) # Windows
      # we dont want os specific libraries for our enclaves
      set(CMAKE_C_STANDARD_LIBRARIES
        ""
        CACHE STRING "override default windows libraries" FORCE)
      set(CMAKE_CXX_STANDARD_LIBRARIES
        ""
        CACHE STRING "override default windows libraries" FORCE)
      set(CMAKE_CXX_FLAGS
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_C_FLAGS
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_CXX_FLAGS_DEBUG
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_C_FLAGS_DEBUG
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_CXX_FLAGS_RELEASE
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_C_FLAGS_RELEASE
        ""
        CACHE STRING "override default windows flags" FORCE)

      set(CMAKE_MODULE_LINKER_FLAGS
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_MODULE_LINKER_FLAGS_DEBUG
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_MODULE_LINKER_FLAGS_RELEASE
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_SHARED_LINKER_FLAGS
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_SHARED_LINKER_FLAGS_DEBUG
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_EXE_LINKER_FLAGS
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_EXE_LINKER_FLAGS_DEBUG
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_EXE_LINKER_FLAGS_RELEASE
        ""
        CACHE STRING "override default windows flags" FORCE)
      set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
        ""
        CACHE STRING "override default windows flags" FORCE)

      # https://www.youtube.com/watch?v=JuZPu4yJA7k
      set(CMAKE_INSTALL_PREFIX
        ${CMAKE_BINARY_DIR}/install
        CACHE STRING "override default windows flags" FORCE)

      set(WARNING_LEVEL 3)
      set(WARNINGS_AS_ERRORS ON)

      if(WARNINGS_AS_ERRORS)
        set(WARNING_FLAG /W${WARNING_LEVEL} /WX)
      else(WARNINGS_AS_ERRORS)
        set(WARNING_FLAG /W${WARNING_LEVEL} /WX-)
      endif(WARNINGS_AS_ERRORS)

      set(SHARED_DEFINES
        ENCLAVE_STATUS=sgx_status_t
        ENCLAVE_OK=SGX_SUCCESS
        _LIB
        NOMINMAX
        ${BUILD_TEST_FLAG}
        ${RPC_USE_LOGGING_FLAG}
        ${RPC_BUILD_SGX_FLAG})
      set(SHARED_HOST_DEFINES ${SHARED_DEFINES} WIN32 _WINDOWS)
      set(SHARED_ENCLAVE_DEFINES ${SHARED_DEFINES} _IN_ENCLAVE)

      set(SHARED_COMPILE_OPTIONS
        ${WARNING_FLAG}
        /bigobj
        /diagnostics:classic
        /EHsc
        /errorReport:prompt
        /FC

        # /FS
        /fp:precise

        # /guard:cf
        /Gd
        /Gy

        # /JMC
        /MP
        /nologo
        /sdl
        /Zc:forScope
        /Zc:inline
        /Zc:rvalueCast
        /Zc:wchar_t
        /Zi
        /wd4996 # allow deprecated functions
      )

      set(SHARED_HOST_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS})

      set(SHARED_ENCLAVE_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS} /d2FH4-
        /Qspectre)

      set(LINK_OPTIONS /machine:x64 /WX)

      set(SHARED_ENCLAVE_LINK_OPTIONS ${LINK_OPTIONS})
      set(SHARED_HOST_LINK_OPTIONS ${LINK_OPTIONS})

      set(SHARED_HOST_LIBRARIES
        advapi32.lib
        comdlg32.lib
        gdi32.lib
        kernel32.lib
        ole32.lib
        oleaut32.lib
        shell32.lib
        user32.lib
        uuid.lib
        winspool.lib
        Crypt32.lib)

      if(${BUILD_TYPE} STREQUAL "release")
        if(${SGX_MODE} STREQUAL "release")
          set(HOST_DEFINES ${SHARED_HOST_DEFINES} NDEBUG
            PREVENT_DEBUG_ENCLAVE # what is this?
          )
        else()
          set(HOST_DEFINES
            ${SHARED_HOST_DEFINES} NDEBUG PREVENT_DEBUG_ENCLAVE # what is

            # this?
            EDEBUG # sets SGX_DEBUG_FLAG to 1
          )
        endif()

        set(HOST_COMPILE_OPTIONS ${SHARED_HOST_COMPILE_OPTIONS} /GL /MD /O2 /Oi
          /Ob2)

        if(${SGX_MODE} STREQUAL "release")
          set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG
            ${ENCLAVE_MEMLEAK_DEFINES})
        else()
          set(ENCLAVE_DEFINES
            ${SHARED_ENCLAVE_DEFINES} NDEBUG
            EDEBUG # sets SGX_DEBUG_FLAG to 1
            ${ENCLAVE_MEMLEAK_DEFINES})
        endif()

        message("!!!!! /GL needs to be renabled for performance reasons")
        set(ENCLAVE_COMPILE_OPTIONS
          ${SHARED_ENCLAVE_COMPILE_OPTIONS}

          # /GL
          /MT /O2 /Oi /Ob2)
        set(ENCLAVE_LINK_OPTIONS ${SHARED_ENCLAVE_LINK_OPTIONS} /INCREMENTAL:NO
          ${ENCLAVE_MEMLEAK_LINK_FLAGS} /debug)
        set(HOST_LINK_OPTIONS ${SHARED_HOST_LINK_OPTIONS} /INCREMENTAL:NO
          /debug)
        set(HOST_LINK_EXE_OPTIONS ${HOST_LINK_OPTIONS} /IGNORE:4099
          /IGNORE:4098)
      else()
        set(HOST_DEFINES ${SHARED_HOST_DEFINES} _DEBUG)

        set(HOST_COMPILE_OPTIONS ${SHARED_HOST_COMPILE_OPTIONS} /MDd /Od /Ob0
          /RTC1)

        set(ENCLAVE_DEFINES
          ${SHARED_ENCLAVE_DEFINES} VERBOSE=2 # needed in some projects
          _DEBUG ${ENCLAVE_MEMLEAK_DEFINES})

        set(ENCLAVE_COMPILE_OPTIONS ${SHARED_ENCLAVE_COMPILE_OPTIONS} /MDd /Od
          /Ob0)

        set(ENCLAVE_LINK_OPTIONS
          ${SHARED_ENCLAVE_LINK_OPTIONS} /IGNORE:4099 /IGNORE:4204 /debug
          /INCREMENTAL:NO ${ENCLAVE_MEMLEAK_LINK_FLAGS})
        set(HOST_LINK_OPTIONS ${SHARED_HOST_LINK_OPTIONS} /debug /INCREMENTAL)
        set(HOST_LINK_EXE_OPTIONS ${HOST_LINK_OPTIONS} /IGNORE:4099
          /IGNORE:4098 /IGNORE:4204 /IGNORE:4203)
      endif()

      if(${SGX_HW}) # not simulation
        set(HOST_LIBRARIES ${SHARED_HOST_LIBRARIES} sgx_tcrypto.lib
          sgx_uae_service.lib sgx_capable.lib sgx_urts.lib)
      else()
        set(HOST_LIBRARIES
          ${SHARED_HOST_LIBRARIES} sgx_tcrypto.lib sgx_uae_service_sim.lib
          sgx_capable.lib sgx_urts_sim.lib)
      endif()

    else() # Linux
      # Review the below(*) if it's necessary to be re-enabled again: this needs
      # a complete rewrite *include(GNUInstallDirs)

      # this is generally frowned apon as it forces all libraries to include
      # this it is needed to force feed gtest with the right c++ runtime perhaps
      # moving this command to a sub project would be sensible
      # *include_directories(BEFORE SYSTEM "/usr/lib/llvm-10/include/c++/v1")

      # *set(DESTDIR ${CMAKE_BINARY_DIR}/tmp)

      message("SGX_SDK_CONTAINS_DEBUG_INFORMATION is ${SGX_SDK_CONTAINS_DEBUG_INFORMATION}")
      set(SHARED_DEFINES
        ENCLAVE_STATUS=sgx_status_t
        ENCLAVE_OK=SGX_SUCCESS
        _LIB
        NOMINMAX
        DEFAULT_LOG_LEVEL=spdlog::level::info
        ${BUILD_TEST_FLAG}
        ${RPC_BUILD_SGX_FLAG})

      if(${BUILD_TYPE} STREQUAL "release")
        if(${SGX_MODE} STREQUAL "release")
          set(HOST_DEFINES ${SHARED_DEFINES} NDEBUG
            PREVENT_DEBUG_ENCLAVE # what is this?
            DISALLOW_BAD_JUMPS
          )
        else() # Prerelease "possibly"
          set(HOST_DEFINES
            ${SHARED_DEFINES} NDEBUG PREVENT_DEBUG_ENCLAVE # what is this?
            DISALLOW_BAD_JUMPS
            EDEBUG # sets SGX_DEBUG_FLAG to 1
          )
        endif()
      else() # debug
        set(HOST_DEFINES
          ${SHARED_DEFINES} _DEBUG
        )
      endif()

      set(SHARED_ENCLAVE_DEFINES
        _IN_ENCLAVE
        ${SHARED_DEFINES}
        CLEAN_LIBC
        ENCLAVE_STATUS=sgx_status_t
        ENCLAVE_OK=SGX_SUCCESS
        DISALLOW_BAD_JUMPS)

      if(${BUILD_TYPE} STREQUAL "release")
        set(CMAKE_CXX_FLAGS_DEBUG "")

        if(FORCE_DEBUG_INFORMATION)
          set(EXTRA_COMPILE_OPTIONS -g -fno-limit-debug-info)
        endif()

        set(OPTIMIZER_FLAGS -O3)

        if(${SGX_MODE} STREQUAL "release")
          set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG
            ${ENCLAVE_MEMLEAK_DEFINES})
        else() # Prerelease "possibly"
          set(ENCLAVE_DEFINES
            ${SHARED_ENCLAVE_DEFINES}
            NDEBUG
            EDEBUG) # sets SGX_DEBUG_FLAG to 1
        endif()
      else() # debug
        set(CMAKE_CXX_FLAGS_DEBUG
          "-g -fno-limit-debug-info") # -g -fno-limit-debug-info

        # set(EXTRA_COMPILE_OPTIONS -g -fno-limit-debug-info) # The one actually used by HOST and ENCLAVE instead of CMAKE_CXX_FLAGS_DEBUG
        set(OPTIMIZER_FLAGS -O0)
        set(ENCLAVE_DEFINES
          ${SHARED_ENCLAVE_DEFINES}
          _DEBUG
          EDEBUG) # sets SGX_DEBUG_FLAG to 1
      endif()

      message("CMAKE_CXX_FLAGS_DEBUG [${CMAKE_CXX_FLAGS_DEBUG}]")
      message("OPTIMIZER_FLAGS [${OPTIMIZER_FLAGS}]")

      set(SHARED_COMPILE_OPTIONS
        -Wno-conversion
        -Wno-exceptions
        -Wno-ignored-attributes
        -Wno-ignored-qualifiers
        -Wno-implicit-exception-spec-mismatch
        -Wno-nonportable-include-path
        -Wno-null-dereference
        -Wno-tautological-undefined-compare
        -Wno-unused-parameter
        -Wno-c++20-extensions

        # -I/usr/include/c++/9
        ${EXTRA_COMPILE_OPTIONS}
        -fPIC)
      set(ENCLAVE_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS}
        -Wno-c++17-extensions
        -ffunction-sections
        -fdata-sections)
      set(HOST_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS} -Wno-trigraphs ${OPTIMIZER_FLAGS})
      message("HOST_COMPILE_OPTIONS [${HOST_COMPILE_OPTIONS}] ${CMAKE_CXX_FLAGS_DEBUG}")
      set(HOST_LINK_OPTIONS -lc++ -L/opt/intel/sgxsdk/lib64 -lsgx_tcrypto)
    endif()

    find_package(SGX REQUIRED)

    # need for SGX_SDK_CONTAINS_DEBUG_INFORMATION has just been determined in FindSGX.cmake
    if(SGX_SDK_CONTAINS_DEBUG_INFORMATION)
      list(APPEND ENCLAVE_DEFINES SGX_SDK_CONTAINS_DEBUG_INFORMATION)
    endif()

    set(HOST_INCLUDES ${SGX_INCLUDE_DIR})
    set(ENCLAVE_LIBC_INCLUDES ${SGX_INCLUDE_DIR} ${SGX_TLIBC_INCLUDE_DIR})
    set(ENCLAVE_LIBCXX_INCLUDES
      ${ENCLAVE_LIBC_INCLUDES} ${SGX_LIBCXX_INCLUDE_DIR}
      ${SGX_LIBSTDCXX_INCLUDE_DIR})

    set(HOST_LIBRARIES ${SHARED_HOST_LIBRARIES} sgx_tcrypto
      ${SGX_USVC_LIB} sgx_capable ${SGX_URTS_LIB})

  else()
    if(WIN32) # Windows
      set(WARNING_LEVEL 3)
      set(WARNINGS_AS_ERRORS ON)

      if(WARNINGS_AS_ERRORS)
        set(WARNING_FLAG /W${WARNING_LEVEL} /WX)
      else(WARNINGS_AS_ERRORS)
        set(WARNING_FLAG /W${WARNING_LEVEL} /WX-)
      endif(WARNINGS_AS_ERRORS)

      set(SHARED_DEFINES
        ${BUILD_TEST_FLAG}
        ${RPC_USE_LOGGING_FLAG}
        ${RPC_BUILD_SGX_FLAG})

      set(SHARED_COMPILE_OPTIONS
        ${WARNING_FLAG}
        /bigobj
      )

      set(SHARED_HOST_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS})

      set(SHARED_ENCLAVE_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS} )

      set(SHARED_ENCLAVE_LINK_OPTIONS ${LINK_OPTIONS})
      set(SHARED_HOST_LINK_OPTIONS ${LINK_OPTIONS})
    else()
    endif()
  endif()

  # ############################################################################
  # enable testing
  if(RPC_BUILD_TEST)
    include(GoogleTest)
    enable_testing()
  endif()
endif(NOT DEPENDANCIES_LOADED)
