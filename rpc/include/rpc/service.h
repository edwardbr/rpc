#pragma once

#include <string>
#include <memory>
#include <map>
#include <list>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <limits>
#include <functional>

#include <rpc/error_codes.h>
#include <rpc/assert.h>
#include <rpc/types.h>
#include <rpc/version.h>
#include <rpc/marshaller.h>
#include <rpc/remote_pointer.h>
#include <rpc/casting_interface.h>
#include <rpc/i_telemetry_service.h>

namespace rpc
{
    class i_interface_stub;
    class object_stub;
    class service;
    class child_service;
    class service_proxy;

    const object dummy_object_id = {std::numeric_limits<uint64_t>::max()};

    class service_logger
    {
    public:
        virtual ~service_logger() = default;
        virtual void before_send(caller_zone caller_zone_id, object object_id, interface_ordinal interface_id, method method_id, size_t in_size_, const char* in_buf_) = 0;
        virtual void after_send(caller_zone caller_zone_id, object object_id, interface_ordinal interface_id, method method_id, int ret, const std::vector<char>& out_buf_) = 0;
    };
    
    // responsible for all object lifetimes created within the zone
    class service : 
        public i_marshaller,
        public rpc::enable_shared_from_this<service>
    {
    protected:
        static std::atomic<uint64_t> zone_id_generator;
        zone zone_id_ = {0};
        mutable std::atomic<uint64_t> object_id_generator = 0;

        // map object_id's to stubs
        mutable std::mutex stub_control;
        std::unordered_map<object, rpc::weak_ptr<object_stub>> stubs;
        std::unordered_map<rpc::interface_ordinal, std::shared_ptr<std::function<rpc::shared_ptr<rpc::i_interface_stub>(const rpc::shared_ptr<rpc::i_interface_stub>&)>>> stub_factories;
        // map wrapped objects pointers to stubs
        std::map<void*, rpc::weak_ptr<object_stub>> wrapped_object_to_stub;

        struct zone_route
        {
            destination_zone dest;
            caller_zone source;
            constexpr bool operator<(const zone_route& val) const 
            {
                if(dest == val.dest)
                    return source < val.source;
                return dest < val.dest;
            }
        };

        mutable std::mutex zone_control;
        std::map<zone_route, rpc::weak_ptr<service_proxy>> other_zones;
        std::list<std::shared_ptr<service_logger>> service_loggers;

        // hard lock on the root object
        const i_telemetry_service* telemetry_service_ = nullptr;
        rpc::shared_ptr<casting_interface> get_castable_interface(object object_id, interface_ordinal interface_id);
                                                        
        template<class T> interface_descriptor proxy_bind_in_param(uint64_t protocol_version, const shared_ptr<T>& iface, shared_ptr<object_stub>& stub);
        template<class T> interface_descriptor stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const shared_ptr<T>& iface);

