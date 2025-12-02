/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
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
#include <rpc/internal/service_proxy.h>
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
    class transport;
    class transport_to_parent;

    const object dummy_object_id = {std::numeric_limits<uint64_t>::max()};

    class service_event
    {
    public:
        virtual ~service_event() = default;
        virtual CORO_TASK(void) on_object_released(object object_id, destination_zone destination) = 0;
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
        std::unordered_map<void*, std::weak_ptr<object_stub>> wrapped_object_to_stub;
        std::string name_;

        mutable std::mutex service_events_control;
        std::set<std::weak_ptr<service_event>, std::owner_less<std::weak_ptr<service_event>>> service_events_;

#ifdef BUILD_COROUTINE
        std::shared_ptr<coro::io_scheduler> io_scheduler_;
#endif

        mutable std::mutex zone_control;
        // owned by service proxies
        std::unordered_map<destination_zone, std::weak_ptr<service_proxy>> other_zones;

        // transports owned by:
        // service proxies
        // pass through objects
        // child services to parent transports
        std::unordered_map<destination_zone, std::weak_ptr<transport>> transports;

        void inner_add_transport(destination_zone adjacent_zone_id, const std::shared_ptr<transport>& transport_ptr);
        void inner_remove_transport(destination_zone destination_zone_id);
        std::shared_ptr<rpc::transport> inner_get_transport(destination_zone destination_zone_id) const;

    protected:
        struct child_service_tag
        {
        };

        friend transport;

        // Friend declarations for binding template functions
        template<class T>
        friend CORO_TASK(int) stub_bind_out_param(const std::shared_ptr<rpc::service>&,
            uint64_t,
            caller_channel_zone,
            caller_zone,
            const shared_ptr<T>&,
            interface_descriptor&);

        template<class T>
        friend CORO_TASK(int) stub_bind_in_param(uint64_t,
            const std::shared_ptr<rpc::service>&,
            caller_channel_zone,
            caller_zone,
            const interface_descriptor&,
            shared_ptr<T>&);

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
        zone get_zone_id() const { return zone_id_; }

        virtual bool check_is_empty() const;

        /////////////////////////////////
        // NOTIFICATION LOGIC
        /////////////////////////////////
        void add_service_event(const std::weak_ptr<service_event>& event);
        void remove_service_event(const std::weak_ptr<service_event>& event);
        CORO_TASK(void) notify_object_gone_event(object object_id, destination_zone destination);

        // passed by value implementing an implicit lock on the life time of ptr
        object get_object_id(const shared_ptr<casting_interface>& ptr) const;

        // Single transport version - for remote connections (SPSC/TCP/etc)
        // Creates service_proxy bound to the provided transport
        template<class in_param_type, class out_param_type>
        CORO_TASK(int)
        connect_to_zone(const char* name,
            std::shared_ptr<transport> child_transport,
            const rpc::shared_ptr<in_param_type>& input_interface,
            rpc::shared_ptr<out_param_type>& output_interface);

        // Attach remote zone - for peer-to-peer connections
        // Takes single transport since this is called by the remote peer during connection
        template<class PARENT_INTERFACE, class CHILD_INTERFACE>
        CORO_TASK(int)
        attach_remote_zone(const char* name,
            std::shared_ptr<transport> peer_transport,
            rpc::interface_descriptor input_descr,
            rpc::interface_descriptor& output_descr,
            std::function<CORO_TASK(int)(const rpc::shared_ptr<PARENT_INTERFACE>&,
                rpc::shared_ptr<CHILD_INTERFACE>&,
                const std::shared_ptr<rpc::service>&)> fn);

        // protected:
        /////////////////////////////////
        // i_marshaller LOGIC
        /////////////////////////////////

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
            std::vector<char>& out_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;
        CORO_TASK(void)
        post(uint64_t protocol_version,
            encoding encoding,
            uint64_t tag,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            post_options options,
            size_t in_size_,
            const char* in_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel) override;
        CORO_TASK(int)
        try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;
        CORO_TASK(int)
        add_ref(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;
        CORO_TASK(int)
        release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            release_options options,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

    public:
        /////////////////////////////////
        // STUB LOGIC
        /////////////////////////////////

        // note this function is not thread safe!  Use it before using the service class for normal operation
        void add_interface_stub_factory(std::function<interface_ordinal(uint8_t)> id_getter,
            std::shared_ptr<std::function<std::shared_ptr<rpc::i_interface_stub>(const std::shared_ptr<rpc::i_interface_stub>&)>>
                factory);

        template<class T>
        std::function<std::shared_ptr<rpc::i_interface_stub>(const std::shared_ptr<object_stub>& stub)>
        create_interface_stub(const shared_ptr<T>& iface);

        int create_interface_stub(rpc::interface_ordinal interface_id,
            std::function<interface_ordinal(uint8_t)> original_interface_id,
            const std::shared_ptr<rpc::i_interface_stub>& original,
            std::shared_ptr<rpc::i_interface_stub>& new_stub);

        uint64_t release_local_stub(
            const std::shared_ptr<object_stub>& stub, bool is_optimistic, caller_zone caller_zone_id);

        void add_transport(destination_zone adjacent_zone_id, const std::shared_ptr<transport>& transport_ptr);
        void remove_transport(destination_zone adjacent_zone_id);
        std::shared_ptr<rpc::transport> get_transport(destination_zone destination_zone_id) const;

    protected:
        virtual void add_zone_proxy(const std::shared_ptr<rpc::service_proxy>& zone);

    private:
        virtual std::shared_ptr<rpc::service_proxy> get_zone_proxy(
            caller_zone caller_zone_id, // when you know who is calling you
            destination_zone destination_zone_id,
            bool& new_proxy_added);
        virtual void remove_zone_proxy(destination_zone destination_zone_id);

        /////////////////////////////////
        // BINDING LOGIC
        /////////////////////////////////

        std::weak_ptr<object_stub> get_object(object object_id) const;

        template<class T>
        CORO_TASK(int)
        bind_in_proxy(uint64_t protocol_version,
            const shared_ptr<T>& iface,
            std::shared_ptr<rpc::object_stub>& stub,
            caller_zone caller_zone_id,
            interface_descriptor& descriptor);

        CORO_TASK(int)
        get_proxy_stub_descriptor(uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            rpc::casting_interface* pointer,
            std::function<std::shared_ptr<rpc::i_interface_stub>(std::shared_ptr<object_stub>)> fn,
            bool outcall,
            std::shared_ptr<object_stub>& stub,
            interface_descriptor& descriptor);

        /////////////////////////////////
        // PRIVATE FUNCTIONS
        /////////////////////////////////

        rpc::shared_ptr<casting_interface> get_castable_interface(object object_id, interface_ordinal interface_id);

        void inner_add_zone_proxy(const std::shared_ptr<rpc::service_proxy>& service_proxy);
        void cleanup_service_proxy(const std::shared_ptr<rpc::service_proxy>& other_zone);

        CORO_TASK(int)
        prepare_out_param(uint64_t protocol_version,
            caller_zone caller_zone_id,
            rpc::casting_interface* base,
            interface_descriptor& descriptor);

        CORO_TASK(void)
        clean_up_on_failed_connection(const std::shared_ptr<rpc::service_proxy>& destination_zone,
            rpc::shared_ptr<rpc::casting_interface> input_interface);

        /////////////////////////////////
        // FRIENDS FUNCTIONS
        /////////////////////////////////

        friend service_proxy;

        template<class T>
        friend CORO_TASK(int) rpc::proxy_bind_out_param(const std::shared_ptr<rpc::service_proxy>& sp,
            const rpc::interface_descriptor& encap,
            rpc::shared_ptr<T>& val);

        template<class T>
        friend CORO_TASK(int) rpc::stub_bind_in_param(uint64_t protocol_version,
            rpc::service& serv,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            const rpc::interface_descriptor& encap,
            rpc::shared_ptr<T>& iface);

        template<class T>
        friend CORO_TASK(int) rpc::create_interface_stub(rpc::service& serv,
            const rpc::shared_ptr<T>& iface,
            caller_zone caller_zone_id,
            rpc::interface_descriptor& descriptor);

        template<class T>
        friend CORO_TASK(int) rpc::stub_bind_out_param(rpc::service& zone,
            uint64_t protocol_version,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            const rpc::shared_ptr<T>& iface,
            rpc::interface_descriptor& descriptor);

        template<class T>
        friend CORO_TASK(int) rpc::proxy_bind_in_param(std::shared_ptr<rpc::object_proxy> object_p,
            uint64_t protocol_version,
            const rpc::shared_ptr<T>& iface,
            std::shared_ptr<rpc::object_stub>& stub,
            rpc::interface_descriptor& descriptor);
    };

    // Child services need to maintain the lifetime of the root object in its zone
    class child_service : public service
    {
        // the enclave needs to hold a hard lock to a root object that represents a runtime
        // the enclave service lifetime is managed by the transport functions
        mutable std::mutex parent_protect;
        // Strong reference to parent transport - keeps parent zone alive
        std::shared_ptr<transport> parent_transport_;
        destination_zone parent_zone_id_;

    public:
        // Set parent transport - must be called during child zone creation
        void set_parent_transport(const std::shared_ptr<transport>& parent_transport)
        {
            std::lock_guard lock(parent_protect);
            parent_transport_ = parent_transport;
        }

        std::shared_ptr<transport> get_parent_transport() const
        {
            std::lock_guard lock(parent_protect);
            return parent_transport_;
        }

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

        template<class PARENT_INTERFACE, class CHILD_INTERFACE>
        static CORO_TASK(int) create_child_zone(const char* name,
            std::shared_ptr<transport> parent_transport,
            rpc::interface_descriptor input_descr,
            rpc::interface_descriptor& output_descr,
            std::function<CORO_TASK(int)(const rpc::shared_ptr<PARENT_INTERFACE>&,
                rpc::shared_ptr<CHILD_INTERFACE>&,
                const std::shared_ptr<rpc::child_service>&)>&& fn
#ifdef BUILD_COROUTINE
            ,
            const std::shared_ptr<coro::io_scheduler>& io_scheduler
#endif
        )
        {
            zone zone_id = parent_transport->get_zone_id();

            auto child_svc = std::shared_ptr<rpc::child_service>(new rpc::child_service(name,
                zone_id,
                parent_transport->get_adjacent_zone_id().as_destination()
#ifdef BUILD_COROUTINE
                    ,
                io_scheduler
#endif
                ));

            // Link the child to the parent via transport
            parent_transport->set_service(child_svc);

            // CRITICAL: Child service must keep parent transport alive
            // This ensures parent zone remains reachable while child exists
            child_svc->set_parent_transport(parent_transport);

            auto parent_service_proxy
                = rpc::service_proxy::create("parent", child_svc, parent_transport, input_descr.destination_zone_id);

            child_svc->add_transport(input_descr.destination_zone_id, parent_transport);
            child_svc->add_zone_proxy(parent_service_proxy);

            rpc::shared_ptr<PARENT_INTERFACE> parent_ptr;
            if (input_descr != interface_descriptor())
            {
                auto err_code = CO_AWAIT rpc::demarshall_interface_proxy(
                    rpc::get_version(), parent_service_proxy, input_descr, parent_ptr);
                if (err_code != rpc::error::OK())
                {
                    CO_RETURN err_code;
                }
            }
            rpc::shared_ptr<CHILD_INTERFACE> child_ptr;
            {
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
                CO_RETURN CO_AWAIT rpc::create_interface_stub(
                    *child_svc, child_ptr, parent_transport->get_adjacent_zone_id().as_caller(), output_descr);
            }
            CO_RETURN rpc::error::OK();
        };
    };

    template<class in_param_type, class out_param_type>
    CORO_TASK(int)
    service::connect_to_zone(const char* name,
        std::shared_ptr<transport> child_transport,
        const rpc::shared_ptr<in_param_type>& input_interface,
        rpc::shared_ptr<out_param_type>& output_interface)
    {
        // Marshal input interface if provided
        rpc::interface_descriptor input_descr{{0}, {0}};
        // Connect via transport (calls remote zone's entry point)
        rpc::interface_descriptor output_descr{{0}, {0}};

        int err_code = rpc::error::OK();

        if (input_interface)
        {
            add_transport(child_transport->get_adjacent_zone_id().as_destination(), child_transport);
            std::shared_ptr<object_stub> stub;
            auto factory = create_interface_stub(input_interface);
            err_code = CO_AWAIT get_proxy_stub_descriptor(rpc::get_version(),
                child_transport->get_adjacent_zone_id().as_caller_channel(),
                child_transport->get_adjacent_zone_id().as_caller(),
                input_interface.get(),
                factory,
                false,
                stub,
                input_descr);

            if (err_code != error::OK())
            {
                remove_transport(child_transport->get_adjacent_zone_id().as_destination());
                return err_code;
            }

            err_code = CO_AWAIT child_transport->connect(input_descr, output_descr);
            if (err_code != rpc::error::OK())
            {
                remove_transport(child_transport->get_adjacent_zone_id().as_destination());
                // Clean up on failure
                CO_RETURN err_code;
            }

            // Create service_proxy for this connection
            auto new_service_proxy = rpc::service_proxy::create(
                name, shared_from_this(), child_transport, child_transport->get_adjacent_zone_id().as_destination());

            // add the proxy to the service
            add_zone_proxy(new_service_proxy);
            // Demarshal output interface if provided
            if (output_descr.object_id != 0 && output_descr.destination_zone_id != 0)
            {
                err_code = CO_AWAIT rpc::demarshall_interface_proxy(
                    rpc::get_version(), new_service_proxy, output_descr, output_interface);
            }
        }

        CO_RETURN err_code;
    }

    // Attach remote zone - for peer-to-peer connections
    // Takes single transport since this is called by the remote peer during connection
    template<class PARENT_INTERFACE, class CHILD_INTERFACE>
    CORO_TASK(int)
    service::attach_remote_zone(const char* name,
        std::shared_ptr<transport> peer_transport,
        rpc::interface_descriptor input_descr,
        rpc::interface_descriptor& output_descr,
        std::function<CORO_TASK(int)(
            const rpc::shared_ptr<PARENT_INTERFACE>&, rpc::shared_ptr<CHILD_INTERFACE>&, const std::shared_ptr<rpc::service>&)> fn)
    {
        // Demarshal parent interface if provided
        rpc::shared_ptr<PARENT_INTERFACE> parent_ptr;
        if (input_descr != interface_descriptor())
        {
            // Create service_proxy for peer connection
            auto peer_service_proxy
                = rpc::service_proxy::create(name, shared_from_this(), peer_transport, input_descr.destination_zone_id);
            add_zone_proxy(peer_service_proxy);

            auto err_code = CO_AWAIT rpc::demarshall_interface_proxy(
                rpc::get_version(), peer_service_proxy, input_descr, parent_ptr);
            if (err_code != rpc::error::OK())
            {
                CO_RETURN err_code;
            }
        }

        // Call local entry point to create child interface
        rpc::shared_ptr<CHILD_INTERFACE> child_ptr;
        auto err_code = CO_AWAIT fn(parent_ptr, child_ptr, shared_from_this());
        if (err_code != rpc::error::OK())
        {
            CO_RETURN err_code;
        }

        // Marshal child interface to return to peer
        if (child_ptr)
        {
            RPC_ASSERT(child_ptr->is_local() && "Cannot support remote pointers from subordinate zones");
            output_descr = CO_AWAIT rpc::create_interface_stub(*this, child_ptr);
        }

        CO_RETURN rpc::error::OK();
    }

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
}
