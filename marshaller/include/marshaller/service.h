#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>

#include <marshaller/marshaller.h>

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>

#include <marshaller/marshaller.h>

namespace rpc
{
    class i_interface_stub;
    class object_stub;
    class service;
    class service_proxy;
    
    // responsible for all object lifetimes created within the zone
    class service : public i_marshaller
    {
        uint64_t zone_id_ = 0;
        std::atomic<uint64_t> object_id_generator = 0;

        // map object_id's to stubs
        std::unordered_map<uint64_t, rpc::weak_ptr<object_stub>> stubs;
        // map wrapped objects pointers to stubs
        std::unordered_map<void*, rpc::weak_ptr<object_stub>> wrapped_object_to_stub;

        // hard lock on the root object
        rpc::shared_ptr<i_interface_stub> root_stub;

        std::unordered_map<uint64_t, rpc::weak_ptr<service_proxy>> other_zones;

        std::mutex insert_control;
    public:
        service(uint64_t zone_id = 1) : zone_id_(zone_id){}
        virtual ~service();
        template<class T, class Stub, class obj_stub = object_stub> error_code initialise(rpc::shared_ptr<T> root_ob)
        {
            assert(check_is_empty());

            auto id = get_new_object_id();
            auto os = rpc::shared_ptr<obj_stub>(new obj_stub(id, *this));
            root_stub = rpc::static_pointer_cast<i_interface_stub>(Stub::create(root_ob, os));
            os->add_interface(root_stub);
            add_object(root_ob.get(), os);
            return 0;
        }

        // this function is needed by services where there is no shared pointer to this object, and its lifetime
        void cleanup();
        bool check_is_empty();
        uint64_t get_zone_id(){return zone_id_;}
        void set_zone_id(uint64_t zone_id){zone_id_ = zone_id;}
        uint64_t get_root_object_id();
        uint64_t get_new_object_id() { return object_id_generator++; }

        template<class T> uint64_t encapsulate_outbound_interfaces(rpc::shared_ptr<T> object);

        error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;

        uint64_t add_lookup_stub(void* pointer,
                                 std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn);
        error_code add_object(void* pointer, rpc::shared_ptr<object_stub> stub);
        rpc::weak_ptr<object_stub> get_object(uint64_t object_id);

        error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;

        void add_zone(uint64_t zone_id, rpc::weak_ptr<service_proxy> zone);
        rpc::weak_ptr<service_proxy> get_zone(uint64_t zone_id);
        void remove_zone(uint64_t zone_id);
    };
}