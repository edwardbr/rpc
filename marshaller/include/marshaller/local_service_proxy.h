#pragma once

#include <marshaller/proxy.h>

namespace rpc
{
    class local_service_proxy : public service_proxy
    {
        rpc::shared_ptr<i_marshaller> marshaller_;
        local_service_proxy(const rpc::shared_ptr<service>& service, uint64_t zone_id, rpc::shared_ptr<i_marshaller> marshaller)
            : service_proxy(service, zone_id), marshaller_(marshaller)
        {
        }

    public:
        static rpc::shared_ptr<local_service_proxy> create(const rpc::shared_ptr<service>& serv, uint64_t zone_id, rpc::shared_ptr<i_marshaller> marshaller, uint64_t object_id)
        {
            auto ret = rpc::shared_ptr<local_service_proxy>(new local_service_proxy(serv, zone_id, marshaller));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            serv->add_zone(zone_id, pthis);
            ret->set_root_object(object_id);
            return ret;
        }

        error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override
        {
            return marshaller_->send(object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
        {
            return marshaller_->try_cast(zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
        {
            return marshaller_->add_ref(zone_id, object_id);
        }
        uint64_t release(uint64_t zone_id, uint64_t object_id) override
        {
            return marshaller_->release(zone_id, object_id);
        }
    };
}