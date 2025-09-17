/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>

namespace rpc
{
    namespace error
    {
        int OK_val = 0;
        int offset_val = 0;
        int offset_val_is_negative = true;

        [[nodiscard]] int OK()
        {
            return OK_val;
        }
        [[nodiscard]] int OUT_OF_MEMORY()
        {
            return offset_val + (offset_val_is_negative ? -1 : 1);
        }
        [[nodiscard]] int NEED_MORE_MEMORY()
        {
            return offset_val + (offset_val_is_negative ? -2 : 2);
        }
        [[nodiscard]] int SECURITY_ERROR()
        {
            return offset_val + (offset_val_is_negative ? -3 : 3);
        }
        [[nodiscard]] int INVALID_DATA()
        {
            return offset_val + (offset_val_is_negative ? -4 : 4);
        }
        [[nodiscard]] int TRANSPORT_ERROR()
        {
            return offset_val + (offset_val_is_negative ? -5 : 5);
        }
        [[nodiscard]] int INVALID_METHOD_ID()
        {
            return offset_val + (offset_val_is_negative ? -6 : 6);
        }
        [[nodiscard]] int INVALID_INTERFACE_ID()
        {
            return offset_val + (offset_val_is_negative ? -7 : 7);
        }
        [[nodiscard]] int INVALID_CAST()
        {
            return offset_val + (offset_val_is_negative ? -8 : 8);
        }
        [[nodiscard]] int ZONE_NOT_SUPPORTED()
        {
            return offset_val + (offset_val_is_negative ? -9 : 9);
        }
        [[nodiscard]] int ZONE_NOT_INITIALISED()
        {
            return offset_val + (offset_val_is_negative ? -10 : 10);
        }
        [[nodiscard]] int ZONE_NOT_FOUND()
        {
            return offset_val + (offset_val_is_negative ? -11 : 11);
        }
        [[nodiscard]] int OBJECT_NOT_FOUND()
        {
            return offset_val + (offset_val_is_negative ? -12 : 12);
        }
        [[nodiscard]] int INVALID_VERSION()
        {
            return offset_val + (offset_val_is_negative ? -13 : 13);
        }
        [[nodiscard]] int EXCEPTION()
        {
            return offset_val + (offset_val_is_negative ? -14 : 14);
        }
        [[nodiscard]] int PROXY_DESERIALISATION_ERROR()
        {
            return offset_val + (offset_val_is_negative ? -15 : 15);
        }
        [[nodiscard]] int STUB_DESERIALISATION_ERROR()
        {
            return offset_val + (offset_val_is_negative ? -16 : 16);
        }
        [[nodiscard]] int INCOMPATIBLE_SERVICE()
        {
            return offset_val + (offset_val_is_negative ? -17 : 17);
        }
        [[nodiscard]] int INCOMPATIBLE_SERIALISATION()
        {
            return offset_val + (offset_val_is_negative ? -18 : 18);
        }
        [[nodiscard]] int REFERENCE_COUNT_ERROR()
        {
            return offset_val + (offset_val_is_negative ? -19 : 19);
        }
        [[nodiscard]] int UNABLE_TO_CREATE_SERVICE_PROXY()
        {
            return offset_val + (offset_val_is_negative ? -20 : 20);
        }
        [[nodiscard]] int SERVICE_PROXY_LOST_CONNECTION()
        {
            return offset_val + (offset_val_is_negative ? -21 : 21);
        }
        [[nodiscard]] int CALL_CANCELLED()
        {
            return offset_val + (offset_val_is_negative ? -22 : 22);
        }
        // dont forget to update MIN & MAX if new values

        [[nodiscard]] int MIN()
        {
            return offset_val + (offset_val_is_negative ? -20 : 1);
        }
        [[nodiscard]] int MAX()
        {
            return offset_val + (offset_val_is_negative ? -1 : 20);
        }

        void set_OK_val(int val)
        {
            OK_val = val;
        }
        void set_offset_val(int val)
        {
            offset_val = val;
        }
        void set_offset_val_is_negative(bool val)
        {
            offset_val_is_negative = val;
        }

        const char* to_string(int err)
        {
            if (err == OUT_OF_MEMORY())
            {
                return "out of memory";
            }
            if (err == NEED_MORE_MEMORY())
            {
                return "need more memory";
            }
            if (err == SECURITY_ERROR())
            {
                return "security error";
            }
            if (err == INVALID_DATA())
            {
                return "invalid data";
            }
            if (err == TRANSPORT_ERROR())
            {
                return "transport error";
            }
            if (err == INVALID_METHOD_ID())
            {
                return "invalid method id";
            }
            if (err == INVALID_INTERFACE_ID())
            {
                return "invalid interface id";
            }
            if (err == INVALID_CAST())
            {
                return "invalid cast";
            }
            if (err == ZONE_NOT_SUPPORTED())
            {
                return "zone not supported";
            }
            if (err == ZONE_NOT_INITIALISED())
            {
                return "zone not initialised";
            }
            if (err == ZONE_NOT_FOUND())
            {
                return "zone not found";
            }
            if (err == OBJECT_NOT_FOUND())
            {
                return "object not found";
            }
            if (err == INVALID_VERSION())
            {
                return "invalid version";
            }
            if (err == EXCEPTION())
            {
                return "exception";
            }
            if (err == PROXY_DESERIALISATION_ERROR())
            {
                return "proxy deserialisation error";
            }
            if (err == STUB_DESERIALISATION_ERROR())
            {
                return "stub deserialisation error";
            }
            if (err == INCOMPATIBLE_SERVICE())
            {
                return "service proxy is incompatible with the client";
            }
            if (err == INCOMPATIBLE_SERIALISATION())
            {
                return "service proxy does not support this serialisation format try JSON";
            }
            if (err == REFERENCE_COUNT_ERROR())
            {
                return "reference count error";
            }
            if (err == UNABLE_TO_CREATE_SERVICE_PROXY())
            {
                return "unable to create service proxy";
            }
            if (err == SERVICE_PROXY_LOST_CONNECTION())
            {
                return "Service proxy has lost connection to the remote service";
            }
            if (err == CALL_CANCELLED())
            {
                return "Service proxy remote call is cancelled";
            }
            return "invalid error code";
        }
    };
}
