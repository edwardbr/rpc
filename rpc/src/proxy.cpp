/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include "rpc/proxy.h"
#include "rpc/logger.h"
#include <cstdio>
#include <limits>

namespace rpc
{
    object_proxy::object_proxy(object object_id, rpc::shared_ptr<service_proxy> service_proxy)
        : object_id_(object_id)
        , service_proxy_(service_proxy)
    {
    }

    object_proxy::~object_proxy()
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_object_proxy_deletion(
                service_proxy_->get_zone_id(), service_proxy_->get_destination_zone_id(), object_id_);
        }
#endif

        // Handle inherited references from race conditions by passing the count to on_object_proxy_released
        int inherited_count = inherited_reference_count_.load();
        if (inherited_count > 0) {
            auto destructor_msg = "object_proxy destructor: " + std::to_string(inherited_count) + " inherited references will be handled by on_object_proxy_released for object " + std::to_string(object_id_.get_val());
            LOG_STR(destructor_msg.c_str(), destructor_msg.size());
        }

        // Handle both normal destruction and inherited references in on_object_proxy_released
        service_proxy_->on_object_proxy_released(object_id_, inherited_count);
        service_proxy_ = nullptr;
    }

    int object_proxy::send(uint64_t protocol_version,
        rpc::encoding encoding,
        uint64_t tag,
        rpc::interface_ordinal interface_id,
        rpc::method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_)
    {
        return service_proxy_->send_from_this_zone(
            protocol_version, encoding, tag, object_id_, interface_id, method_id, in_size_, in_buf_, out_buf_);
    }

    int object_proxy::send(uint64_t tag,
        std::function<interface_ordinal(uint64_t)> id_getter,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_)
    {
        return service_proxy_->send_from_this_zone(
            encoding::enc_default, tag, object_id_, id_getter, method_id, in_size_, in_buf_, out_buf_);
    }

    int object_proxy::try_cast(std::function<interface_ordinal(uint64_t)> id_getter)
    {
        return service_proxy_->sp_try_cast(service_proxy_->get_destination_zone_id(), object_id_, id_getter);
    }

    destination_zone object_proxy::get_destination_zone_id() const
    {
        return service_proxy_->get_destination_zone_id();
    }
}
