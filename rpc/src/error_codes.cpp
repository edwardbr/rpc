#include <rpc/error_codes.h>

namespace rpc
{
    namespace error
    {
        int OK_val = 0;
        int offset_val = 0;
        int offset_val_is_negative = true;

        [[nodiscard]] int OK(){return OK_val;}
        [[nodiscard]] int OUT_OF_MEMORY(){return offset_val + (offset_val_is_negative ? - 1 : 1);}
        [[nodiscard]] int NEED_MORE_MEMORY(){return offset_val + (offset_val_is_negative ? - 2 : 2);}
        [[nodiscard]] int SECURITY_ERROR(){return offset_val + (offset_val_is_negative ? - 3 : 3);}
        [[nodiscard]] int INVALID_DATA(){return offset_val + (offset_val_is_negative ? - 4 : 4);}
        [[nodiscard]] int TRANSPORT_ERROR(){return offset_val + (offset_val_is_negative ? - 5 : 5);}
        [[nodiscard]] int INVALID_METHOD_ID(){return offset_val + (offset_val_is_negative ? - 6 : 6);}
        [[nodiscard]] int INVALID_INTERFACE_ID(){return offset_val + (offset_val_is_negative ? - 7 : 7);}
        [[nodiscard]] int INVALID_CAST(){return offset_val + (offset_val_is_negative ? - 8 : 8);}
        [[nodiscard]] int ZONE_NOT_SUPPORTED(){return offset_val + (offset_val_is_negative ? - 9 : 9);}
        [[nodiscard]] int ZONE_NOT_INITIALISED(){return offset_val + (offset_val_is_negative ? - 10 : 10);}
        [[nodiscard]] int ZONE_NOT_FOUND(){return offset_val + (offset_val_is_negative ? - 11 : 11);}
        [[nodiscard]] int OBJECT_NOT_FOUND(){return offset_val + (offset_val_is_negative ? - 12 : 12);}
        [[nodiscard]] int INVALID_VERSION(){return offset_val + (offset_val_is_negative ? - 13 : 13);}//dont forget to update MIN & MAX if new values

        [[nodiscard]] int MIN(){return offset_val + (offset_val_is_negative ? -13 : 1);}
        [[nodiscard]] int MAX(){return offset_val + (offset_val_is_negative ? -1 : 13);}

        void set_OK_val(int val){OK_val = val;}
        void set_offset_val(int val){offset_val = val;}
        void set_offset_val_is_negative(bool val){offset_val_is_negative = val;}
    };
}