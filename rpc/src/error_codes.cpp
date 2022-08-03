namespace rpc
{
    namespace error
    {
        int OK_val = 0;
        int offset_val = 0;
        int offset_val_is_negative = true;

        int OK(){return OK_val;}
        int OUT_OF_MEMORY(){return offset_val + (offset_val_is_negative ? - 1 : 1);}
        int NEED_MORE_MEMORY(){return offset_val + (offset_val_is_negative ? - 2 : 2);}
        int SECURITY_ERROR(){return offset_val + (offset_val_is_negative ? - 3 : 3);}
        int INVALID_DATA(){return offset_val + (offset_val_is_negative ? - 4 : 4);}
        int TRANSPORT_ERROR(){return offset_val + (offset_val_is_negative ? - 5 : 5);}
        int INVALID_METHOD_ID(){return offset_val + (offset_val_is_negative ? - 6 : 6);}
        int INVALID_INTERFACE_ID(){return offset_val + (offset_val_is_negative ? - 7 : 7);}
        int INVALID_CAST(){return offset_val + (offset_val_is_negative ? - 8 : 8);}

        void set_OK_val(int val){OK_val = val;}
        void set_offset_val(int val){offset_val = val;}
        void set_offset_val_is_negative(bool val){offset_val_is_negative = val;}
    };
}