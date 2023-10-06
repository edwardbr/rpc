#pragma once 

namespace rpc
{
    namespace error
    {
        int OK();
        int MIN();
        int OUT_OF_MEMORY();                //service has no more memory
        int NEED_MORE_MEMORY();             //a call needs more memory for its out parameters
        int SECURITY_ERROR();               //a security specific issue 
        int INVALID_DATA();
        int TRANSPORT_ERROR();              //an error with the custom transport has occured
        int INVALID_METHOD_ID();            //a method call is invalid based on the wrong ordinal
        int INVALID_INTERFACE_ID();         //an interface is not implemented by an object
        int INVALID_CAST();                 //unable to cast an interface to another one
        int ZONE_NOT_SUPPORTED();           //zone is not consistent with the proxy 
        int ZONE_NOT_INITIALISED();         //zone is not ready
        int ZONE_NOT_FOUND();               //zone not found
        int OBJECT_NOT_FOUND();             //object id is invalid
        int INVALID_VERSION();              //a service proxy does not supports a version of rpc
        int EXCEPTION();                    //an uncaught exception has occured
        int PROXY_DESERIALISATION_ERROR();  //a proxy is unable to deserialise data from a service
        int STUB_DESERIALISATION_ERROR();   //a stub is unable to deserialise data from a caller
        int INCOMPATIBLE_SERVICE();         //a service proxy is incompatible with the client
        int INCOMPATIBLE_SERIALISATION();   //service proxy does not support this serialisation format try JSON
        int REFERENCE_COUNT_ERROR();        //reference count error
        int MAX();//the biggest value

        void set_OK_val(int val);
        void set_offset_val(int val);
        void set_offset_val_is_negative(bool val);
        const char* to_string(int);
    };
}