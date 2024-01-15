#pragma once

#include <rpc/casting_interface.h>
#include <rpc/proxy.h>

namespace rpc
{
    bool are_in_same_zone(const casting_interface* first, const casting_interface* second)
    {
        if(!first)
            return true;
        if(!second)
            return true;
        auto first_address_null = first->query_proxy_base() == nullptr;
        auto second_address_null = second->query_proxy_base() == nullptr;
        if(first_address_null || second_address_null)
            return true;
        
        // if((!first_address_null && second_address_null) || (first_address_null && !second_address_null))
        //     return false;
        
        auto first_zone_id = first->query_proxy_base()->get_object_proxy()->get_service_proxy()->get_zone_id();
        auto second_zone_id = second->query_proxy_base()->get_object_proxy()->get_service_proxy()->get_zone_id();
        if(first_zone_id == second_zone_id)
            return true;
        return false;
    }
}