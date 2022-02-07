#pragma once

#include <marshaller/proxy.h>

namespace rpc
{
    class local_service_proxy : public service_proxy
    {
        rpc::shared_ptr<i_marshaller> the_service_;
        local_service_proxy(rpc::shared_ptr<i_marshaller> the_service)
            : the_service_(the_service)
        {
        }

    public:
        static rpc::shared_ptr<local_service_proxy> create(rpc::shared_ptr<i_marshaller> the_service, uint64_t object_id)
        {
            auto ret = rpc::shared_ptr<local_service_proxy>(new local_service_proxy(the_service));
            ret->weak_this_ = rpc::static_pointer_cast<service_proxy>(ret);
            ret->set_root_object(object_id);
            return ret;
        }

        error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, size_t out_size_, char* out_buf_) override
        {
            return the_service_->send(object_id, interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
        }
        error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
        {
            return the_service_->try_cast(zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
        {
            return the_service_->add_ref(zone_id, object_id);
        }
        uint64_t release(uint64_t zone_id, uint64_t object_id) override
        {
            return the_service_->release(zone_id, object_id);
        }
    };
}