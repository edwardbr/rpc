/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <rpc/telemetry/i_telemetry_service.h>

namespace rpc
{
    class animation_telemetry_service : public rpc::i_telemetry_service
    {
    public:
        static bool create(std::shared_ptr<rpc::i_telemetry_service>& service,
            const std::string& test_suite_name,
            const std::string& name,
            const std::filesystem::path& directory);

        ~animation_telemetry_service() override;

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
        void on_service_proxy_deletion(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id) const override;
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
        void on_object_proxy_deletion(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id) const override;

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

    private:
        enum class field_kind
        {
            string,
            number,
            boolean,
            floating
        };

        struct event_field
        {
            std::string key;
            std::string value;
            field_kind type;
        };

        struct event_record
        {
            double timestamp = 0.0;
            std::string type;
            std::vector<event_field> fields;
        };

        animation_telemetry_service(std::filesystem::path output_path,
            std::string test_suite_name,
            std::string test_name);

        static std::string sanitize_name(const std::string& name);
        static std::string escape_json(const std::string& input);
        static event_field make_string_field(const std::string& key, const std::string& value);
        static event_field make_number_field(const std::string& key, uint64_t value);
        static event_field make_signed_field(const std::string& key, int64_t value);
        static event_field make_boolean_field(const std::string& key, bool value);
        static event_field make_floating_field(const std::string& key, double value);

        void record_event(const std::string& type, std::initializer_list<event_field> fields = {}) const;
        void record_event(const std::string& type, std::vector<event_field>&& fields) const;
        void write_output() const;

        inline double timestamp_now() const
        {
            auto now = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration<double>(now - start_time_);
            return delta.count();
        }

        mutable std::mutex mutex_;
        mutable std::vector<event_record> events_;
        mutable std::unordered_map<uint64_t, std::string> zone_names_;
        mutable std::unordered_map<uint64_t, uint64_t> zone_parents_;

        std::filesystem::path output_path_;
        std::string suite_name_;
        std::string test_name_;
        std::chrono::steady_clock::time_point start_time_;
    };
}
