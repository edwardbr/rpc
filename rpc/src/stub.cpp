/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>

namespace rpc
{
    object_stub::object_stub(object id, service& zone, void* target)
        : id_(id)
        , zone_(zone)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_stub_creation(zone_.get_zone_id(), id_, (uint64_t)target);
#endif            
    }
    object_stub::~object_stub()
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_stub_deletion(zone_.get_zone_id(), id_);
#endif            
    }

    rpc::shared_ptr<rpc::casting_interface> object_stub::get_castable_interface() const
    {
        std::lock_guard g(map_control);
        RPC_ASSERT(!stub_map.empty());
        auto& iface = stub_map.begin()->second;
        return iface->get_castable_interface();
    }

    //this method is not thread safe as it is only used when the object is constructed by service
    //or by an internal call by this class
    void object_stub::add_interface(const rpc::shared_ptr<i_interface_stub>& iface)
    {
        stub_map[iface->get_interface_id(rpc::VERSION_2)] = iface;
    }

    rpc::shared_ptr<i_interface_stub> object_stub::get_interface(interface_ordinal interface_id)
    {
        std::lock_guard g(map_control);
        auto res = stub_map.find(interface_id);
        if(res == stub_map.end())
            return nullptr;
        return res->second;
    }

    CORO_TASK(int) object_stub::call(
        uint64_t protocol_version
        , rpc::encoding enc
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
            std::lock_guard g(map_control);
            auto item = stub_map.find(interface_id);
            if (item != stub_map.end())
            {
                stub = item->second;
            }
        }
        if(stub)
        {
            CO_RETURN CO_AWAIT stub->call(protocol_version, enc, caller_channel_zone_id, caller_zone_id, method_id, in_size_, in_buf_, out_buf_);
        }        
        RPC_ERROR("Invalid interface ID in stub call");
        CO_RETURN rpc::error::INVALID_INTERFACE_ID();
    }

    int object_stub::try_cast(interface_ordinal interface_id)
    {
        std::lock_guard g(map_control);
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
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_stub_add_ref(zone_.get_zone_id(), id_, {}, ret, {});
#endif            
        RPC_ASSERT(ret != std::numeric_limits<uint64_t>::max());
        RPC_ASSERT(ret != 0);
        return ret;
    }

    uint64_t object_stub::release()
    {
        uint64_t count = --reference_count;
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_stub_release(zone_.get_zone_id(), id_, {}, count, {});
#endif            
        RPC_ASSERT(count != std::numeric_limits<uint64_t>::max());
        return count;
    }

    void object_stub::release_from_service()
    {
        zone_.release_local_stub(p_this);
    }

}