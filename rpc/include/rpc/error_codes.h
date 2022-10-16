#pragma once 

namespace rpc
{
    namespace error
    {
        int OK();
        int OUT_OF_MEMORY();
        int NEED_MORE_MEMORY();
        int SECURITY_ERROR();
        int INVALID_DATA();
        int TRANSPORT_ERROR();
        int INVALID_METHOD_ID();
        int INVALID_INTERFACE_ID();
        int INVALID_CAST();
        int ZONE_NOT_SUPPORTED();
        int ZONE_NOT_INITIALISED();
        int ZONE_NOT_FOUND();
        int OBJECT_NOT_FOUND();

        void set_OK_val(int val);
        void set_offset_val(int val);
        void set_offset_val_is_negative(bool val);
    };
}