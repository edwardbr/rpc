/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <filesystem>
#include <unordered_map>
#include <set>
#include <memory>
#include <shared_mutex>
// types.h is included via i_telemetry_service.h
#include <rpc/telemetry/i_telemetry_service.h>

namespace spdlog
{
    class logger;
}

namespace rpc
{
    class console_telemetry_service : public rpc::i_telemetry_service
    {
        mutable std::unordered_map<uint64_t, std::string> zone_names_;
        mutable std::shared_ptr<spdlog::logger> logger_;
        // Track zone relationships: zone_id -> set of child zones
        mutable std::unordered_map<uint64_t, std::set<uint64_t>> zone_children_;
        // Track zone relationships: zone_id -> parent zone (0 if root)
        mutable std::unordered_map<uint64_t, uint64_t> zone_parents_;

        // Thread safety: shared_mutex allows multiple concurrent readers with exclusive writers
        mutable std::shared_mutex zone_names_mutex_;
        mutable std::shared_mutex zone_children_mutex_;
        mutable std::shared_mutex zone_parents_mutex_;

        // Optional file output
        std::filesystem::path log_directory_;
        std::string test_suite_name_;
        std::string test_name_;
        mutable std::string logger_name_; // Store logger name for proper cleanup

        static constexpr size_t ASYNC_QUEUE_SIZE = 8192;

        std::string get_zone_name(uint64_t zone_id) const;
        std::string get_zone_color(uint64_t zone_id) const;
        std::string get_level_color(level_enum level) const;
        std::string reset_color() const;
        void register_zone_name(uint64_t zone_id, const char* name, bool optional_replace) const;
        void init_logger() const;
        void print_topology_diagram() const;
        void print_zone_tree(uint64_t zone_id, int depth) const;

        console_telemetry_service(
            const std::string& test_suite_name, const std::string& test_name, const std::filesystem::path& directory);

    public:
        static bool create(std::shared_ptr<rpc::i_telemetry_service>& service,
            const std::string& test_suite_name,
            const std::string& name,
            const std::filesystem::path& directory);

        virtual ~console_telemetry_service();
        console_telemetry_service();
        console_telemetry_service(const console_telemetry_service&) = delete;
        console_telemetry_service& operator=(const console_telemetry_service&) = delete;

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
