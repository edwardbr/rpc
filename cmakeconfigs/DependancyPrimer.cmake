#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

if(NOT DEPENDANCIES_LOADED)
  # if this is loaded in a parent module then dont do this
  set(DEPENDANCIES_LOADED ON)

  option(BUILD_ENCLAVE "build enclave code" ON)
  option(BUILD_HOST "build host code" ON)
  option(BUILD_EXE "build exe code" ON)
  option(BUILD_DC_APPS "build dc apps" ON)
  option(BUILD_TEST "build test code" ON)
  option(BUILD_MEASUREMENT "measure enclave code" ON)
  option(DEBUG_ENCLAVE_MEMLEAK "detect memory leaks in enclaves" OFF)
  option(SECRETARIUM_UNITY_BUILD "enable unity build" OFF)

  option(CMAKE_VERBOSE_MAKEFILE "verbose build step" ON)
  option(CMAKE_RULE_MESSAGES "verbose cmake" ON)

  option(INCLUDE_DC_APPS_DLLS "include dc apps as dlls" ON)
  option(INCLUDE_DC_APPS_EMBEDDED "include dc apps in main enclave" ON)
  option(ENABLE_CLANG_TIDY "Enable clang-tidy in build" ON)
  option(ENABLE_CLANG_TIDY_FIX "Turn on auto fix in clang tidy" OFF)
  option(RPC_USE_LOGGING "Turn on logging" OFF)

  if(NOT DEFINED DC_APPS)
    set(DC_APPS "ALL_DCAPPS")
  endif()

  message("BUILD_TYPE ${BUILD_TYPE}")
  message("CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")
  message("SGX_MODE ${SGX_MODE}")
  message("SGX_HW ${SGX_HW}")
  message("SGX_KEY ${SGX_KEY}")
  message("RPC_BUILD_SGX ${RPC_BUILD_SGX}")
  message("RPC_BUILD_TEST ${RPC_BUILD_TEST}")
  message("RPC_USE_LOGGING ${RPC_USE_LOGGING}")

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

  # ############################################################################
  # reset and apply cmake separate enclave and host compile flags
  if(RPC_BUILD_TEST)
    set(BUILD_TEST_FLAG RPC_BUILD_TEST)
  else(RPC_BUILD_TEST)
    set(BUILD_TEST_FLAG)
  endif(RPC_BUILD_TEST)

  if(${RPC_BUILD_SGX} STREQUAL ON)
    if(WIN32)
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
    else(WIN32)
      # this is generally frowned apon as it forces all libraries to include this it is needed to force feed gtest with
      # the right c++ runtime perhaps moving this command to a sub project would be sensible
      include_directories(BEFORE SYSTEM "/usr/lib/llvm-10/include/c++/v1")

      set(DESTDIR ${CMAKE_BINARY_DIR}/tmp)
    endif(WIN32)
  endif()

  # ####################################################################################################################
  # settings
  if(CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
  else()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  endif()

  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  set(ENCLAVE_TARGET "SGX")

  include(GNUInstallDirs)

  if(${ENCLAVE_TARGET} STREQUAL "SGX")
    if(WIN32)
      set(WARNING_FLAG /W3 /WX)

      if(DEBUG_ENCLAVE_MEMLEAK)
        set(ENCLAVE_MEMLEAK_LINK_FLAGS /IGNORE:4006 /IGNORE:4088 /FORCE:MULTIPLE /WX:NO)
        set(ENCLAVE_MEMLEAK_DEFINES MEMLEAK_CHECK)
        set(DEFAULT_LOG_LEVEL "spdlog::level::trace")
      else()
        set(DEFAULT_LOG_LEVEL "spdlog::level::info")
      endif()

      if(RPC_SERIALISATION_TEXT)
        set(RPC_SERIALISATION_FMT RPC_SERIALISATION_TEXT)
      elseif(RPC_SERIALISATION_JSON)
        set(RPC_SERIALISATION_FMT RPC_SERIALISATION_JSON)
      else()
        set(RPC_SERIALISATION_FMT RPC_SERIALISATION_BINARY)
      endif()

      set(SHARED_DEFINES
        ENCLAVE_STATUS=sgx_status_t
        ENCLAVE_OK=SGX_SUCCESS
        _LIB
        NOMINMAX
        ${RPC_SERIALISATION_FMT}
        DEFAULT_LOG_LEVEL=${DEFAULT_LOG_LEVEL})
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

      set(SHARED_ENCLAVE_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS} /d2FH4- /Qspectre)

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
          set(HOST_DEFINES ${SHARED_HOST_DEFINES} NDEBUG)
        else()
          set(HOST_DEFINES ${SHARED_HOST_DEFINES} NDEBUG EDEBUG # sets SGX_DEBUG_FLAG to 1
          )
        endif()

        set(HOST_COMPILE_OPTIONS
          ${SHARED_HOST_COMPILE_OPTIONS}
          /GL
          /MD
          /O2
          /Oi
          /Ob2)

        if(${SGX_MODE} STREQUAL "release")
          set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG ${ENCLAVE_MEMLEAK_DEFINES})
        else()
          set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG EDEBUG # sets SGX_DEBUG_FLAG to 1
            ${ENCLAVE_MEMLEAK_DEFINES})
        endif()

        message("!!!!! /GL needs to be renabled for performance reasons")
        set(ENCLAVE_COMPILE_OPTIONS
          ${SHARED_ENCLAVE_COMPILE_OPTIONS}

          # /GL
          /MT
          /O2
          /Oi
          /Ob2)
        set(ENCLAVE_LINK_OPTIONS ${SHARED_ENCLAVE_LINK_OPTIONS} /INCREMENTAL:NO ${ENCLAVE_MEMLEAK_LINK_FLAGS} /debug)
        set(HOST_LINK_OPTIONS ${SHARED_HOST_LINK_OPTIONS} /INCREMENTAL:NO /debug)
        set(HOST_LINK_EXE_OPTIONS ${HOST_LINK_OPTIONS} /IGNORE:4099 /IGNORE:4098)
      else()
        set(HOST_DEFINES ${SHARED_HOST_DEFINES} _DEBUG FMT_HEADER_ONLY=1)

        set(HOST_COMPILE_OPTIONS
          ${SHARED_HOST_COMPILE_OPTIONS}
          /MDd
          /Od
          /Ob0
          /RTC1)

        set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} VERBOSE=2 # needed in some projects
          _DEBUG ${ENCLAVE_MEMLEAK_DEFINES})

        set(ENCLAVE_COMPILE_OPTIONS ${SHARED_ENCLAVE_COMPILE_OPTIONS} /MDd /Od /Ob0)

        set(ENCLAVE_LINK_OPTIONS
          ${SHARED_ENCLAVE_LINK_OPTIONS}
          /IGNORE:4099
          /IGNORE:4204
          /debug
          /INCREMENTAL:NO
          ${ENCLAVE_MEMLEAK_LINK_FLAGS})
        set(HOST_LINK_OPTIONS ${SHARED_HOST_LINK_OPTIONS} /debug /INCREMENTAL)
        set(HOST_LINK_EXE_OPTIONS
          ${HOST_LINK_OPTIONS}
          /IGNORE:4099
          /IGNORE:4098
          /IGNORE:4204
          /IGNORE:4203)
      endif()

      if(${SGX_HW}) # not simulation
        set(HOST_LIBRARIES
          ${SHARED_HOST_LIBRARIES}
          sgx_tcrypto.lib
          sgx_uae_service.lib
          sgx_capable.lib
          sgx_urts.lib)
      else()
        set(HOST_LIBRARIES
          ${SHARED_HOST_LIBRARIES}
          sgx_tcrypto.lib
          sgx_uae_service_sim.lib
          sgx_capable.lib
          sgx_urts_sim.lib)
      endif()

      set(ENCLAVE_SSL_INCLUDES "C:/Dev/Libs/VS2015/x64/openssl-1.1.1/include")

    else()
      find_program(CLANG_TIDY_EXE NAMES "clang-tidy" REQUIRED)

      # setup clang-tidy command from executable + options
      if(ENABLE_CLANG_TIDY_FIX)
        set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" -fix-errors -fix)
      else()
        set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
      endif()

      set(HOST_DEFINES ENCLAVE_STATUS=sgx_status_t ENCLAVE_OK=SGX_SUCCESS DISALLOW_BAD_JUMPS)
      set(ENCLAVE_DEFINES
        _IN_ENCLAVE
        CLEAN_LIBC
        ENCLAVE_STATUS=sgx_status_t
        ENCLAVE_OK=SGX_SUCCESS
        DISALLOW_BAD_JUMPS)
      set(ENCLAVE_COMPILE_OPTIONS
        -Wno-c++17-extensions
        -Wno-nonportable-include-path
        -Wno-conversion
        -Wno-unused-parameter
        -Wno-tautological-undefined-compare
        -Wno-dynamic-class-memaccess
        -Wno-ignored-qualifiers
        -Wno-exceptions
        -Wno-null-dereference
        -Wno-ignored-attributes
        -Wno-implicit-exception-spec-mismatch
        -I/usr/lib/llvm-10/include/c++/v1)
      set(HOST_COMPILE_OPTIONS
        -I/usr/lib/llvm-10/include/c++/v1
        -Wno-nonportable-include-path
        -Wno-conversion
        -Wno-unused-parameter
        -Wno-tautological-undefined-compare
        -Wno-dynamic-class-memaccess
        -Wno-ignored-qualifiers
        -Wno-exceptions
        -Wno-null-dereference
        -Wno-ignored-attributes
        -Wno-implicit-exception-spec-mismatch
        -Wno-trigraphs)
      set(HOST_LINK_OPTIONS
        -L/usr/lib/llvm-10/lib
        -Wl,-rpath,/usr/lib/llvm-10/lib
        -lc++
        -L/opt/intel/sgxsdk/lib64
        -lsgx_tcrypto
        linux_dependancies_host)
      if(${SGX_HW}) # not simulation
        set(HOST_LIBRARIES
          ${SHARED_HOST_LIBRARIES}
          sgx_tcrypto
          sgx_uae_service
          sgx_capable
          sgx_urts)
      else()
        set(HOST_LIBRARIES
          ${SHARED_HOST_LIBRARIES}
          sgx_tcrypto
          sgx_uae_service_sim
          sgx_capable
          sgx_urts_sim)
      endif()        
      set(OS_DEPENDANCIES_ENCLAVE linux_dependancies_enclave)
      set(ENCLAVE_SSL_INCLUDES "/opt/intel/sgxssl/include")
    endif()

    find_package(SGX REQUIRED)

    set(HOST_INCLUDES ${SGX_INCLUDE_DIR})
    set(ENCLAVE_LIBC_INCLUDES ${SGX_INCLUDE_DIR} ${SGX_TLIBC_INCLUDE_DIR})
    set(ENCLAVE_LIBCXX_INCLUDES ${ENCLAVE_LIBC_INCLUDES} ${SGX_LIBCXX_INCLUDE_DIR} ${SGX_LIBSTDCXX_INCLUDE_DIR})
  elseif(${ENCLAVE_TARGET} STREQUAL "OPEN_ENCLAVE")
    # remove sgx when fixed
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/submodules/SGX-CMake/cmake)
    find_package(SGX REQUIRED)
    set(ENCLAVE_COMPILE_OPTIONS)
    set(ENCLAVE_DEFINES
      OE_API_VERSION=2
      _IN_ENCLAVE
      _IN_OE
      CLEAN_LIBC
      ENCLAVE_STATUS=oe_result_t
      ENCLAVE_OK=OE_OK
      DISALLOW_BAD_JUMPS)
    set(ENCLAVE_LIBRARIES openenclave::oeenclave oecrypto::oecrypto openenclave::oelibc openenclave::oelibcxx)
  else()
    message(FATAL_ERROR "Invalid ENCLAVE_TARGET value")
  endif()

  message("HOST_DEFINES ${HOST_DEFINES}")
  message("HOST_COMPILE_OPTIONS ${HOST_COMPILE_OPTIONS}")
  message("ENCLAVE_DEFINES ${ENCLAVE_DEFINES}")
  message("ENCLAVE_COMPILE_OPTIONS ${ENCLAVE_COMPILE_OPTIONS}")

  # create vc140.pdb so that we do not have nasty projects not making their own pdb file
  if(WIN32)
    set(PDB_PATH ${CMAKE_BINARY_DIR}/output/${BUILD_TYPE})
    execute_process(COMMAND "cmd.exe" "/C" "mkdir ${PDB_PATH}")
    message("ignore Access is denied message if on next line")
    execute_process(COMMAND "cmd.exe" "/C" "copy nul vc140.pdb" WORKING_DIRECTORY "${PDB_PATH}")
    execute_process(COMMAND "cmd.exe" "/C" "attrib +R vc140.pdb" WORKING_DIRECTORY "${PDB_PATH}")
  endif()

  # ####################################################################################################################
  set(CMAKE_DEBUG_POSTFIX
    ""
    CACHE STRING "Adds a postfix for debug-built libraries." FORCE)

  # enable testing
  if(BUILD_TEST)
    include(CTest)
    include(GoogleTest)
  endif()
endif(NOT DEPENDANCIES_LOADED)