        void inner_add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy);

        friend proxy_base;                  
        friend i_interface_stub;                          

    public:
        explicit service(zone zone_id, const i_telemetry_service* telemetry_service);
        virtual ~service();

        static zone generate_new_zone_id();
        object generate_new_object_id() const;

        virtual bool check_is_empty() const;
        zone get_zone_id() const {return zone_id_;}
        void set_zone_id(zone zone_id){zone_id_ = zone_id;}
        virtual destination_zone get_parent_zone_id() const {return {0};}
        virtual rpc::shared_ptr<rpc::service_proxy> get_parent() const {return nullptr;}
        virtual void set_parent(const rpc::shared_ptr<rpc::service_proxy>& parent_service_proxy){RPC_ASSERT(false);};
        const i_telemetry_service* get_telemetry_service() const {return telemetry_service_;}

        interface_descriptor prepare_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, rpc::proxy_base* base);
        interface_descriptor get_proxy_stub_descriptor(uint64_t protocol_version, 
                                                        caller_channel_zone caller_channel_zone_id, 
                                                        caller_zone caller_zone_id, 
                                                        rpc::casting_interface* pointer,
                                                        std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn,
                                                        bool outcall,
                                                        rpc::shared_ptr<object_stub>& stub);
                                 
        rpc::weak_ptr<object_stub> get_object(object object_id) const;

        int send(
            uint64_t protocol_version 
			, encoding encoding 
			, uint64_t tag 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id 
            , method method_id 
            , size_t in_size_
            , const char* in_buf_ 
            , std::vector<char>& out_buf_)
            override;
        int try_cast(            
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id) override;
        uint64_t add_ref(
            uint64_t protocol_version 
            , destination_channel_zone destination_channel_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , add_ref_options build_out_param_channel 
            , bool proxy_add_ref) override;
        uint64_t release(
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_zone caller_zone_id) override;

        uint64_t release_local_stub(const rpc::shared_ptr<rpc::object_stub>& stub);

        virtual void add_zone_proxy(const rpc::shared_ptr<service_proxy>& zone);
        virtual rpc::shared_ptr<service_proxy> get_zone_proxy(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, caller_zone new_caller_zone_id, bool& new_proxy_added);
        virtual void remove_zone_proxy(destination_zone destination_zone_id, caller_zone caller_zone_id, destination_channel_zone destination_channel_zone_id);
        template<class T> rpc::shared_ptr<T> get_local_interface(uint64_t protocol_version, object object_id)
        {
            return rpc::static_pointer_cast<T>(get_castable_interface(object_id, T::get_id(protocol_version)));
        }
        
        template<class proxy_class, class in_param_type, class out_param_type, typename... Args>
        int connect_to_zone(
            const rpc::i_telemetry_service* telemetry_service, 
            const rpc::shared_ptr<in_param_type>& input_interface, 
            rpc::shared_ptr<out_param_type>& output_interface,
            Args... args)
        {
            RPC_ASSERT(input_interface == nullptr || !input_interface->query_proxy_base() || input_interface->query_proxy_base()->get_object_proxy()->get_service_proxy()->get_zone_id() == zone_id_);

            auto new_service_proxy = proxy_class::create(args...);

            rpc::interface_descriptor input_descr{{0},{0}};
            if(input_interface)
            {
                if(input_interface->query_proxy_base())
                {
                    input_descr = prepare_out_param(rpc::get_version(), {0}, new_service_proxy->get_destination_zone_id().as_caller(), input_interface->query_proxy_base());   
                }
                else
                {
                    caller_channel_zone empty_caller_channel_zone = {};
                    caller_zone caller_zone_id = zone_id_.as_caller();

                    rpc::shared_ptr<rpc::object_stub> stub;
                    auto factory = create_interface_stub(input_interface);
                    input_descr = get_proxy_stub_descriptor(rpc::get_version(), empty_caller_channel_zone, caller_zone_id, input_interface.get(), factory, false, stub);        
                }
            }

            rpc::interface_descriptor output_descr{{0},{0}};
            auto err_code = new_service_proxy->connect(input_descr, output_descr);
            if(err_code != rpc::error::OK())
            {
                //clean up
                return err_code;
            }
            
            if(output_descr.object_id != 0 && output_descr.destination_zone_id != 0)
            {
                err_code = rpc::demarshall_interface_proxy(rpc::get_version(), new_service_proxy, output_descr, zone_id_.as_caller(), output_interface);
            }
            return err_code;
        }

        template<class T> 
        std::function<shared_ptr<i_interface_stub>(const shared_ptr<object_stub>& stub)> create_interface_stub(
            const shared_ptr<T>& iface);
        int create_interface_stub(
            rpc::interface_ordinal interface_id
            , std::function<interface_ordinal(uint8_t)> original_interface_id
            , const rpc::shared_ptr<rpc::i_interface_stub>& original
            , rpc::shared_ptr<rpc::i_interface_stub>& new_stub);

        //note this function is not thread safe!  Use it before using the service class for normal operation
        void add_interface_stub_factory(std::function<interface_ordinal (uint8_t)> id_getter, std::shared_ptr<std::function<rpc::shared_ptr<rpc::i_interface_stub>(const rpc::shared_ptr<rpc::i_interface_stub>&)>> factory);

        //note this is not thread safe and should only be used on setup
        void add_service_logger(const std::shared_ptr<service_logger>& logger)
        {
            service_loggers.push_back(logger);
        }
        friend service_proxy;
    };

    //Child services need to maintain the lifetime of the root object in its zone 
    class child_service : public service
    {
        //the enclave needs to hold a hard lock to a root object that represents a runtime
        //the enclave service lifetime is managed by the transport functions 
        rpc::shared_ptr<rpc::service_proxy> parent_service_proxy_;
        destination_zone parent_zone_id_;
    public:
        explicit child_service(zone zone_id, destination_zone parent_zone_id, const i_telemetry_service* telemetry_service) : 
            service(zone_id, telemetry_service),
            parent_zone_id_(parent_zone_id)
        {}

        virtual ~child_service();

        rpc::shared_ptr<rpc::service_proxy> get_parent() const override {return parent_service_proxy_;}
        void set_parent(const rpc::shared_ptr<rpc::service_proxy>& parent_service_proxy) override;
        destination_zone get_parent_zone_id() const override {return parent_zone_id_;}
    };


    template<class T> 
    rpc::interface_descriptor create_interface_stub(rpc::service& serv, const rpc::shared_ptr<T>& iface)
    {
        caller_channel_zone empty_caller_channel_zone = {};
        caller_zone caller_zone_id = serv.get_zone_id().as_caller();

        if(!iface)
        {
            RPC_ASSERT(false);
            return {{0},{0}};
        }
        rpc::shared_ptr<rpc::object_stub> stub;
        auto factory = serv.create_interface_stub(iface);
        return serv.get_proxy_stub_descriptor(rpc::get_version(), empty_caller_channel_zone, caller_zone_id, iface.get(), factory, false, stub);
    }


    struct retry_buffer
    {
        std::vector<char> data;
        int return_value;
    };

}
