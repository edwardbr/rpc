#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>

#include <yas/count_streams.hpp>
#include <yas/serialize.hpp>

#include <rpc/types.h>
#include <rpc/error_codes.h>

namespace rpc
{
    enum class encoding : uint64_t
    {
        enc_default = 0,    //equivelant to yas_binary
        yas_binary = 1,
        yas_compressed_binary = 2,
        //yas_text = 4,     //not really needed
        yas_json = 8        //we may have different json parsers that have a better implementation e.g. glaze
        // protocol_buffers = 16,
        // flat_buffers = 32,
        // mpi = 64
    };
    
    //note a serialiser may support more than one encoding
    namespace serialiser
    {
        class yas{};
        class protocol_buffers{};
        class flat_buffers{};
        class open_mpi{};
        // etc..
    };

    struct span
    {
        const uint8_t* begin;
        const uint8_t* end;
        
        span(const uint8_t* begin_, const uint8_t* end_) : begin(begin_), end(end_){}
        span(const char* begin_,    const char* end_) : begin((const uint8_t*)begin_), end((const uint8_t*)end_){}
        span(const int8_t* begin_,  const int8_t* end_) : begin((const uint8_t*)begin_), end((const uint8_t*)end_){}
        
        
        span(const std::string l) : begin((const uint8_t*)&*l.begin()), end((const uint8_t*)&*l.end()){}
        
        span(const std::vector<uint8_t> l) : begin(&*l.begin()), end(&*l.end()){}
        span(const std::vector<char> l) : begin((const uint8_t*)&*l.begin()), end((const uint8_t*)&*l.end()){}
        span(const std::vector<int8_t> l) : begin((const uint8_t*)&*l.begin()), end((const uint8_t*)&*l.end()){}
        
        template<size_t size>
        span(const std::array<uint8_t, size> l) : begin(&*l.begin()), end(&*l.end()){}
        template<size_t size>
        span(const std::array<char, size> l) : begin((const uint8_t*)&*l.begin()), end((const uint8_t*)&*l.end()){}
        template<size_t size>
        span(const std::array<int8_t, size> l) : begin((const uint8_t*)&*l.begin()), end((const uint8_t*)&*l.end()){}
    };
    
    // serialisation primatives, it will be nice to move yas out of headers and make the library hidden.  Something for the future
    template<typename T,class OutputBlob = std::vector<std::uint8_t>>
	OutputBlob to_yas_json(const T& obj)
	{
		yas::shared_buffer yas_buffer = yas::save<::yas::mem|::yas::json|::yas::no_header>(obj);
		return OutputBlob(yas_buffer.data.get(), yas_buffer.data.get() + yas_buffer.size);
	}
	
	template<typename T,class OutputBlob = std::vector<std::uint8_t>>
	OutputBlob to_yas_binary(const T& obj)
	{
		yas::shared_buffer yas_buffer = yas::save<::yas::mem|::yas::binary|::yas::no_header>(obj);
		return OutputBlob(yas_buffer.data.get(), yas_buffer.data.get() + yas_buffer.size);
	}
	
	template<typename T,class OutputBlob = std::vector<std::uint8_t>>
	OutputBlob to_compressed_yas_binary(const T& obj)
	{
		yas::shared_buffer yas_buffer = yas::save<::yas::mem|::yas::binary|::yas::compacted|::yas::no_header>(obj);
		return OutputBlob(yas_buffer.data.get(), yas_buffer.data.get() + yas_buffer.size);
	}
    
    template<typename T,class OutputBlob = std::vector<std::uint8_t>>
	OutputBlob serialise(const T& obj, encoding enc)
	{
        if(enc == encoding::yas_json)
            return to_yas_json(obj);
        if(enc == encoding::enc_default || enc == encoding::yas_binary)
            return to_yas_binary(obj);
        if(enc == encoding::yas_compressed_binary)
            return to_compressed_yas_binary(obj);
		throw std::runtime_error("invalid encoding type");
	}
    
	// deserialisation primatives
	template<typename T>
	std::string from_yas_json(const span& data, T& obj)
	{
		try
		{
			yas::load<yas::mem|yas::json|yas::no_header>(yas::intrusive_buffer{(const char*)data.begin, (size_t)(data.end - data.begin)}, obj);
			return "";
		}
		catch(const std::exception& ex)
		{
			// an error has occurred so do the best one can and set the type_id to 0
			return std::string("An exception has occured a data blob was incompatible with the type that is deserialising to: ") + ex.what();
		}
		catch(...)
		{
			// an error has occurred so do the best one can and set the type_id to 0
			return "An exception has occured a data blob was incompatible with the type that is deserialising to";
		}
	}
	
	template<typename T>
	std::string from_yas_binary(const span& data, T& obj)
	{
		try
		{
			yas::load<yas::mem|::yas::binary|::yas::no_header>(yas::intrusive_buffer{(const char*)data.begin, (size_t)(data.end - data.begin)}, obj);
			return "";
		}
		catch(const std::exception& ex)
		{
			// an error has occurred so do the best one can and set the type_id to 0
			return std::string("An exception has occured a data blob was incompatible with the type that is deserialising to: ") + ex.what();
		}
		catch(...)
		{
			// an error has occurred so do the best one can and set the type_id to 0
			return "An exception has occured a data blob was incompatible with the type that is deserialising to";
		}
	}
	
	template<typename T>
	std::string from_yas_compressed_binary(const span& data, T& obj)
	{
		try
		{
			yas::load<yas::mem|::yas::binary|::yas::compacted|::yas::no_header>(yas::intrusive_buffer{(const char*)data.begin, (size_t)(data.end - data.begin)}, obj);
			return "";
		}
		catch(const std::exception& ex)
		{
			// an error has occurred so do the best one can and set the type_id to 0
			return std::string("An exception has occured a data blob was incompatible with the type that is deserialising to: ") + ex.what();
		}
		catch(...)
		{
			// an error has occurred so do the best one can and set the type_id to 0
			return "An exception has occured a data blob was incompatible with the type that is deserialising to";
		}
	}
    
        
    template<typename T>
	std::string deserialise(encoding enc, const span& data, const T& obj)
	{
        if(enc == encoding::yas_json)
            return from_yas_json(data, obj);
        if(enc == encoding::enc_default || enc == encoding::yas_binary)
            return from_yas_binary(data, obj);
        if(enc == encoding::yas_compressed_binary)
            return from_yas_compressed_binary(data, obj);
		return "invalid encoding type";
	}
}
