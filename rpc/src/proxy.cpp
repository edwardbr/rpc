/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include "rpc/proxy.h"
#include "rpc/logger.h"
#include <cstdio>
#include <limits>

namespace rpc
{
    object_proxy::object_proxy( object object_id, 
                                rpc::shared_ptr<service_proxy> service_proxy)
        : object_id_(object_id)
        , service_proxy_(service_proxy)
    {}

    object_proxy::~object_proxy() 
    { 
        // Get service_proxy once for the entire destructor to ensure consistency
        auto service_proxy = service_proxy_.get_nullable();

        // Add detailed logging to track object_proxy destruction
#ifdef USE_RPC_LOGGING
        if (service_proxy)
        {
            RPC_DEBUG("object_proxy destructor: service zone={} destination_zone={} caller_zone={} object_id={}",
                      service_proxy->get_zone_id().get_val(),
                      service_proxy->get_destination_zone_id().get_val(),
                      service_proxy->get_caller_zone_id().get_val(),
                      object_id_.get_val());
        }
        else
        {
            RPC_DEBUG("object_proxy destructor: service_proxy_ is nullptr for object_id={}",
                      object_id_.get_val());
        }
#endif

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service && service_proxy)
        {
            telemetry_service->on_object_proxy_deletion(service_proxy->get_zone_id(), service_proxy->get_destination_zone_id(), object_id_);
        }
#endif

        // Handle additional references inherited from concurrent proxy destruction
        int inherited_count = inherited_reference_count_.load();
#ifdef USE_RPC_LOGGING
        if (inherited_count > 0)
        {
            RPC_DEBUG("object_proxy destructor: {} inherited references will be handled by on_object_proxy_released for object {}",
                      inherited_count, object_id_.get_val());
        }
#endif

        service_proxy->on_object_proxy_released(object_id_, inherited_count);
        service_proxy_ = nullptr;
    }
    
    CORO_TASK(int) object_proxy::send(
            uint64_t protocol_version 
            , rpc::encoding encoding 
            , uint64_t tag 
            , rpc::interface_ordinal interface_id 
            , rpc::method method_id 
            , size_t in_size_
            , const char* in_buf_ 
            , std::vector<char>& out_buf_)
    {
        auto service_proxy = service_proxy_.get_nullable();
        RPC_ASSERT(service_proxy);
        if (!service_proxy)
            CO_RETURN rpc::error::ZONE_NOT_INITIALISED();
        CO_RETURN CO_AWAIT service_proxy->send_from_this_zone(
            protocol_version,
            encoding,
            tag,
            object_id_, 
            interface_id, 
            method_id, 
            in_size_, 
            in_buf_, 
            out_buf_);
    }

    CORO_TASK(int) object_proxy::send(uint64_t tag, std::function<interface_ordinal (uint64_t)> id_getter, method method_id, size_t in_size_, const char* in_buf_,
                                  std::vector<char>& out_buf_)
    {
        auto service_proxy = service_proxy_.get_nullable();
        RPC_ASSERT(service_proxy);
        if (!service_proxy)
            CO_RETURN rpc::error::ZONE_NOT_INITIALISED();
        CO_RETURN CO_AWAIT service_proxy->send_from_this_zone(
            encoding::enc_default,
            tag,
            object_id_, 
            id_getter, 
            method_id, 
            in_size_, 
            in_buf_, 
            out_buf_);
    }

    CORO_TASK(int) object_proxy::try_cast(std::function<interface_ordinal (uint64_t)> id_getter)
    {
        auto service_proxy = service_proxy_.get_nullable();
        RPC_ASSERT(service_proxy);
        if (!service_proxy)
            CO_RETURN rpc::error::ZONE_NOT_INITIALISED();
        CO_RETURN CO_AWAIT service_proxy->sp_try_cast(service_proxy->get_destination_zone_id(), object_id_, id_getter);
    }

    destination_zone object_proxy::get_destination_zone_id() const 
    {
        auto service_proxy = service_proxy_.get_nullable();
        RPC_ASSERT(service_proxy);
        if (!service_proxy)
            return destination_zone{0};
        return service_proxy->get_destination_zone_id();
    }
}
