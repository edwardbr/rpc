#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>
#include <atomic>

#include <rpc/marshaller.h>
#include <rpc/remote_pointer.h>

namespace rpc
{
    class i_interface_stub;
    class object_stub;
    class service;
    class service_proxy;
    
    // responsible for all object lifetimes created within the zone
    class service : public i_marshaller
    {
    protected:
        static std::atomic<uint64_t> zone_count;
        uint64_t zone_id_ = 0;
        mutable std::atomic<uint64_t> object_id_generator = 0;

        // map object_id's to stubs
        std::unordered_map<uint64_t, rpc::weak_ptr<object_stub>> stubs;
        // map wrapped objects pointers to stubs
        std::unordered_map<void*, rpc::weak_ptr<object_stub>> wrapped_object_to_stub;

        std::unordered_map<uint64_t, rpc::weak_ptr<service_proxy>> other_zones;

        // hard lock on the root object
        mutable std::mutex insert_control;
    public:
        service(uint64_t zone_id = generate_new_zone_id()) : zone_id_(zone_id){}
        virtual ~service();

        // this function is needed by services where there is no shared pointer to this object, and its lifetime
        virtual void cleanup();
        virtual bool check_is_empty() const;
        uint64_t get_zone_id() const {return zone_id_;}
        void set_zone_id(uint64_t zone_id){zone_id_ = zone_id;}
        static uint64_t generate_new_zone_id() { return ++zone_count; }
        uint64_t generate_new_object_id() const { return ++object_id_generator; }

        template<class T> uint64_t encapsulate_outbound_interfaces(const rpc::shared_ptr<T>& object);

        int send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;

        uint64_t add_lookup_stub(void* pointer,
                                 std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn);
        int add_object(void* pointer, const rpc::shared_ptr<object_stub>& stub);
        rpc::weak_ptr<object_stub> get_object(uint64_t object_id) const;

        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;

        virtual void add_zone_proxy(const rpc::shared_ptr<service_proxy>& zone);
        virtual rpc::shared_ptr<service_proxy> get_zone_proxy(uint64_t zone_id) const;
        virtual void remove_zone_proxy(uint64_t zone_id);
    };

    //Child services need to maintain the lifetime of the root object in its zone 
    class child_service : public service
    {
        //the enclave needs to hold a hard lock to a root object that represents a runtime
        //the enclave service lifetime is managed by the transport functions 
        rpc::shared_ptr<i_interface_stub> root_stub_;
        rpc::shared_ptr<rpc::service_proxy> parent_service_;
    public:
        child_service(uint64_t zone_id) : 
            service(zone_id)
        {}

        virtual ~child_service();

        void set_parent(const rpc::shared_ptr<rpc::service_proxy>& parent_service)
        {
            parent_service_ = parent_service;
            add_zone_proxy(parent_service_);
        }

        template<class T, class Stub, class obj_stub = object_stub> 
        void create_stub(const rpc::shared_ptr<T>& root_ob, uint64_t* stub_id = nullptr)
        {
            assert(check_is_empty());

            auto id = generate_new_object_id();
            auto os = rpc::shared_ptr<obj_stub>(new obj_stub(id, *this));
            root_stub_ = rpc::static_pointer_cast<i_interface_stub>(Stub::create(root_ob, os));
            os->add_interface(root_stub_);
            add_object(root_ob.get(), os);
            
            if(stub_id)
            {
                *stub_id = id;
            }
        }
        virtual void cleanup() override;
        bool check_is_empty() const override;
        uint64_t get_root_object_id() const;
        rpc::shared_ptr<service_proxy> get_zone_proxy(uint64_t zone_id) const override;
    };
}