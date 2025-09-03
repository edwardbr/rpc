# formatted using cmake-format
cmake_minimum_required(VERSION 3.24)

message("DEPENDENCIES_LOADED ${DEPENDENCIES_LOADED}")

if(NOT DEPENDENCIES_LOADED)
  message("configuring dependencies")

  # if this is loaded in a parent module then dont do this
  set(DEPENDENCIES_LOADED ON)

  # ####################################################################################################################
  # settings
  option(BUILD_ENCLAVE "build enclave code" ON)
  option(BUILD_HOST "build host code" ON)
  option(BUILD_EXE "build exe code" ON)
  option(BUILD_TEST "build test code" ON)
  option(DEBUG_HOST_LEAK "enable leak sanitizer (only use when ptrace is accessible)" OFF)
  option(DEBUG_HOST_ADDRESS "enable address sanitizer" OFF)
  option(DEBUG_HOST_THREAD "enable thread sanitizer (cannot be used with leak sanitizer)" OFF)
  option(DEBUG_HOST_UNDEFINED "enable undefined behaviour sanitizer" OFF)
  option(DEBUG_HOST_ALL "enable all sanitizers" OFF)
  option(DEBUG_ENCLAVE_MEMLEAK "detect memory leaks in enclaves" OFF)
  option(ENABLE_CLANG_TIDY "Enable clang-tidy in build (needs to build with clang)" OFF)
  option(ENABLE_CLANG_TIDY_FIX "Turn on auto fix in clang tidy" OFF)
  option(ENABLE_COVERAGE "Turn on code coverage" OFF)

  option(CMAKE_VERBOSE_MAKEFILE "verbose build step" OFF)
  option(CMAKE_RULE_MESSAGES "verbose cmake" OFF)

  option(FORCE_DEBUG_INFORMATION "force inclusion of debug information" ON)

  option(USE_RPC_LOGGING "turn on rpc logging" OFF)
  option(RPC_HANG_ON_FAILED_ASSERT "hang on failed assert" OFF)
  option(USE_RPC_TELEMETRY "turn on rpc telemetry" OFF)
  option(USE_CONSOLE_TELEMETRY "turn on rpc telemetry" OFF)  
  option(USE_RPC_TELEMETRY_RAII_LOGGING
    "turn on the logging of the addref release and try cast activity of the services, proxies and stubs" OFF)

  if(NOT DEFINED RPC_OUT_BUFFER_SIZE)
    # setting RPC_OUT_BUFFER_SIZE to 4kb which is the default page size for windows and linux
    set(RPC_OUT_BUFFER_SIZE 0x1000)
  endif()

  message("BUILD_TYPE ${BUILD_TYPE}")
  message("CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")
  message("SGX_MODE ${SGX_MODE}")
  message("SGX_HW ${SGX_HW}")
  message("SGX_KEY ${SGX_KEY}")

  message("BUILD_ENCLAVE ${BUILD_ENCLAVE}")
  message("BUILD_HOST ${BUILD_HOST}")
  message("BUILD_EXE  ${BUILD_EXE}")
  message("BUILD_TEST ${BUILD_TEST}")

  message("AWAIT_ATTACH_ON_ENCLAVE_ERRORS  ${AWAIT_ATTACH_ON_ENCLAVE_ERRORS}")
  message("DEBUG_ENCLAVE_MEMLEAK  ${DEBUG_ENCLAVE_MEMLEAK}")

  message("CMAKE_VERBOSE_MAKEFILE  ${CMAKE_VERBOSE_MAKEFILE}")
  message("CMAKE_RULE_MESSAGES  ${CMAKE_RULE_MESSAGES}")

  message("ENABLE_CLANG_TIDY  ${ENABLE_CLANG_TIDY}")
  message("ENABLE_CLANG_TIDY_FIX  ${ENABLE_CLANG_TIDY_FIX}")

  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(ENCLAVE_TARGET "SGX")

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

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

  # https://www.youtube.com/watch?v=JuZPu4yJA7k
  message(INSTALL_LOCATION ${INSTALL_LOCATION})

  if(DEFINED INSTALL_LOCATION)
    message(CMAKE_INSTALL_PREFIX ${INSTALL_LOCATION})
    set(CMAKE_INSTALL_PREFIX
      ${INSTALL_LOCATION}
      CACHE STRING "override location of installation files" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX
      ${CMAKE_BINARY_DIR}/install
      CACHE STRING "override location of installation files" FORCE)
  endif()

  # ####################################################################################################################
  # load the submodules
  find_package(Git QUIET)
  message("submodules GIT_FOUND ${GIT_FOUND}")

  if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    # Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    message("doing GIT_SUBMODULE ${GIT_SUBMODULE}")

    if(GIT_SUBMODULE)
      message(STATUS "Submodule init")
      execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule init
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_SUBMOD_INIT_RESULT)

      if(NOT GIT_SUBMOD_INIT_RESULT EQUAL "0")
        message(FATAL_ERROR "submodule init")
      endif()

      message(STATUS "Submodule update")
      execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_SUBMOD_RESULT)

      if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
      endif()
    endif()
  endif()

  # ####################################################################################################################
  # reset and apply cmake separate enclave and host compile flags
  if(BUILD_TEST)
    set(BUILD_TEST_FLAG BUILD_TEST)
  else()
    set(BUILD_TEST_FLAG)
  endif()

  if(USE_RPC_LOGGING)
    set(USE_RPC_LOGGING_FLAG USE_RPC_LOGGING)
  else()
    set(USE_RPC_LOGGING_FLAG)
  endif()
  
  if(RPC_HANG_ON_FAILED_ASSERT)
    set(RPC_HANG_ON_FAILED_ASSERT_FLAG RPC_HANG_ON_FAILED_ASSERT)
  else()
    set(RPC_HANG_ON_FAILED_ASSERT_FLAG)
  endif()

  if(USE_RPC_TELEMETRY)
    set(USE_RPC_TELEMETRY_FLAG USE_RPC_TELEMETRY)
  else()
    set(USE_RPC_TELEMETRY_FLAG)
  endif()

  if(USE_CONSOLE_TELEMETRY)
    set(USE_CONSOLE_TELEMETRY_FLAG USE_CONSOLE_TELEMETRY)
  else()
    set(USE_CONSOLE_TELEMETRY_FLAG)
  endif()
  
  if(USE_RPC_TELEMETRY_RAII_LOGGING)
    set(USE_RPC_TELEMETRY_RAII_LOGGING_FLAG USE_RPC_TELEMETRY_RAII_LOGGING)
  else()
    set(USE_RPC_TELEMETRY_RAII_LOGGING_FLAG)
  endif()

  set(RPC_ENCLAVE_FMT_LIB)
  set(RPC_HOST_FMT_LIB)
  if(USE_RPC_LOGGING)
    set(RPC_HOST_FMT_LIB fmt::fmt)
  endif()
  
  if(BUILD_ENCLAVE)
    set(BUILD_ENCLAVE_FLAG BUILD_ENCLAVE)
    if(USE_RPC_LOGGING)
      set(RPC_ENCLAVE_FMT_LIB fmt::fmt-header-only)
    endif()
  else()
    set(BUILD_ENCLAVE_FLAG)
  endif()
  
  if(${ENCLAVE_TARGET} STREQUAL "SGX")
    if(${SGX_HW}) # not simulation
      set(SGX_HW_OR_SIM_DEFINE SGX_HW)
    else()
      set(SGX_HW_OR_SIM_DEFINE SGX_SIM)
    endif()

    set(SHARED_DEFINES
      ENCLAVE_STATUS=sgx_status_t
      ENCLAVE_OK=SGX_SUCCESS
      _LIB
      NOMINMAX
      _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
      ${USE_RPC_LOGGING_FLAG}
      ${RPC_HANG_ON_FAILED_ASSERT_FLAG}
      ${USE_RPC_TELEMETRY_FLAG}
      ${USE_CONSOLE_TELEMETRY_FLAG}
      ${USE_RPC_TELEMETRY_RAII_LOGGING_FLAG}
      ${BUILD_ENCLAVE_FLAG}
      ${BUILD_TEST_FLAG}
      ${ENCLAVE_MEMLEAK_DEFINES}
      ${ENABLE_EXTERNAL_VERIFICATION_FLAG}
      ${SGX_HW_OR_SIM_DEFINE}
      RPC_OUT_BUFFER_SIZE=${RPC_OUT_BUFFER_SIZE})

    if(WIN32) # Windows
      if(BUILD_ENCLAVE)
        find_package(SGX REQUIRED)
      endif()

      # we dont want os specific libraries for our enclaves
      set(CMAKE_C_STANDARD_LIBRARIES
        ""
        CACHE STRING "override default windows libraries" FORCE)
      set(CMAKE_C_COMPILE_OBJECT
        ""
        CACHE STRING "override default windows libraries" FORCE)
      set(CMAKE_C_ARCHIVE_FINISH
        ""
        CACHE STRING "override default windows libraries" FORCE)
      set(CMAKE_C_ARCHIVE_CREATE
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

      set(WARNING_FLAG /W3 /WX /w34265 /wd4996)

      if(DEBUG_ENCLAVE_MEMLEAK)
        set(ENCLAVE_MEMLEAK_LINK_FLAGS /IGNORE:4006 /IGNORE:4088 /FORCE:MULTIPLE /WX:NO)
        set(ENCLAVE_MEMLEAK_DEFINES MEMLEAK_CHECK)
      endif()

      cmake_path(SET TEMP_DIR NORMALIZE "c:/temp/")
      cmake_path(SET RUNTIME_DIR NORMALIZE "c:/Secretarium/Runtime/")

      set(SHARED_DEFINES ${SHARED_DEFINES} TEMP_DIR="${TEMP_DIR}" RUNTIME_DIR="${RUNTIME_DIR}")

      # to fill out later
      set(WARN_PEDANTIC)
      set(WARN_OK)

      # the are conflicts between winsock.h and winsock2.h, the latter being the preferred one (and sometimes actually
      # required), but the former getting included through windows.h for instance, unless winsock2.h has already been
      # included OR WIN32_LEAN_AND_MEAN is defined, which is therefore a good solution to handle the problem everywhere
      set(SHARED_HOST_DEFINES ${SHARED_DEFINES} WIN32 _WINDOWS WIN32_LEAN_AND_MEAN)

      if(BUILD_ENCLAVE)
        set(SHARED_ENCLAVE_DEFINES ${SHARED_DEFINES} _IN_ENCLAVE ${ENCLAVE_MEMLEAK_DEFINES})
      endif()

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
        /Zc:__cplusplus
        /Zi
        /wd4996 # allow deprecated functions
      )

      set(HOST_DEBUG_OPTIONS)

      set(SHARED_HOST_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS} ${HOST_DEBUG_OPTIONS})

      set(SHARED_ENCLAVE_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS} /d2FH4- /Qspectre)

      set(LINK_OPTIONS /MACHINE:x64 /WX)

      set(SHARED_ENCLAVE_LINK_OPTIONS ${LINK_OPTIONS})
      set(SHARED_HOST_LINK_OPTIONS ${LINK_OPTIONS} ${HOST_DEBUG_OPTIONS})

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
        Crypt32.lib

        # ${DCAP_LIBRARY_DIR}/sgx_dcap_quoteverify.lib ${DCAP_LIBRARY_DIR}/sgx_dcap_ql.lib
      )

      if(${BUILD_TYPE} STREQUAL "release")
        if(${SGX_MODE} STREQUAL "release")
          set(HOST_DEFINES ${SHARED_HOST_DEFINES} NDEBUG PREVENT_DEBUG_ENCLAVE # what is this?
          )
        else()
          set(HOST_DEFINES ${SHARED_HOST_DEFINES} NDEBUG PREVENT_DEBUG_ENCLAVE # what is this?
            EDEBUG # sets SGX_DEBUG_FLAG to 1
          )
        endif()

        set(HOST_COMPILE_OPTIONS
          ${SHARED_HOST_COMPILE_OPTIONS}
          /GL
          /MD
          /O2
          /Oi
          /Ob2)

        if(BUILD_ENCLAVE)
          if(${SGX_MODE} STREQUAL "release")
            set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG)
          else()
            set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG EDEBUG # sets SGX_DEBUG_FLAG to 1
            )
          endif()
        endif()

        if(BUILD_ENCLAVE)
          message("!!!!! /GL needs to be renabled for performance reasons")
          set(ENCLAVE_COMPILE_OPTIONS
            ${SHARED_ENCLAVE_COMPILE_OPTIONS}

            # /GL
            /MT
            /O2
            /Oi
            /Ob2)
          set(ENCLAVE_LINK_OPTIONS ${SHARED_ENCLAVE_LINK_OPTIONS} /INCREMENTAL:NO ${ENCLAVE_MEMLEAK_LINK_FLAGS} /DEBUG)
        endif()

        set(HOST_LINK_OPTIONS ${SHARED_HOST_LINK_OPTIONS} /INCREMENTAL:NO /DEBUG)
        set(HOST_LINK_DYNAMIC_LIBRARY_OPTIONS ${HOST_LINK_OPTIONS})
        set(HOST_LINK_EXE_OPTIONS ${HOST_LINK_OPTIONS} /IGNORE:4099 /IGNORE:4098)
      else()
        set(HOST_DEFINES ${SHARED_HOST_DEFINES} _DEBUG)

        set(HOST_COMPILE_OPTIONS
          ${SHARED_HOST_COMPILE_OPTIONS}
          /MDd
          /Od
          /Ob0
          /RTC1)

        if(BUILD_ENCLAVE)
          set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} VERBOSE=2 # needed in some projects
            _DEBUG ${ENCLAVE_MEMLEAK_DEFINES})

          set(ENCLAVE_COMPILE_OPTIONS ${SHARED_ENCLAVE_COMPILE_OPTIONS} /MDd /Od /Ob0)

          set(ENCLAVE_LINK_OPTIONS
            ${SHARED_ENCLAVE_LINK_OPTIONS}
            /IGNORE:4099
            /IGNORE:4204
            /IGNORE:4217
            /DEBUG
            /INCREMENTAL:NO
            ${ENCLAVE_MEMLEAK_LINK_FLAGS})
        endif()

        set(HOST_LINK_OPTIONS ${SHARED_HOST_LINK_OPTIONS} /DEBUG /INCREMENTAL)
        set(HOST_LINK_DYNAMIC_LIBRARY_OPTIONS ${HOST_LINK_OPTIONS})
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

    else() # Linux
      if(ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE NAMES "clang-tidy" REQUIRED)

        # setup clang-tidy command from executable + options
        if(ENABLE_CLANG_TIDY_FIX)
          set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" -fix-errors -fix)
        else()
          set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
        endif()
      else()
        set(CLANG_TIDY_COMMAND)
      endif()

      # Review the below(*) if it's necessary to be re-enabled again: this needs a complete rewrite
      # *include(GNUInstallDirs)

      # this is generally frowned apon as it forces all libraries to include this it is needed to force feed gtest with
      # the right c++ runtime perhaps moving this command to a sub project would be sensible *include_directories(BEFORE
      # SYSTEM "/usr/lib/llvm-10/include/c++/v1")
      if(DEBUG_ENCLAVE_MEMLEAK)
        set(ENCLAVE_MEMLEAK_DEFINES MEMLEAK_CHECK)
      endif()

      # *set(DESTDIR ${CMAKE_BINARY_DIR}/tmp)
      cmake_path(SET TEMP_DIR NORMALIZE "/tmp/")
      cmake_path(SET RUNTIME_DIR NORMALIZE "/var/secretarium/runtime/")

      message("SGX_SDK_CONTAINS_DEBUG_INFORMATION is ${SGX_SDK_CONTAINS_DEBUG_INFORMATION}")
      set(SHARED_DEFINES ${SHARED_DEFINES} TEMP_DIR="${TEMP_DIR}" RUNTIME_DIR="${RUNTIME_DIR}")

      if(${BUILD_TYPE} STREQUAL "release")
        if(${SGX_MODE} STREQUAL "release")
          set(HOST_DEFINES ${SHARED_DEFINES} NDEBUG PREVENT_DEBUG_ENCLAVE # what is this?
            DISALLOW_BAD_JUMPS)
        else() # Prerelease "possibly"
          set(HOST_DEFINES
            ${SHARED_DEFINES}
            NDEBUG
            PREVENT_DEBUG_ENCLAVE # what is this?
            DISALLOW_BAD_JUMPS
            EDEBUG # sets SGX_DEBUG_FLAG to 1
          )
        endif()
      else() # debug
        set(HOST_DEFINES ${SHARED_DEFINES} _DEBUG)
      endif()

      if(BUILD_ENCLAVE)
        set(SHARED_ENCLAVE_DEFINES
          _IN_ENCLAVE
          ${SHARED_DEFINES}
          CLEAN_LIBC
          ENCLAVE_STATUS=sgx_status_t
          ENCLAVE_OK=SGX_SUCCESS
          DISALLOW_BAD_JUMPS)
      endif()

      if(${BUILD_TYPE} STREQUAL "release")
        set(CMAKE_CXX_FLAGS_DEBUG "")
        set(CMAKE_C_FLAGS_DEBUG "")
        set(OPTIMIZER_FLAGS -O3)

        if(BUILD_ENCLAVE)
          if(${SGX_MODE} STREQUAL "release")
            set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG ${ENCLAVE_MEMLEAK_DEFINES})
          else() # Prerelease "possibly"
            set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} NDEBUG EDEBUG ${ENCLAVE_MEMLEAK_DEFINES}) # sets

            # SGX_DEBUG_FLAG
          endif()
        endif()
      else() # debug
        set(EXTRA_COMPILE_OPTIONS ${DEBUG_HOST_ENCLAVE_OPTIONS}) # The one actually used by HOST and ENCLAVE instead

        # ofq
        set(CMAKE_CXX_FLAGS_DEBUG ${DEBUG_COMPILE_FLAGS})
        set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
        set(OPTIMIZER_FLAGS -O0)

        if(BUILD_ENCLAVE)
          set(ENCLAVE_DEFINES ${SHARED_ENCLAVE_DEFINES} _DEBUG ${ENCLAVE_MEMLEAK_DEFINES}) # sets SGX_DEBUG_FLAG to 1
        endif()
      endif()

      message("CMAKE_CXX_FLAGS_DEBUG [${CMAKE_CXX_FLAGS_DEBUG}]")
      message("OPTIMIZER_FLAGS [${OPTIMIZER_FLAGS}]")

      set(SHARED_COMPILE_OPTIONS
        -Wno-unknown-pragmas

        # this has a ticket to remove
        -Wno-deprecated-declarations
        -Wno-gnu-zero-variadic-macro-arguments
        ${EXTRA_COMPILE_OPTIONS}
        ${OPTIMIZER_FLAGS})

      message("CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")

      if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
        set(CLANG_WARNS
          -Wc99-extensions
          -Wzero-length-array
          -Wflexible-array-extensions
          -Wpragma-pack-suspicious-include
          -Wshadow-field-in-constructor
          -Wno-gnu-zero-variadic-macro-arguments
          -Wno-implicit-exception-spec-mismatch

          # some extra checks
          -Wnon-virtual-dtor
          -Wdelete-non-virtual-dtor)
      else()
        set(CLANG_WARNS -Wno-variadic-macros -Wno-gnu-zero-variadic-macro-arguments -Wno-c++20-extensions)
      endif()

      set(WARN_BASELINE
        ${CLANG_WARNS}
        -Werror # convert warnings into errors

        # -Wdeprecated-dynamic-exception-spec   #sgx is riddled with throws() -Wsuggest-destructor-override #sgx is
        # not using override in its stl -Wdocumentation-unknown-command       #yes this would be nice
        # -Wexit-time-destructors               #not really an error -Wglobal-constructors                 #not really
        # an error -Wshadow-all                          #a nice to have but a big change -Wsuggest-override
        # #RapidJson is riddled with this problem -Wold-style-cast                      #this would be a massive
        # change -Wconversion                          #this is recommended but causes an explosion in our code
        -Wall
        -Wextra

        # this is needed by yas
        -Wno-variadic-macros)
      set(WARN_PEDANTIC -DWARN_PEDANTIC ${WARN_BASELINE} -Wpedantic)
      set(WARN_SIGN_CONVERSION -Wsign-conversion)
      set(WARN_TYPE_SIZES -Wshorten-64-to-32 -Wsign-compare -Wshift-sign-overflow)

      set(WARN_OK
        -DWARN_OK
        ${WARN_BASELINE}
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-sign-compare)

      # ################################################################################################################
      # reset and apply cmake separate enclave and host compile flags
      set(HOST_DEBUG_OPTIONS)

      if(BUILD_TEST)
        if(DEBUG_HOST_ALL)
          set(DEBUG_HOST_LEAK ON)
          set(DEBUG_HOST_ADDRESS ON)
          set(DEBUG_HOST_THREAD OFF)
          set(DEBUG_HOST_UNDEFINED ON)
        endif()

        if(DEBUG_HOST_ADDRESS)
          set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=address -fno-omit-frame-pointer)
        endif()

        if(DEBUG_HOST_THREAD)
          set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=thread -fno-omit-frame-pointer)
          set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
        endif()

        if(DEBUG_HOST_UNDEFINED)
          set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=undefined)
        endif()

        if(DEBUG_HOST_LEAK)
          if(DEBUG_HOST_ADDRESS)
            set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=leak -fno-omit-frame-pointer)
          else()
            set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=leak)
          endif()
        endif()

        if(DEBUG_HOST_MEMORY)
          if(DEBUG_HOST_ADDRESS)
            set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=leak -fno-omit-frame-pointer)
          else()
            set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=leak)
          endif()
        endif()

        if(DEBUG_HOST_MEMORY)
          set(DEBUG_HOST_FLAG DEBUG_HOST_LEAK)

          if(DEBUG_HOST_ADDRESS)
            set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=leak -fno-omit-frame-pointer)
          else()
            set(HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS} -fsanitize=leak)
          endif()
        endif()
      endif()

      if(BUILD_ENCLAVE)
        set(ENCLAVE_COMPILE_OPTIONS
          ${SHARED_COMPILE_OPTIONS}
          -Wno-c++17-extensions
          -ffunction-sections
          -fdata-sections
          -Wno-implicit-exception-spec-mismatch)
        set(HOST_LINK_OPTIONS -L/opt/intel/sgxsdk/lib64 -lsgx_tcrypto ${HOST_DEBUG_OPTIONS})
        set(HOST_LINK_EXE_OPTIONS -lsgx_dcap_quoteverify -lsgx_dcap_ql ${HOST_DEBUG_OPTIONS})
      else()
        set(HOST_LINK_OPTIONS ${HOST_LINK_OPTIONS} ${HOST_DEBUG_OPTIONS})
        set(HOST_LINK_EXE_OPTIONS ${HOST_DEBUG_OPTIONS})
      endif()

      set(HOST_LINK_DYNAMIC_LIBRARY_OPTIONS ${HOST_LINK_OPTIONS} -fPIC)

      set(HOST_COMPILE_OPTIONS ${SHARED_COMPILE_OPTIONS} -Wno-trigraphs ${HOST_DEBUG_OPTIONS})

      # this is here as we need to override the rpath we cannot use the ${HOST_LINK_EXE_OPTIONS} as cmake scrambles the
      # $
      set(CMAKE_EXE_LINKER_FLAGS [[-Wl,-rpath,'$ORIGIN']])

      if(ENABLE_COVERAGE)
        message("enabling code coverage")

        # clang specific list(APPEND HOST_COMPILE_OPTIONS -fprofile-instr-generate -fcoverage-mapping)
        # set(CMAKE_EXE_LINKER_FLAGS [[-Wl,-rpath,'$ORIGIN' -fprofile-instr-generate -fcoverage-mapping ]])

        # gcc lcov list(APPEND HOST_COMPILE_OPTIONS -fprofile-instr-generate -fcoverage-mapping)
        # set(CMAKE_EXE_LINKER_FLAGS [[-Wl,-rpath,'$ORIGIN' -fprofile-instr-generate ]])

        # list(APPEND HOST_COMPILE_OPTIONS --coverage) set(CMAKE_EXE_LINKER_FLAGS [[-Wl,-rpath,'$ORIGIN'
        # -fprofile-instr-generate ]])

        # finally gcc gcov
        list(APPEND HOST_COMPILE_OPTIONS -fprofile-arcs -ftest-coverage)
        set(CMAKE_EXE_LINKER_FLAGS [[-Wl,-rpath,'$ORIGIN' -fprofile-arcs -ftest-coverage ]])
      endif()
    endif()

    if(BUILD_ENCLAVE)
      find_package(SGX REQUIRED)
    endif()

    # need for SGX_SDK_CONTAINS_DEBUG_INFORMATION has just been determined in FindSGX.cmake
    if(SGX_SDK_CONTAINS_DEBUG_INFORMATION)
      list(APPEND ENCLAVE_DEFINES SGX_SDK_CONTAINS_DEBUG_INFORMATION)
    endif()

    set(HOST_INCLUDES ${SGX_INCLUDE_DIR})

    if(BUILD_ENCLAVE)
      set(ENCLAVE_LIBC_INCLUDES ${SGX_INCLUDE_DIR} ${SGX_TLIBC_INCLUDE_DIR})
      set(ENCLAVE_LIBCXX_INCLUDES ${ENCLAVE_LIBC_INCLUDES} ${SGX_LIBCXX_INCLUDE_DIR} ${SGX_LIBSTDCXX_INCLUDE_DIR})
    endif()

    set(HOST_LIBRARIES ${SHARED_HOST_LIBRARIES})

    if(BUILD_ENCLAVE)
      # add some sgx here
      set(HOST_LIBRARIES ${HOST_LIBRARIES} ${SGX_USVC_LIB} sgx_capable ${SGX_URTS_LIB})
    endif()
  else()
    message(FATAL_ERROR "Invalid ENCLAVE_TARGET value")
  endif()

  message("HOST_DEFINES ${HOST_DEFINES}")
  message("HOST_COMPILE_OPTIONS ${HOST_COMPILE_OPTIONS}")
  message("HOST_LINK_OPTIONS ${HOST_LINK_OPTIONS}")
  message("HOST_LINK_EXE_OPTIONS ${HOST_LINK_EXE_OPTIONS}")
  message("HOST_DEBUG_OPTIONS ${HOST_DEBUG_OPTIONS}")

  message("ENCLAVE_DEFINES ${ENCLAVE_DEFINES}")
  message("ENCLAVE_COMPILE_OPTIONS ${ENCLAVE_COMPILE_OPTIONS}")
  message("ENCLAVE_LINK_OPTIONS ${ENCLAVE_LINK_OPTIONS}")

  # ####################################################################################################################
  # this gets rid of the 'd' suffix to file names needed to prevent confusion with the enclave measurement logic, it
  # will be nice to remove this limitation
  set(CMAKE_DEBUG_POSTFIX
    ""
    CACHE STRING "Adds a postfix for debug-built libraries." FORCE)

  # ####################################################################################################################
  # enable testing
  if(BUILD_TEST)
    include(GoogleTest)
    enable_testing()
  endif()
endif(NOT DEPENDENCIES_LOADED)
