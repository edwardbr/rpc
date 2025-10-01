/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
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

#include <rpc/internal/error_codes.h>
#include <rpc/internal/assert.h>
#include <rpc/internal/types.h>
#include <rpc/internal/version.h>
#include <rpc/internal/marshaller.h>
#include <rpc/internal/remote_pointer.h>
#include <rpc/internal/coroutine_support.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

#ifdef BUILD_COROUTINE
#include <coro/io_scheduler.hpp>
#endif

namespace rpc
{
    class i_interface_stub;
    class object_stub;
    class service;
    class child_service;
    class service_proxy;
    class casting_interface;
    struct current_service_tracker;

    const object dummy_object_id = {std::numeric_limits<uint64_t>::max()};

    template<class T>
    CORO_TASK(int)
    demarshall_interface_proxy(uint64_t protocol_version,
        const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val);

    template<class T>
    CORO_TASK(rpc::interface_descriptor)
    create_interface_stub(rpc::service& serv, const rpc::shared_ptr<T>& iface);

    class service_logger
    {
    public:
        virtual ~service_logger() = default;
        virtual void before_send(caller_zone caller_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_)
            = 0;
        virtual void after_send(caller_zone caller_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            int ret,
            const std::vector<char>& out_buf_)
            = 0;
    };

    // responsible for all object lifetimes created within the zone
    class service : public i_marshaller, public std::enable_shared_from_this<rpc::service>
    {
    protected:
        static std::atomic<uint64_t> zone_id_generator;
        zone zone_id_ = {0};
        mutable std::atomic<uint64_t> object_id_generator = 0;

        // map object_id's to stubs
        mutable std::mutex stub_control;
        std::unordered_map<object, std::weak_ptr<object_stub>> stubs;
        std::unordered_map<rpc::interface_ordinal,
            std::shared_ptr<std::function<std::shared_ptr<rpc::i_interface_stub>(const std::shared_ptr<rpc::i_interface_stub>&)>>>
            stub_factories;
        // map wrapped objects pointers to stubs
        std::map<void*, std::weak_ptr<object_stub>> wrapped_object_to_stub;
        std::string name_;

#ifdef BUILD_COROUTINE
        std::shared_ptr<coro::io_scheduler> io_scheduler_;
#endif

        struct zone_route
        {
            destination_zone dest;
            caller_zone source;
            constexpr bool operator<(const zone_route& val) const
            {
                if (dest == val.dest)
                    return source < val.source;
                return dest < val.dest;
            }
        };

        mutable std::mutex zone_control;
        std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;

        rpc::shared_ptr<casting_interface> get_castable_interface(object object_id, interface_ordinal interface_id);

        template<class T>
        CORO_TASK(interface_descriptor)
        bind_in_proxy(uint64_t protocol_version, const shared_ptr<T>& iface, std::shared_ptr<rpc::object_stub>& stub);
        template<class T>
        CORO_TASK(interface_descriptor)
        bind_out_stub(uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            const shared_ptr<T>& iface);

        void inner_add_zone_proxy(const std::shared_ptr<rpc::service_proxy>& service_proxy);
        void cleanup_service_proxy(const std::shared_ptr<rpc::service_proxy>& other_zone);

        friend casting_interface;
        friend i_interface_stub;
        friend current_service_tracker;

    protected:
        struct child_service_tag
        {
        };

    public:
#ifdef BUILD_COROUTINE
        explicit service(const char* name, zone zone_id, const std::shared_ptr<coro::io_scheduler>& scheduler);
        explicit service(
            const char* name, zone zone_id, const std::shared_ptr<coro::io_scheduler>& scheduler, child_service_tag);
#else
        explicit service(const char* name, zone zone_id);
        explicit service(const char* name, zone zone_id, child_service_tag);
#endif
        virtual ~service();

        static zone generate_new_zone_id();

#ifdef BUILD_COROUTINE
        template<typename Callable> auto schedule(Callable&& callable)
        {
            // Forwards the lambda (or any other callable) to the real scheduler
            return io_scheduler_->schedule(std::forward<Callable>(callable));
        }
        auto get_scheduler() const { return io_scheduler_; }
#endif

        // we are using a pointer as this is a thread local variable it will not change mid stream, only use this function when servicing an rpc call
        static service* get_current_service();
        static void set_current_service(service* svc);
        object generate_new_object_id() const;
        std::string get_name() const { return name_; }

