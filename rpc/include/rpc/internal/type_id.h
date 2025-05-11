/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <vector>
#include <type_traits>
#include <stdint.h>

namespace rpc
{
    // the route to all fingerprinting
    template<typename T> class id
    {
        // not implemented here! only in the concrete derivations
        // static constexpr uint64_t get(uint64_t)
    };

    constexpr uint64_t STD_VECTOR_UINT_8_ID = 0x71FC1FAC5CD5E6FA;
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

    // declarations more can be added but these are the most commonly used
    template<> class id<std::string>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return STD_STRING_ID; }
    };

    template<> class id<std::vector<uint8_t>>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return STD_VECTOR_UINT_8_ID; }
    };

    template<> class id<uint8_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_8_ID; }
    };

    template<> class id<uint16_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_16_ID; }
    };

    template<> class id<uint32_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_32_ID; }
    };

    template<> class id<uint64_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_64_ID; }
    };

    template<> class id<int8_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_8_ID; }
    };

    template<> class id<int16_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_16_ID; }
    };

    template<> class id<int32_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_32_ID; }
    };

    template<> class id<int64_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_64_ID; }
    };

    template<> class id<float>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return FLOAT_32_ID; }
    };

    template<> class id<double>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return FLOAT_64_ID; }
    };
}
