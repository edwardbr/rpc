#pragma once

#include <rpc/proxy.h>
#include <rpc/error_codes.h>

namespace rpc
{
    class root_service_proxy : public service_proxy
    {
        root_service_proxy(const rpc::shared_ptr<service>& serv)
            : service_proxy(serv, serv)
        {
        }

    public:
        ~root_service_proxy()
        {
            log_str("~root_service_proxy",100);
        }
        static rpc::shared_ptr<root_service_proxy> create(const rpc::shared_ptr<service>& serv)
        {
            auto ret = rpc::shared_ptr<root_service_proxy>(new root_service_proxy(serv));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            return ret;
        }

        int send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override
        {
            return get_service().send(zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
        {
            return get_service().try_cast(zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
        {
            return get_service().add_ref(zone_id, object_id);
        }
        uint64_t release(uint64_t zone_id, uint64_t object_id) override
        {
            return get_service().release(zone_id, object_id);
        }
    };

    class branch_service_proxy : public service_proxy
    {
        branch_service_proxy(const rpc::shared_ptr<service>& serv, const rpc::shared_ptr<service>& operating_zone_service)
            : service_proxy(serv, operating_zone_service)
        {
        }

    public:
        ~branch_service_proxy()
        {
            log_str("~branch_service_proxy",100);
        }
        static rpc::shared_ptr<branch_service_proxy> create(const rpc::shared_ptr<service>& serv, const rpc::shared_ptr<service>& operating_zone_service)
        {
            auto ret = rpc::shared_ptr<branch_service_proxy>(new branch_service_proxy(serv, operating_zone_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            operating_zone_service->add_zone_proxy(pthis);
            return ret;
        }

        int send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override
        {
            return get_service().send(zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
        {
            return get_service().try_cast(zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
        {
            return get_service().add_ref(zone_id, object_id);
        }
        uint64_t release(uint64_t zone_id, uint64_t object_id) override
        {
            return get_service().release(zone_id, object_id);
        }
    };
}