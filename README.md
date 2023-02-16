# RPC++
Remote Procedure Calls for modern C++

## Intro
This library implements an RPC solution that reflects modern C++ concepts.  It is intended for developers that want a pure C++ experience and supports C++ types natively, and all other languages communicate with this library using JSON or some other generic protocol.  

## What are Remote Procedure Calls?
Remote procedure calls allow applications to talk each other without getting snarled up in underlying communication protocols.  The transport and the end user programming API's are treated as separate concerns.  
### In a nutshell
In other words you can easily make an API that is accessible from another machine, dll, arena or embedded device and not think about serialization.
### A bit of history
RPC has been around for decades targeting mainly the C programming language and was very popular in the 80's and 90's. The technology reached its zenith with the arrival of (D)COM from Microsoft and CORBA from a consortium of other companies.  Unfortunately both organizations hated each other and with their closed source attitudes people fell out of love with them in favour with XMLRPC, SOAP and REST as the new sexy kids on the block.  

RPC though is a valuable solution for all solutions, however historically it did not offer direct answers for working over insecure networks, partly because of the short sighted intransigence and secrecy of various organizations.  People are coming off from some of the other aging fads and are returning to higher performance solutions such as this.

### Yes but what are they?
RPC calls are function calls that pretend to run code locally, when in fact the calls are serialized up and sent to a different location such as a remote machine, where the parameters are unpacked and the function call is made.  The return values are then sent back in a similar manner back to the caller, with the caller potentially unaware that the call was made on a remote location.

Typically the caller calls the function with the same signature as the implementation through a 'proxy', The proxy then packages up the call and sends it to the intended target.  The target then calls a 'stub' that unpacks the request and calls the function on the callers behalf.

To do this the intended destination object needs to implement a set of agreed functions.  These functions collectively are known as interfaces, or in C++ speak a pure abstract virtual base classes.  These interfaces are then defined in a language not dissimilar to C++ header files called Interface Definition Language files with an extension of *.idl.

A code generator then reads this idl and generates pure C++ headers containing virtual base classes that implementors need to satisfy as well as proxy and stub code.

The only thing left to do is to implement code that uses a particular transport for getting the calls to their destination and back again.  As well as an rpc::service object that acts as the function calling router for that component.

## Use cases

 * In-process calls.
 * In-process calls between different memory arenas.
 * To other applications on the same machine.
 * To embedded devices.
 * To remote devices on a private network or the Internet.

## Design

This idl format supports structures, interfaces, namespaces and basic STL type objects including string, map, vector, optional, variant.

TODO...

## Feature pipelines

This initial version uses synchronous calls only, however on the pipeline it is planned to work with coroutines.
This solution currently only supports error codes, exception based error handling is to be implemented at some date.

Currently it has adaptors for
 * in memory calls
 * calls between different memory arenas
 * calls to SGX enclaves

This implementation currently uses YAS for its serialization needs, however it could be extended to other serializers in the future.

## Building and installation
Out of the box without any configuration management this library can be compiled on windows and linux machines.  It requires a relatively up to date c++ compiler and CMake.

There are different CMake options specified in the root CMakeLists.txt:
```
 option(RPC_STANDALONE "build the rpc on its own and not part of another repo" ON)
 option(RPC_BUILD_SGX "build enclave code" OFF)
 option(RPC_BUILD_TEST "build test code" ON)
 option(RPC_USE_LOGGING "turn on rpc logging" OFF)
 option(FORCE_DEBUG_INFORMATION "force inclusion of debug information" OFF)
 ```

From the command line:
mkdir build
cd build
cmake ..

If you are using VSCode add the CMake Tools extension, you will then see targets on the bar at the bottom to compile code for different targets.

Alternatively you can set them explicitly from the .vscode/settings.json file

```
{
    "cmake.configureArgs": [
        "-DRPC_USE_LOGGING=ON",
        "-DRPC_BUILD_SGX=ON"
    ]
}
```

Also for viewing idl files more easily install the "Microsoft MIDL and Mozilla WebIDL syntax highlighting" extension to get some highlighting support.

Currently tested on Compilers:
Clang 10+ 
GCC 9.4
Visual Studio 2017

Though strongly suggest upgrading to the latest version of these compilers
