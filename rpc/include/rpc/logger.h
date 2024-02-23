#pragma once

#ifndef LOG_STR_DEFINED
# ifdef USE_RPC_LOGGING
#  define LOG_STR(str, sz) rpc_log(str, sz)
#  define LOG_CSTR(str) rpc_log(str, strlen(str))
#  include <sgx_error.h>
   extern "C" {
#  ifdef _IN_ENCLAVE
     sgx_status_t __cdecl rpc_log(const char* str, size_t sz);
#  else
     void rpc_log(const char* str, size_t sz);
#  endif
   }
# else
#  define LOG_STR(str, sz)
#  define LOG_CSTR(str)
# endif
#define LOG_STR_DEFINED
#endif
