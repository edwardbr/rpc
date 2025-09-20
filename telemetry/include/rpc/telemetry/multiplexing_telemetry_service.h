/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <filesystem>

#include <rpc/telemetry/i_telemetry_service.h>

#ifndef _IN_ENCLAVE
// Forward declare TestInfo to avoid including gtest in headers
namespace testing {
    class TestInfo;
}
#endif

namespace rpc
{
#ifndef _IN_ENCLAVE
    /**
     * @brief Configuration for creating telemetry services with test-specific names.
     */
    struct telemetry_service_config
    {
        std::string type;          // "console", "file", etc.
        std::string output_path;   // For file/console services that need a path

        telemetry_service_config(const std::string& t, const std::string& path = "")
            : type(t), output_path(path) {}
    };
#endif

    /**
     * @brief A telemetry service that forwards all telemetry events to multiple child services.
     *
     * This service implements i_telemetry_service and acts as a multiplexer, forwarding all
     * telemetry events to a configurable list of child telemetry services. This allows
     * running multiple telemetry backends simultaneously (e.g., console + file + custom).
     *
     * Non-enclave builds only.
     */
    class multiplexing_telemetry_service : public rpc::i_telemetry_service
    {
    private:
        std::vector<std::shared_ptr<i_telemetry_service>> children_;
#ifndef _IN_ENCLAVE
        std::vector<telemetry_service_config> service_configs_;
#endif

    public:
        /**
         * @brief Factory method to create a multiplexing telemetry service.
         *
         * @param service Output parameter for the created service
         * @param child_services Vector of child telemetry services to forward to
         * @return true if creation was successful, false otherwise
         */
        static bool create(std::vector<std::shared_ptr<i_telemetry_service>>&& child_services);

        /**
         * @brief Constructor with child services.
         *
         * @param child_services Vector of child telemetry services to forward to
         */
        explicit multiplexing_telemetry_service(std::vector<std::shared_ptr<i_telemetry_service>>&& child_services);

        virtual ~multiplexing_telemetry_service() {};
        multiplexing_telemetry_service(const multiplexing_telemetry_service&) = delete;
        multiplexing_telemetry_service& operator=(const multiplexing_telemetry_service&) = delete;

        /**
         * @brief Add a child telemetry service to forward events to.
         *
         * @param child The child service to add
         */
        void add_child(std::shared_ptr<i_telemetry_service> child);

        /**
         * @brief Get the number of child services.
         *
         * @return size_t Number of child services
         */
        size_t get_child_count() const;

        /**
         * @brief Clear all child services (for test cleanup).
         */
        void clear_children();

#ifndef _IN_ENCLAVE        
        /**
         * @brief Register a telemetry service configuration that will be created for each test.
         *
         * @param type Type of service ("console", "file", etc.)
         * @param output_path Optional output path for services that need it
         */
        void register_service_config(const std::string& type, const std::string& output_path = "");

        /**
         * @brief Reset for a new test - clear children and recreate them with current test info.
         *
         * @param test_info Current test information for creating services with correct names
         */
        void start_test(const char* test_suite_name, const char* name);
        
        /**
         * @brief Reset for a new test - clear children hem with current test info.
         */
        void reset_for_test();
#endif

        // i_telemetry_service interface - all methods forward to children
        void on_service_creation(const char* name, rpc::zone zone_id, rpc::destination_zone parent_zone_id) const override;
        void on_service_deletion(rpc::zone zone_id) const override;
        void on_service_try_cast(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_service_add_ref(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::add_ref_options options) const override;
        void on_service_release(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_zone caller_zone_id) const override;

        void on_service_proxy_creation(const char* service_name,
            const char* service_proxy_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id) const override;
        void on_cloned_service_proxy_creation(const char* service_name,
            const char* service_proxy_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id) const override;
        void on_service_proxy_deletion(
            rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const override;
        void on_service_proxy_try_cast(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_service_proxy_add_ref(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::add_ref_options options) const override;
        void on_service_proxy_release(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id) const override;
        void on_service_proxy_add_external_ref(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            int ref_count) const override;
        void on_service_proxy_release_external_ref(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            int ref_count) const override;

        void on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const override;
        void on_impl_deletion(uint64_t address, rpc::zone zone_id) const override;

        void on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const override;
        void on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const override;
        void on_stub_send(rpc::zone zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id) const override;
        void on_stub_add_ref(rpc::zone zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            uint64_t count,
            rpc::caller_zone caller_zone_id) const override;
        void on_stub_release(rpc::zone zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            uint64_t count,
            rpc::caller_zone caller_zone_id) const override;

        void on_object_proxy_creation(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            bool add_ref_done) const override;
        void on_object_proxy_deletion(
            rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const override;

        void on_interface_proxy_creation(const char* name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_deletion(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_send(const char* method_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id) const override;

        void message(level_enum level, const char* message) const override;
    };
}