        virtual bool check_is_empty() const;
        virtual bool has_service_proxies() const;
        zone get_zone_id() const { return zone_id_; }
        void set_zone_id(zone zone_id) { zone_id_ = zone_id; }
        virtual destination_zone get_parent_zone_id() const { return {0}; }
        virtual std::shared_ptr<rpc::service_proxy> get_parent() const { return nullptr; }
        virtual bool set_parent_proxy(const std::shared_ptr<rpc::service_proxy>&)
        {
            RPC_ASSERT(false);
            return false;
        }

        // passed by value implementing an implicit lock on the life time of ptr
        object get_object_id(const shared_ptr<casting_interface>& ptr) const;

        CORO_TASK(interface_descriptor)
        prepare_out_param(uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            rpc::casting_interface* base);
        CORO_TASK(interface_descriptor)
        get_proxy_stub_descriptor(uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            rpc::casting_interface* pointer,
            std::function<std::shared_ptr<rpc::i_interface_stub>(std::shared_ptr<object_stub>)> fn,
            bool outcall,
            std::shared_ptr<object_stub>& stub);

        std::weak_ptr<object_stub> get_object(object object_id) const;

        CORO_TASK(int)
        send(uint64_t protocol_version,
            encoding encoding,
            uint64_t tag,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_) override;
        CORO_TASK(int)
        try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id) override;
        CORO_TASK(int)
        add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count) override;
        CORO_TASK(int)
        release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            release_options options,
            uint64_t& reference_count) override;

        uint64_t release_local_stub(const std::shared_ptr<object_stub>& stub, bool is_optimistic);

        virtual void add_zone_proxy(const std::shared_ptr<rpc::service_proxy>& zone);
        virtual std::shared_ptr<rpc::service_proxy> get_zone_proxy(caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            caller_zone new_caller_zone_id,
            bool& new_proxy_added);
        virtual void remove_zone_proxy(destination_zone destination_zone_id, caller_zone caller_zone_id);
        virtual void remove_zone_proxy_if_not_used(destination_zone destination_zone_id, caller_zone caller_zone_id);
        template<class T> rpc::shared_ptr<T> get_local_interface(uint64_t protocol_version, object object_id)
        {
            return rpc::static_pointer_cast<T>(get_castable_interface(object_id, T::get_id(protocol_version)));
        }

        CORO_TASK(interface_descriptor)
        prepare_remote_input_interface(caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            rpc::casting_interface* base,
            std::shared_ptr<rpc::service_proxy>& destination_zone);

        CORO_TASK(void)
        clean_up_on_failed_connection(const std::shared_ptr<rpc::service_proxy>& destination_zone,
            rpc::shared_ptr<rpc::casting_interface> input_interface);

        // int decrement_reference_count(const std::shared_ptr<rpc::service_proxy>& proxy, int ref_count);

        template<class proxy_class, class in_param_type, class out_param_type, typename... Args>
        CORO_TASK(int)
        connect_to_zone(const char* name,
            rpc::destination_zone new_zone_id,
            const rpc::shared_ptr<in_param_type>& input_interface,
            rpc::shared_ptr<out_param_type>& output_interface,
            Args&&... args)
        {
            // RPC_ASSERT(input_interface == nullptr || input_interface->is_local()
            //            || casting_interface::get_zone(*input_interface) == zone_id_);

            auto new_service_proxy = proxy_class::create(name, new_zone_id, shared_from_this(), args...);
            add_zone_proxy(new_service_proxy);
            rpc::interface_descriptor input_descr{{0}, {0}};
            std::shared_ptr<rpc::service_proxy> destination_zone;
            if (input_interface)
            {
                if (input_interface->is_local())
                {
                    caller_channel_zone empty_caller_channel_zone = {};
                    caller_zone caller_zone_id = zone_id_.as_caller();

                    std::shared_ptr<object_stub> stub;
                    auto factory = create_interface_stub(input_interface);
                    input_descr = CO_AWAIT get_proxy_stub_descriptor(rpc::get_version(),
                        empty_caller_channel_zone,
                        caller_zone_id,
                        input_interface.get(),
                        factory,
                        false,
                        stub);
                }
                else
                {
                    input_descr = CO_AWAIT prepare_remote_input_interface({0},
                        new_service_proxy->get_destination_zone_id().as_caller(),
                        input_interface.get(),
                        destination_zone);
                }
            }

            rpc::interface_descriptor output_descr{{0}, {0}};
            auto err_code = CO_AWAIT new_service_proxy->connect(input_descr, output_descr);
            if (err_code != rpc::error::OK())
            {
                // clean up
                CO_AWAIT clean_up_on_failed_connection(destination_zone, input_interface);
                CO_RETURN err_code;
            }

            if (output_descr.object_id != 0 && output_descr.destination_zone_id != 0)
            {
                err_code = CO_AWAIT rpc::demarshall_interface_proxy(
                    rpc::get_version(), new_service_proxy, output_descr, zone_id_.as_caller(), output_interface);
            }
            else
            {
                new_service_proxy->release_external_ref();
                remove_zone_proxy_if_not_used(
                    new_service_proxy->get_destination_zone_id(), new_service_proxy->get_caller_zone_id());
            }
            CO_RETURN err_code;
        }

        template<class SERVICE_PROXY, class PARENT_INTERFACE, class CHILD_INTERFACE, typename... Args>
        CORO_TASK(int)
        attach_remote_zone(const char* name,
            rpc::interface_descriptor input_descr,
            rpc::interface_descriptor& output_descr,
            std::function<CORO_TASK(int)(const rpc::shared_ptr<PARENT_INTERFACE>&,
                rpc::shared_ptr<CHILD_INTERFACE>&,
                const std::shared_ptr<rpc::service>&)> fn,
            Args&&... args)
        {
            // link the child to the parent
            auto parent_service_proxy = CO_AWAIT SERVICE_PROXY::attach_remote("remote", shared_from_this(), args...);
            if (!parent_service_proxy)
                CO_RETURN rpc::error::UNABLE_TO_CREATE_SERVICE_PROXY();
            add_zone_proxy(parent_service_proxy);

            rpc::shared_ptr<PARENT_INTERFACE> parent_ptr;
            if (input_descr != interface_descriptor())
            {
                auto err_code = CO_AWAIT rpc::demarshall_interface_proxy(
                    rpc::get_version(), parent_service_proxy, input_descr, get_zone_id().as_caller(), parent_ptr);
                if (err_code != rpc::error::OK())
                {
                    CO_RETURN err_code;
                }
            }
            rpc::shared_ptr<CHILD_INTERFACE> child_ptr;
            {
                auto err_code = CO_AWAIT fn(parent_ptr, child_ptr, shared_from_this());
                if (err_code != rpc::error::OK())
                {
                    CO_RETURN err_code;
                }
            }
            if (child_ptr)
            {
                RPC_ASSERT(
                    child_ptr->is_local()
                    && "we cannot support remote pointers to subordinate zones as it has not been registered yet");
                output_descr = CO_AWAIT rpc::create_interface_stub(*this, child_ptr);
            }
            CO_RETURN rpc::error::OK();
        }

        template<class T>
        std::function<std::shared_ptr<rpc::i_interface_stub>(const std::shared_ptr<object_stub>& stub)>
        create_interface_stub(const shared_ptr<T>& iface);
        int create_interface_stub(rpc::interface_ordinal interface_id,
            std::function<interface_ordinal(uint8_t)> original_interface_id,
            const std::shared_ptr<rpc::i_interface_stub>& original,
            std::shared_ptr<rpc::i_interface_stub>& new_stub);

        // note this function is not thread safe!  Use it before using the service class for normal operation
        void add_interface_stub_factory(std::function<interface_ordinal(uint8_t)> id_getter,
            std::shared_ptr<std::function<std::shared_ptr<rpc::i_interface_stub>(const std::shared_ptr<rpc::i_interface_stub>&)>>
                factory);

        friend service_proxy;
    };

    // protect the current service local pointer
    struct current_service_tracker
    {
        service* old_service_ = nullptr;
        current_service_tracker(service* current_service)
        {
            old_service_ = service::get_current_service();
            service::set_current_service(current_service);
        }
        ~current_service_tracker() { service::set_current_service(old_service_); }
    };

    // Child services need to maintain the lifetime of the root object in its zone
    class child_service : public service
    {
        // the enclave needs to hold a hard lock to a root object that represents a runtime
        // the enclave service lifetime is managed by the transport functions
        std::mutex parent_protect;
        std::shared_ptr<rpc::service_proxy> parent_service_proxy_;
        destination_zone parent_zone_id_;

    public:
#ifdef BUILD_COROUTINE
        explicit child_service(const char* name,
            zone zone_id,
            destination_zone parent_zone_id,
            const std::shared_ptr<coro::io_scheduler>& io_scheduler)
            : service(name, zone_id, io_scheduler)
            , parent_zone_id_(parent_zone_id)
        {
        }
#else
        explicit child_service(const char* name, zone zone_id, destination_zone parent_zone_id)
            : service(name, zone_id)
            , parent_zone_id_(parent_zone_id)
        {
        }
#endif

        virtual ~child_service();

        std::shared_ptr<rpc::service_proxy> get_parent() const override { return parent_service_proxy_; }
        bool set_parent_proxy(const std::shared_ptr<rpc::service_proxy>& parent_service_proxy) override;

        destination_zone get_parent_zone_id() const override { return parent_zone_id_; }

        template<class SERVICE_PROXY, class PARENT_INTERFACE, class CHILD_INTERFACE, typename... Args>
        static CORO_TASK(int) create_child_zone(const char* name,
            zone zone_id,
            destination_zone parent_zone_id,
            rpc::interface_descriptor input_descr,
            rpc::interface_descriptor& output_descr,
            std::function<CORO_TASK(int)(const rpc::shared_ptr<PARENT_INTERFACE>&,
                rpc::shared_ptr<CHILD_INTERFACE>&,
                const std::shared_ptr<rpc::child_service>&)> fn,
#ifdef BUILD_COROUTINE
            const std::shared_ptr<coro::io_scheduler>& io_scheduler,
#endif
            std::shared_ptr<rpc::child_service>& new_child_service,
            Args&&... args)
        {
            auto child_svc = std::shared_ptr<rpc::child_service>(new rpc::child_service(name,
                zone_id,
                parent_zone_id
#ifdef BUILD_COROUTINE
                ,
                io_scheduler
#endif
                ));

            // link the child to the parent
            auto parent_service_proxy = SERVICE_PROXY::create("parent", parent_zone_id, child_svc, args...);
            if (!parent_service_proxy)
            {
                RPC_ERROR("Unable to create service proxy in create_child_service");
                CO_RETURN rpc::error::UNABLE_TO_CREATE_SERVICE_PROXY();
            }
            child_svc->add_zone_proxy(parent_service_proxy);
            if (!child_svc->set_parent_proxy(parent_service_proxy))
            {
                RPC_ERROR("Unable to create set_parent_proxy in create_child_service");
                CO_RETURN rpc::error::UNABLE_TO_CREATE_SERVICE_PROXY();
            }
            parent_service_proxy->set_parent_channel(true);

            rpc::shared_ptr<PARENT_INTERFACE> parent_ptr;
            if (input_descr != interface_descriptor())
            {
                auto err_code = CO_AWAIT rpc::demarshall_interface_proxy(
                    rpc::get_version(), parent_service_proxy, input_descr, zone_id.as_caller(), parent_ptr);
                if (err_code != rpc::error::OK())
                {
                    CO_RETURN err_code;
                }
            }
            rpc::shared_ptr<CHILD_INTERFACE> child_ptr;
            {
                new_child_service = child_svc;
                auto err_code = CO_AWAIT fn(parent_ptr, child_ptr, child_svc);
                if (err_code != rpc::error::OK())
                {
                    CO_RETURN err_code;
                }
            }
            if (child_ptr)
            {
                RPC_ASSERT(
                    child_ptr->is_local()
                    && "we cannot support remote pointers to subordinate zones as it has not been registered yet");
                output_descr = CO_AWAIT rpc::create_interface_stub(*child_svc, child_ptr);
            }
            CO_RETURN rpc::error::OK();
        };
    };

    template<class T>
    CORO_TASK(rpc::interface_descriptor)
    create_interface_stub(rpc::service& serv, const rpc::shared_ptr<T>& iface)
    {
        caller_channel_zone empty_caller_channel_zone = {};
        caller_zone caller_zone_id = serv.get_zone_id().as_caller();

        if (!iface)
        {
            RPC_ASSERT(false);
            CO_RETURN{{0}, {0}};
        }
        std::shared_ptr<object_stub> stub;
        auto factory = serv.create_interface_stub(iface);
        CO_RETURN CO_AWAIT serv.get_proxy_stub_descriptor(
            rpc::get_version(), empty_caller_channel_zone, caller_zone_id, iface.get(), factory, false, stub);
    }

    struct retry_buffer
    {
        std::vector<char> data;
        int return_value;
    };

}
