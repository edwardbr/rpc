#include <rpc/casting_interface.h>
#include <rpc/proxy.h>

namespace rpc
{    
    void casting_interface::__add_shared_ptr() const
    {
        const auto* ip = get_interface_proxy();
        if(ip)
        {
            auto op = ip->get_object_proxy();
//            op->add_shared_ptr();
        }
    }
    
    void casting_interface::__remove_shared_ptr() const
    {
        const auto* ip = get_interface_proxy();
        if(ip)
        {
            auto op = ip->get_object_proxy();
//            op->remove_shared_ptr();
        }
    }

    bool are_in_same_zone(const casting_interface* first, const casting_interface* second)
    {
        if(!first)
            return true;
        if(!second)
            return true;
        bool first_address_null = first->get_interface_proxy() == nullptr;
        bool second_address_null = second->get_interface_proxy() == nullptr;
        if(first_address_null || second_address_null)
            return true;
        
        // if((!first_address_null && second_address_null) || (first_address_null && !second_address_null))
        //     return false;
        
        auto first_zone_id = first->get_interface_proxy()->get_object_proxy()->get_service_proxy()->get_zone_id();
        auto second_zone_id = second->get_interface_proxy()->get_object_proxy()->get_service_proxy()->get_zone_id();
        if(first_zone_id == second_zone_id)
            return true;
        return false;
    }
}