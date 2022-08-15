#pragma once

#include <rpc/proxy.h>

namespace rpc
{
    //This is for enclaves to call the host 
    class host_service_proxy : public service_proxy
    {
        host_service_proxy(uint64_t host_zone_id, const rpc::shared_ptr<service>& operating_zone_service);

    public:
        static rpc::shared_ptr<service_proxy> create(uint64_t host_zone_id, const rpc::shared_ptr<service>& operating_zone_service)
        {
            auto ret = rpc::shared_ptr<host_service_proxy>(new host_service_proxy(host_zone_id, operating_zone_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            operating_zone_service->add_zone_proxy(ret);
            return pthis;
        }

    public:
        virtual ~host_service_proxy();
        int initialise();

        int send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };
}