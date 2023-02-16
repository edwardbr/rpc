# RPC++
Remote Procedure Calls for modern C++

## Intro
This library implements an RPC solution that reflects modern C++ concepts.  It is intended for developers that want a pure C++ experience and supports C++ types natively, and all other languages communicate with this library using JSON or some other generic protocol.  

## What are Remote Procedure Calls?
RPC has been around for decades targeting mainly the C programming language and was very popular in the 80's and 90's before falling out of favour with XML rpc, SOAP and REST.

RPC calls are function calls that pretend to be function calls that run code locally when in fact the calls are serialized up and sent to a different location such as a remote machine, where the parameters are unpacked and the function call is made.  The return values are then sent back in a similar manner back to the caller, with the caller potentially unaware that the call was made on a remote location.

Typically the caller calls the function with the same signature as the implementation into a 'proxy' the proxy then packages up the call and sends it to the intended target.  The target then calls a 'stub' that unpacks the request and calls the function on the callers behalf.

To do this the target object needs to implement a set of agreed functions, known as an interface, or in C++ speak a pure abstract virtual base class.  These interfaces are then defined in a language not dissimilar to C++ header files called Interface Definition Language files with an extension of *.idl.

A code generator then reads this idl and generates pure C++ headers containing virtual base classes that implementors need to satisfy as well as proxy and stub code.

The only thing left to do is to implement code that uses a particular transport for getting the calls to their destination and back again.  As well as an rpc::service object that acts as the function calling router for that component.

## Use cases

Although RPC is mainly intended to communicate between machines it can also be used for:
 * In-process calls.
 * In-process calls between two different memory arenas.
 * To other applications on the same machine.
 * To embedded devices

It builds on the principals of (D)COM and CORBA from the 90's. However all interactions are done with smart pointers based on STL std::shared_ptr etc.

As with all RPC an interface definition file is required.  In this case we use files with a .idl extension, protocol buffers and flat buffers have their own formats, however this library uses an idl that closely matches modern C++ header files. 

This idl format supports structures, interfaces, namespaces and basic STL type objects including string, map, vector, optional, variant.

## Feature pipelines

This initial version uses synchronous calls only, however on the pipeline it is planned to work with coroutines.

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

