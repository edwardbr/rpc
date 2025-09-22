/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <rpc/telemetry/multiplexing_telemetry_service.h>

#ifndef _IN_ENCLAVE
#include <rpc/telemetry/console_telemetry_service.h>
#include <rpc/telemetry/sequence_diagram_telemetry_service.h>
#include <rpc/telemetry/animation_telemetry_service.h>
#endif

namespace rpc
{
    bool multiplexing_telemetry_service::create(std::vector<std::shared_ptr<i_telemetry_service>>&& child_services)
    {
        // Allow empty multiplexer - services can be added later
        rpc::telemetry_service_ = std::make_shared<multiplexing_telemetry_service>(std::move(child_services));
        return true;
    }

    multiplexing_telemetry_service::multiplexing_telemetry_service(
        std::vector<std::shared_ptr<i_telemetry_service>>&& child_services)
        : children_(std::move(child_services))
    {
    }

    void multiplexing_telemetry_service::add_child(std::shared_ptr<i_telemetry_service> child)
    {
        if (child)
        {
            children_.push_back(child);
        }
    }

    size_t multiplexing_telemetry_service::get_child_count() const
    {
        return children_.size();
    }

    void multiplexing_telemetry_service::clear_children()
    {
        children_.clear();
    }

#ifndef _IN_ENCLAVE
    void multiplexing_telemetry_service::register_service_config(const std::string& type, const std::string& output_path)
    {
        service_configs_.emplace_back(type, output_path);
    }

    void multiplexing_telemetry_service::start_test(const char* test_suite_name, const char* name)
    {
        // Clear existing children
        children_.clear();

        // Recreate services based on cached configurations with current test info
        for (const auto& config : service_configs_)
        {
            std::shared_ptr<i_telemetry_service> service;

            if (config.type == "console")
            {
                if (rpc::console_telemetry_service::create(service, test_suite_name, name, config.output_path))
                {
                    children_.push_back(service);
                }
            }
            else if (config.type == "sequence")
            {
                if (rpc::sequence_diagram_telemetry_service::create(service, test_suite_name, name, config.output_path))
                {
                    children_.push_back(service);
                }
            }
            else if (config.type == "animation")
            {
                if (rpc::animation_telemetry_service::create(service, test_suite_name, name, config.output_path))
                {
                    children_.push_back(service);
                }
            }
            // Add other service types here as needed (file, custom, etc.)
        }
    }

    void multiplexing_telemetry_service::reset_for_test()
    {
        // Clear existing children
        children_.clear();
    }
#endif

    // Forward all telemetry events to children
    void multiplexing_telemetry_service::on_service_creation(
        const char* name, rpc::zone zone_id, rpc::destination_zone parent_zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_creation(name, zone_id, parent_zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_deletion(rpc::zone zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_deletion(zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_try_cast(zone_id, destination_zone_id, caller_zone_id, object_id, interface_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_add_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::add_ref_options options) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_add_ref(zone_id,
                    destination_channel_zone_id,
                    destination_zone_id,
                    object_id,
                    caller_channel_zone_id,
                    caller_zone_id,
                    options);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_release(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_zone caller_zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_release(
                    zone_id, destination_channel_zone_id, destination_zone_id, object_id, caller_zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_proxy_creation(const char* service_name,
        const char* service_proxy_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_proxy_creation(
                    service_name, service_proxy_name, zone_id, destination_zone_id, caller_zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_cloned_service_proxy_creation(const char* service_name,
        const char* service_proxy_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_cloned_service_proxy_creation(
                    service_name, service_proxy_name, zone_id, destination_zone_id, caller_zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_proxy_deletion(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_proxy_deletion(zone_id, destination_zone_id, caller_zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_proxy_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_proxy_try_cast(zone_id, destination_zone_id, caller_zone_id, object_id, interface_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_proxy_add_ref(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::add_ref_options options) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_proxy_add_ref(
                    zone_id, destination_zone_id, destination_channel_zone_id, caller_zone_id, object_id, options);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_proxy_release(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_proxy_release(
                    zone_id, destination_zone_id, destination_channel_zone_id, caller_zone_id, object_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_proxy_add_external_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_proxy_add_external_ref(
                    zone_id, destination_channel_zone_id, destination_zone_id, caller_zone_id, ref_count);
            }
        }
    }

    void multiplexing_telemetry_service::on_service_proxy_release_external_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_service_proxy_release_external_ref(
                    zone_id, destination_channel_zone_id, destination_zone_id, caller_zone_id, ref_count);
            }
        }
    }

    void multiplexing_telemetry_service::on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_impl_creation(name, address, zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_impl_deletion(uint64_t address, rpc::zone zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_impl_deletion(address, zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_stub_creation(zone_id, object_id, address);
            }
        }
    }

    void multiplexing_telemetry_service::on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_stub_deletion(zone_id, object_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_stub_send(
        rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_stub_send(zone_id, object_id, interface_id, method_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_stub_add_ref(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_stub_add_ref(zone_id, object_id, interface_id, count, caller_zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_stub_release(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_stub_release(zone_id, object_id, interface_id, count, caller_zone_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_object_proxy_creation(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, bool add_ref_done) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_object_proxy_creation(zone_id, destination_zone_id, object_id, add_ref_done);
            }
        }
    }

    void multiplexing_telemetry_service::on_object_proxy_deletion(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_object_proxy_deletion(zone_id, destination_zone_id, object_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_interface_proxy_creation(const char* name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_interface_proxy_creation(name, zone_id, destination_zone_id, object_id, interface_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_interface_proxy_deletion(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_interface_proxy_deletion(zone_id, destination_zone_id, object_id, interface_id);
            }
        }
    }

    void multiplexing_telemetry_service::on_interface_proxy_send(const char* method_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->on_interface_proxy_send(
                    method_name, zone_id, destination_zone_id, object_id, interface_id, method_id);
            }
        }
    }

    void multiplexing_telemetry_service::message(level_enum level, const char* message) const
    {
        for (const auto& child : children_)
        {
            if (child)
            {
                child->message(level, message);
            }
        }
    }
}
