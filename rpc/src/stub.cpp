#include "rpc/stub.h"
#include "rpc/service.h"
#include "rpc/version.h"
#include "rpc/logger.h"

namespace rpc
{
    object_stub::object_stub(object id, service& zone)
        : id_(id)
        , zone_(zone)
    {
#ifdef USE_RPC_LOGGING
        auto message = std::string("object_stub::object_stub zone ") + std::to_string(zone_.get_zone_id()) 
        + std::string(", object_id ") + std::to_string(id_);
        LOG_STR(message.c_str(), message.size());
#endif
    }
    object_stub::~object_stub()
    {
#ifdef USE_RPC_LOGGING
        auto message = std::string("object_stub::~object_stub zone ") + std::to_string(zone_.get_zone_id()) 
        + std::string(", object_id ") + std::to_string(id_);
        LOG_STR(message.c_str(), message.size());
#endif
    }

    rpc::shared_ptr<rpc::casting_interface> object_stub::get_castable_interface() const
    {
        assert(!stub_map.empty());
        auto& iface = stub_map.begin()->second;
        return iface->get_castable_interface();
    }

    void object_stub::add_interface(const rpc::shared_ptr<i_interface_stub>& iface)
    {
#ifdef RPC_V1        
        stub_map[iface->get_interface_id(rpc::VERSION_1)] = iface;
#endif
#ifdef RPC_V2
        stub_map[iface->get_interface_id(rpc::VERSION_2)] = iface;
#endif
    }

    rpc::shared_ptr<i_interface_stub> object_stub::get_interface(interface_ordinal interface_id)
    {
        auto res = stub_map.find(interface_id);
        if(res == stub_map.end())
            return nullptr;
        return res->second;
    }

    int object_stub::call(
        uint64_t protocol_version
        , caller_channel_zone caller_channel_zone_id
        , caller_zone caller_zone_id
        , interface_ordinal interface_id
        , method method_id
        , size_t in_size_
        , const char* in_buf_
        , std::vector<char>& out_buf_)
    {
        rpc::shared_ptr<i_interface_stub> stub;
        {
            auto item = stub_map.find(interface_id);
            if (item != stub_map.end())
            {
                stub = item->second;
            }
        }
        if(stub)
        {
            return stub->call(protocol_version, caller_channel_zone_id, caller_zone_id, method_id, in_size_, in_buf_, out_buf_);
        }        
        return rpc::error::INVALID_INTERFACE_ID();
    }

    int object_stub::try_cast(interface_ordinal interface_id)
    {
        int ret = rpc::error::OK();
        auto item = stub_map.find(interface_id);
        if (item == stub_map.end())
        {
            rpc::shared_ptr<i_interface_stub> new_stub;
            rpc::shared_ptr<i_interface_stub> stub = stub_map.begin()->second;
            ret = stub->cast(interface_id, new_stub);
            if (ret == rpc::error::OK() && new_stub)
            {
                add_interface(new_stub);
            }
        }
        return ret;
    }

    uint64_t object_stub::add_ref()
    {
        uint64_t ret = ++reference_count;
        assert(ret != std::numeric_limits<uint64_t>::max());
        assert(ret != 0);
        return ret;
    }

    uint64_t object_stub::release()
    {
        uint64_t count = --reference_count;
        assert(count != std::numeric_limits<uint64_t>::max());
        return count;
    }

    void object_stub::release_from_service()
    {
        zone_.release_local_stub(p_this);
    }

}