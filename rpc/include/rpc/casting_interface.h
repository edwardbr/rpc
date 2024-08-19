#pragma once

#include <rpc/types.h>
#include <rpc/version.h>
#include <type_traits>
#include <stdint.h>

namespace rpc
{
    class proxy_base;

    //this do nothing class is for static pointer casting
    class casting_interface
    {
    public:
        virtual ~casting_interface() = default;
        virtual void* get_address() const = 0;     
        virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;        
        // this is only implemented by proxy_base
        virtual proxy_base* query_proxy_base() const {return nullptr;} 
    };
    bool are_in_same_zone(const casting_interface* first, const casting_interface* second);

    //this is a nice helper function to match an interface id to a interface in a version independant way
    template<class T>
    bool match(rpc::interface_ordinal interface_id)
    {
        return 
            #ifdef RPC_V2
                T::get_id(rpc::VERSION_2) == interface_id
            #endif
            #if defined(RPC_V1) && defined(RPC_V2)
                ||
            #endif
            #ifdef RPC_V1
                T::get_id(rpc::VERSION_1) == interface_id
            #endif            
            #if !defined(RPC_V1) && !defined(RPC_V2)
                false
            #endif
            ;
    }
    
    template <typename T>
    class has_get_id_member
    {
        typedef char one;
        struct two { char x[2]; };

        template <typename C> static one test( decltype(&C::get_id) ) ;
        template <typename C> static two test(...);    

    public:
        enum { value = sizeof(test<T>(0)) == sizeof(char) };
    };
    
    template<typename T>
    class id
    {
        
    };
    
    template <typename T>
    class has_id_get_member
    {
        typedef char one;
        struct two { char x[2]; };

        template <typename C> static one test( decltype(&id<C>::get) ) ;
        template <typename C> static two test(...);    

    public:
        enum { value = sizeof(test<T>(0)) == sizeof(char) };
    };    
    
    // auto test_for_get_id_presence = [](auto &lhs) -> decltype(rpc::get_id<std::remove_reference_t<decltype(lhs)>>(0ULL)) {
    //     return rpc::get_id<std::remove_reference_t<decltype(lhs)>>(rpc::VERSION_2);
    // };

    // template <typename T>
    // constexpr bool has_rpc_get_id_fn = std::is_invocable_v<decltype(test_for_get_id_presence), T&>;
    
    
    auto test_for_get_id_presence = [](auto &lhs) -> decltype(rpc::id<std::remove_reference_t<std::remove_cv_t<decltype(lhs)>>>::get_id(rpc::VERSION_2)) {
        return rpc::id<std::remove_reference_t<std::remove_cv_t<decltype(lhs)>>>::get_id(rpc::VERSION_2);
    };

    template <typename T>
    constexpr bool has_rpc_get_id_fn = std::is_invocable_v<decltype(test_for_get_id_presence), T&>;


    constexpr uint64_t STD_STRING_ID = 0x71FC1FAC5CD5E6F9;
    constexpr uint64_t UINT_8_ID = 0x71FC1FAC5CD5E6F8;
    constexpr uint64_t UINT_16_ID = 0x71FC1FAC5CD5E6F7;
    constexpr uint64_t UINT_32_ID = 0x71FC1FAC5CD5E6F6;
    constexpr uint64_t UINT_64_ID = 0x71FC1FAC5CD5E6F5;
    constexpr uint64_t INT_8_ID = 0x71FC1FAC5CD5E6F4;
    constexpr uint64_t INT_16_ID = 0x71FC1FAC5CD5E6F3;
    constexpr uint64_t INT_32_ID = 0x71FC1FAC5CD5E6F2;
    constexpr uint64_t INT_64_ID = 0x71FC1FAC5CD5E6F1;
    constexpr uint64_t FLOAT_32_ID = 0x71FC1FAC5CD5E6F0;
    constexpr uint64_t FLOAT_64_ID = 0x71FC1FAC5CD5E6EF;


    //declarations more can be added but these are the most commonly used

    template<>
    class id<std::string>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return STD_STRING_ID;
        }
    };

    template<>
    class id<uint8_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return UINT_8_ID;
        }
    };
    
    template<>
    class id<uint16_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return UINT_16_ID;
        }
    };
    
    template<>
    class id<uint32_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return UINT_32_ID;
        }
    };
    
    template<>
    class id<uint64_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return UINT_64_ID;
        }
    };
    
    template<>
    class id<int8_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return INT_8_ID;
        }
    };
    
    template<>
    class id<int16_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return INT_16_ID;
        }
    };
    
    template<>
    class id<int32_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return INT_32_ID;
        }
    };
    
    template<>
    class id<int64_t>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return INT_64_ID;
        }
    };

    
    template<>
    class id<float>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return FLOAT_32_ID;
        }
    };

    
    template<>
    class id<double>
    {
    public:
        static uint64_t get(uint64_t)
        {
            return FLOAT_64_ID;
        }
    };
}