#pragma once

#include <marshaller/proxy.h>

namespace rpc
{
    //This is for enclaves to call the host 
    class host_service_proxy : public service_proxy
    {
        host_service_proxy(const rpc::shared_ptr<service>& serv, uint64_t zone_id);

    public:
        static rpc::shared_ptr<service_proxy> create(const rpc::shared_ptr<service>& serv, uint64_t zone_id)
        {
            auto ret = rpc::shared_ptr<host_service_proxy>(new host_service_proxy(serv, zone_id));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            return pthis;
        }

    public:
        virtual ~host_service_proxy();
        int initialise();

        int send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };
}