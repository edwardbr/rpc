/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <rpc/rpc.h>

namespace rpc
{
    bool are_in_same_zone(const casting_interface* first, const casting_interface* second)
    {
        if (!first)
            return true;
        if (!second)
            return true;
        auto first_address_null = first == nullptr;
        auto second_address_null = second == nullptr;
        if (first_address_null || second_address_null)
            return true;

        // if((!first_address_null && second_address_null) || (first_address_null && !second_address_null))
        //     return false;

        auto first_zone_id = casting_interface::get_zone(*first);
        auto second_zone_id = casting_interface::get_zone(*second);
        if (first_zone_id == second_zone_id)
            return true;
        return false;
    }



    object casting_interface::get_object_id(const casting_interface& iface)
    {
        auto obj = iface.get_object_proxy();
        if (!obj)
            return {0};
        return obj->get_object_id();
    }

    std::shared_ptr<rpc::service_proxy> casting_interface::get_service_proxy(const casting_interface& iface)
    {
        auto obj = iface.get_object_proxy();
        if (!obj)
            return nullptr;
        return obj->get_service_proxy();
    }

    std::shared_ptr<rpc::service> casting_interface::get_service(const casting_interface& iface)
    {
        auto proxy = get_service_proxy(iface);
        if (!proxy)
            return nullptr;
        return proxy->get_operating_zone_service();
    }

    zone casting_interface::get_zone(const casting_interface& iface)
    {
        auto proxy = get_service_proxy(iface);
        if (!proxy)
            return {0};
        return proxy->get_zone_id();
    }

    destination_zone casting_interface::get_destination_zone(const casting_interface& iface)
    {
        auto proxy = get_service_proxy(iface);
        if (!proxy)
            return {0};
        return proxy->get_destination_zone_id();
    }

    destination_channel_zone casting_interface::get_channel_zone(const casting_interface& iface)
    {
        auto proxy = get_service_proxy(iface);
        if (!proxy)
            return {0};
        return proxy->get_destination_channel_zone_id();
    }
}
