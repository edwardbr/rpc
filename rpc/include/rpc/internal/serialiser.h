/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>

#include <yas/count_streams.hpp>
#include <yas/serialize.hpp>
#include <yas/std_types.hpp>

#include <rpc/internal/types.h>
#include <rpc/internal/error_codes.h>
#include <rpc/rpc_types.h>

namespace rpc
{

    // note a serialiser may support more than one encoding
    namespace serialiser
    {
        class yas
        {
        };
        class protocol_buffers
        {
        };
        class flat_buffers
        {
        };
        class open_mpi
        {
        };
        // etc..
    };

    struct span
    {
        const uint8_t* begin;
        const uint8_t* end;

        span(const uint8_t* begin_, const uint8_t* end_)
            : begin(begin_)
            , end(end_)
        {
        }
        span(const char* begin_, const char* end_)
            : begin((const uint8_t*)begin_)
            , end((const uint8_t*)end_)
        {
        }
        span(const int8_t* begin_, const int8_t* end_)
            : begin((const uint8_t*)begin_)
            , end((const uint8_t*)end_)
        {
        }

        span(const std::string& l)
            : begin((const uint8_t*)l.data())
            , end((const uint8_t*)(l.data() + l.size()))
        {
        }

        span(const std::vector<uint8_t>& l)
            : begin(l.data())
            , end(l.data() + l.size())
        {
        }
        span(const std::vector<char>& l)
            : begin((const uint8_t*)l.data())
            , end((const uint8_t*)(l.data() + l.size()))
        {
        }
        span(const std::vector<int8_t>& l)
            : begin((const uint8_t*)l.data())
            , end((const uint8_t*)(l.data() + l.size()))
        {
        }

        template<size_t size>
        span(const std::array<uint8_t, size>& l)
            : begin(l.data())
            , end(l.data() + l.size())
        {
        }
        template<size_t size>
        span(const std::array<char, size>& l)
            : begin((const uint8_t*)l.data())
            , end((const uint8_t*)(l.data() + l.size()))
        {
        }
        template<size_t size>
        span(const std::array<int8_t, size>& l)
            : begin((const uint8_t*)l.data())
            , end((const uint8_t*)(l.data() + l.size()))
        {
        }
    };

    // serialisation primatives, it will be nice to move yas out of headers and make the library hidden.  Something for the future
    template<class OutputBlob = std::vector<std::uint8_t>, typename T> OutputBlob to_yas_json(const T& obj)
    {
        yas::shared_buffer yas_buffer = yas::save<::yas::mem | ::yas::json | ::yas::no_header>(obj);
        return OutputBlob(yas_buffer.data.get(), yas_buffer.data.get() + yas_buffer.size);
    }

    template<class OutputBlob = std::vector<std::uint8_t>, typename T> OutputBlob to_yas_binary(const T& obj)
    {
        yas::shared_buffer yas_buffer = yas::save<::yas::mem | ::yas::binary | ::yas::no_header>(obj);
        return OutputBlob(yas_buffer.data.get(), yas_buffer.data.get() + yas_buffer.size);
    }

    template<class OutputBlob = std::vector<std::uint8_t>, typename T> OutputBlob to_compressed_yas_binary(const T& obj)
    {
        yas::shared_buffer yas_buffer = yas::save<::yas::mem | ::yas::binary | ::yas::compacted | ::yas::no_header>(obj);
        return OutputBlob(yas_buffer.data.get(), yas_buffer.data.get() + yas_buffer.size);
    }

    template<class OutputBlob = std::vector<std::uint8_t>, typename T> OutputBlob serialise(const T& obj, encoding enc)
    {
        if (enc == encoding::yas_json)
            return to_yas_json(obj);
        if (enc == encoding::enc_default || enc == encoding::yas_binary)
            return to_yas_binary(obj);
        if (enc == encoding::yas_compressed_binary)
            return to_compressed_yas_binary(obj);
        throw std::runtime_error("invalid encoding type");
    }

    // serialisation primatives, it will be nice to move yas out of headers and make the library hidden.  Something for the future
    template<typename T> uint64_t yas_json_saved_size(const T& obj)
    {
        return yas::saved_size<::yas::mem | ::yas::json | ::yas::no_header>(obj);
    }

    template<typename T> uint64_t yas_binary_saved_size(const T& obj)
    {
        return yas::saved_size<::yas::mem | ::yas::binary | ::yas::no_header>(obj);
    }

    template<typename T> uint64_t compressed_yas_binary_saved_size(const T& obj)
    {
        return yas::saved_size<::yas::mem | ::yas::binary | ::yas::compacted | ::yas::no_header>(obj);
    }

    template<typename T> uint64_t get_saved_size(const T& obj, encoding enc)
    {
        if (enc == encoding::yas_json)
            return yas_json_saved_size(obj);
        if (enc == encoding::enc_default || enc == encoding::yas_binary)
            return yas_binary_saved_size(obj);
        if (enc == encoding::yas_compressed_binary)
            return compressed_yas_binary_saved_size(obj);
        throw std::runtime_error("invalid encoding type");
    }

    // deserialisation primatives
    template<typename T> std::string from_yas_json(const span& data, T& obj)
    {
        try
        {
            yas::load<yas::mem | yas::json | yas::no_header>(
                yas::intrusive_buffer{(const char*)data.begin, (size_t)(data.end - data.begin)}, obj);
            return "";
        }
        catch (const std::exception& ex)
        {
            // an error has occurred so do the best one can and set the type_id to 0
            return std::string("An exception has occurred a data blob was incompatible with the type that is "
                               "deserialising to: ")
                   + ex.what();
        }
        catch (...)
        {
            // an error has occurred so do the best one can and set the type_id to 0
            return "An exception has occurred a data blob was incompatible with the type that is deserialising to";
        }
    }

    template<typename T> std::string from_yas_binary(const span& data, T& obj)
    {
        try
        {
            yas::load<yas::mem | ::yas::binary | ::yas::no_header>(
                yas::intrusive_buffer{(const char*)data.begin, (size_t)(data.end - data.begin)}, obj);
            return "";
        }
        catch (const std::exception& ex)
        {
            // an error has occurred so do the best one can and set the type_id to 0
            return std::string("An exception has occurred a data blob was incompatible with the type that is "
                               "deserialising to: ")
                   + ex.what();
        }
        catch (...)
        {
            // an error has occurred so do the best one can and set the type_id to 0
            return "An exception has occurred a data blob was incompatible with the type that is deserialising to";
        }
    }

    template<typename T> std::string from_yas_compressed_binary(const span& data, T& obj)
    {
        try
        {
            yas::load<yas::mem | ::yas::binary | ::yas::compacted | ::yas::no_header>(
                yas::intrusive_buffer{(const char*)data.begin, (size_t)(data.end - data.begin)}, obj);
            return "";
        }
        catch (const std::exception& ex)
        {
            // an error has occurred so do the best one can and set the type_id to 0
            return std::string("An exception has occurred a data blob was incompatible with the type that is "
                               "deserialising to: ")
                   + ex.what();
        }
        catch (...)
        {
            // an error has occurred so do the best one can and set the type_id to 0
            return "An exception has occurred a data blob was incompatible with the type that is deserialising to";
        }
    }

    template<typename T> std::string deserialise(encoding enc, const span& data, const T& obj)
    {
        if (enc == encoding::yas_json)
            return from_yas_json(data, obj);
        if (enc == encoding::enc_default || enc == encoding::yas_binary)
            return from_yas_binary(data, obj);
        if (enc == encoding::yas_compressed_binary)
            return from_yas_compressed_binary(data, obj);
        return "invalid encoding type";
    }
}
