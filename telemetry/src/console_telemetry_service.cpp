#include <iostream>
#include <thread>
#include <chrono>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <rpc/telemetry/console_telemetry_service.h>

namespace rpc
{
    console_telemetry_service::console_telemetry_service() = default;

    console_telemetry_service::~console_telemetry_service()
    {
        if (logger_) {
            // Flush any pending async messages - this blocks until complete
            logger_->flush();
            
            // Get reference to the thread pool before dropping logger
            auto tp = spdlog::thread_pool();
            
            // Remove logger from spdlog registry to prevent conflicts
            std::string logger_name = logger_->name();
            spdlog::drop(logger_name);
            
            // Drop the logger reference to allow proper cleanup
            logger_.reset();
            
            // Wait for thread pool to finish processing if it exists and has work
            if (tp) {
                // Wait for the queue to be empty - this is more deterministic than arbitrary sleep
                constexpr int max_wait_ms = 100;
                constexpr int check_interval_ms = 1;
                
                for (int waited = 0; waited < max_wait_ms; waited += check_interval_ms) {
                    if (tp->queue_size() == 0) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
                }
                
                // Small additional delay to ensure worker thread processes the empty queue check
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    void capitalise(std::string& name)
    {
        // Capitalize the entire name
        for (char& c : name)
        {
            if (c >= 'a' && c <= 'z')
            {
                c = c - 'a' + 'A';
            }
        }
    }

    std::string console_telemetry_service::get_zone_name(uint64_t zone_id) const
    {
        auto it = zone_names_.find(zone_id);
        if (it != zone_names_.end())
        {
            std::string name = it->second;
            // Capitalize the entire name
            capitalise(name);
            return "[" + name + " = " + std::to_string(zone_id) + "]";
        }
        return "[" + std::to_string(zone_id) + "]";
    }

    std::string console_telemetry_service::get_zone_color(uint64_t zone_id) const
    {
        // ANSI color codes - cycle through 8 bright colors
        const char* colors[] = {
            "\033[91m", // Bright Red
            "\033[92m", // Bright Green
            "\033[93m", // Bright Yellow
            "\033[94m", // Bright Blue
            "\033[95m", // Bright Magenta
            "\033[96m", // Bright Cyan
            "\033[97m", // Bright White
            "\033[90m"  // Bright Black (Gray)
        };
        return colors[zone_id % 8];
    }

    std::string console_telemetry_service::get_level_color(level_enum level) const
    {
        switch (level)
        {
        case warn:
            return "\033[93m"; // Bright Yellow
        case err:
            return "\033[91m"; // Bright Red
        case critical:
            return "\033[95m"; // Bright Magenta
        default:
            return ""; // No color for other levels
        }
    }

    std::string console_telemetry_service::reset_color() const
    {
        return "\033[0m"; // Reset to default color
    }

    void console_telemetry_service::init_logger() const
    {
        if (!logger_)
        {
            try
            {
                // Use unique logger name based on instance address to avoid conflicts
                std::string logger_name = "console_telemetry_" + std::to_string(reinterpret_cast<uintptr_t>(this));
                
                // Try to get existing logger first
                logger_ = spdlog::get(logger_name);
                if (!logger_) {
                    // Create async logger using spdlog's async factory
                    logger_ = spdlog::stdout_color_mt<spdlog::async_factory>(logger_name);
                }

                // Use raw pattern to preserve our custom ANSI formatting
                logger_->set_pattern("%v");
                logger_->set_level(spdlog::level::trace);
            }
            catch (...)
            {
                try {
                    // Fallback to synchronous logging if async fails
                    std::string fallback_name = "console_telemetry_sync_" + std::to_string(reinterpret_cast<uintptr_t>(this));
                    logger_ = spdlog::get(fallback_name);
                    if (!logger_) {
                        logger_ = spdlog::stdout_color_mt(fallback_name);
                    }
                    logger_->set_pattern("%v");
                } catch (...) {
                    // Last resort - use default logger
                    logger_ = spdlog::default_logger();
                }
            }
        }
    }

    void console_telemetry_service::register_zone_name(uint64_t zone_id, const char* name, bool optional_replace) const
    {
        auto it = zone_names_.find(zone_id);
        if (it != zone_names_.end())
        {
            if (it->second != name)
            {
                if (optional_replace)
                {
                    return;
                }
                init_logger();
                logger_->warn("\033[93mWARNING: Zone {} name changing from '{}' to '{}'\033[0m", zone_id, it->second, name);
            }
        }
        zone_names_[zone_id] = name;
    }

    bool console_telemetry_service::create(std::shared_ptr<rpc::i_telemetry_service>& service,
        const std::string& test_suite_name,
        const std::string& name,
        const std::filesystem::path& directory)
    {
        // Discard the parameters - console output doesn't need them
        (void)test_suite_name;
        (void)name;
        (void)directory;

        auto console_service = std::make_shared<console_telemetry_service>();
        console_service->init_logger();
        service = console_service;
        return true;
    }

    void console_telemetry_service::on_service_creation(const char* name, rpc::zone zone_id) const
    {
        register_zone_name(zone_id.id, name, false);
        init_logger();
        logger_->info("{}{} service_creation{}", get_zone_color(zone_id.id), get_zone_name(zone_id.id), reset_color());
        
        // Print topology diagram after each service creation
        print_topology_diagram();
    }
    
    void console_telemetry_service::on_child_zone_creation(const char* name, rpc::zone child_zone_id, rpc::destination_zone parent_zone_id) const
    {
        register_zone_name(child_zone_id.id, name, false);
        init_logger();
        logger_->info("{}{} child_zone_creation: parent={}{}", 
            get_zone_color(child_zone_id.id), get_zone_name(child_zone_id.id), 
            get_zone_name(parent_zone_id.get_val()), reset_color());
        
        // Track the parent-child relationship
        zone_children_[parent_zone_id.get_val()].insert(child_zone_id.id);
        zone_parents_[child_zone_id.id] = parent_zone_id.get_val();
        
        // Print topology diagram after child zone creation
        print_topology_diagram();
    }
    
    void console_telemetry_service::print_topology_diagram() const
    {
        init_logger();
        logger_->info("{}=== TOPOLOGY DIAGRAM ==={}", get_level_color(level_enum::info), reset_color());
        
        if (zone_names_.empty()) {
            logger_->info("{}No zones registered yet{}", get_level_color(level_enum::info), reset_color());
            return;
        }
        
        // Find root zones (zones with no parent)
        std::set<uint64_t> root_zones;
        for (const auto& zone_pair : zone_names_) {
            uint64_t zone_id = zone_pair.first;
            if (zone_parents_.find(zone_id) == zone_parents_.end() || zone_parents_.at(zone_id) == 0) {
                root_zones.insert(zone_id);
            }
        }
        
        if (root_zones.empty()) {
            // No parent-child relationships tracked yet, show flat list
            logger_->info("{}Active Zones (no hierarchy tracked yet):{}", get_level_color(level_enum::info), reset_color());
            for (const auto& zone_pair : zone_names_) {
                uint64_t zone_id = zone_pair.first;
                const std::string& zone_name = zone_pair.second;
                logger_->info("{}  Zone {}: {}{}", 
                    get_zone_color(zone_id), zone_id, zone_name, reset_color());
            }
        } else {
            // Show hierarchical structure
            logger_->info("{}Zone Hierarchy:{}", get_level_color(level_enum::info), reset_color());
            for (uint64_t root_zone : root_zones) {
                print_zone_tree(root_zone, 0);
            }
        }
        
        logger_->info("{}========================={}", get_level_color(level_enum::info), reset_color());
    }
    
    void console_telemetry_service::print_zone_tree(uint64_t zone_id, int depth) const
    {
        std::string indent(depth * 2, ' ');
        std::string branch = (depth > 0) ? "├─ " : "";
        
        auto zone_name_it = zone_names_.find(zone_id);
        std::string zone_name = (zone_name_it != zone_names_.end()) ? zone_name_it->second : "unknown";
        
        logger_->info("{}{}{}{}Zone {}: {} {}", 
            get_level_color(level_enum::info), indent, branch, reset_color(),
            get_zone_color(zone_id), zone_id, zone_name, reset_color());
        
        // Print children
        auto children_it = zone_children_.find(zone_id);
        if (children_it != zone_children_.end()) {
            for (uint64_t child_zone : children_it->second) {
                print_zone_tree(child_zone, depth + 1);
            }
        }
    }

    void console_telemetry_service::on_service_deletion(rpc::zone zone_id) const
    {
        init_logger();
        logger_->info("{}{} service_deletion{}", get_zone_color(zone_id.id), get_zone_name(zone_id.id), reset_color());
    }

    void console_telemetry_service::on_service_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        init_logger();
        logger_->info("{}{} service_try_cast: destination_zone={} caller_zone={} object_id={} interface_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            get_zone_name(caller_zone_id.id),
            object_id.id,
            interface_id.id,
            reset_color());
    }

    void console_telemetry_service::on_service_add_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::add_ref_options options) const
    {
        init_logger();
        logger_->info("{}{} service_add_ref: destination_channel_zone={} destination_zone={} object_id={} "
                      "caller_channel_zone={} caller_zone={} options={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_channel_zone_id.id),
            get_zone_name(destination_zone_id.id),
            object_id.id,
            get_zone_name(caller_channel_zone_id.id),
            get_zone_name(caller_zone_id.id),
            static_cast<int>(options),
            reset_color());
    }

    void console_telemetry_service::on_service_release(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_zone caller_zone_id) const
    {
        init_logger();
        logger_->info(
            "{}{} service_release: destination_channel_zone={} destination_zone={} object_id={} caller_zone={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_channel_zone_id.id),
            get_zone_name(destination_zone_id.id),
            object_id.id,
            get_zone_name(caller_zone_id.id),
            reset_color());
    }

    void console_telemetry_service::on_service_proxy_creation(const char* service_name,
        const char* service_proxy_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id) const
    {
        std::string uppercase_name(service_proxy_name);
        capitalise(uppercase_name);
        register_zone_name(zone_id.id, service_name, true);
        register_zone_name(destination_zone_id.id, service_proxy_name, true);
        init_logger();
        logger_->info("{}{} service_proxy_creation: name=[{}] destination_zone={} caller_zone={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            uppercase_name,
            get_zone_name(destination_zone_id.id),
            get_zone_name(caller_zone_id.id),
            reset_color());
    }

    void console_telemetry_service::on_cloned_service_proxy_creation(const char* service_name,
        const char* service_proxy_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id) const
    {
        std::string uppercase_name(service_proxy_name);
        capitalise(uppercase_name);
        register_zone_name(zone_id.id, service_name, true);
        register_zone_name(destination_zone_id.id, service_proxy_name, true);
        init_logger();
        logger_->info("{}{} cloned_service_proxy_creation: name=[{}] destination_zone={} caller_zone={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            uppercase_name,
            get_zone_name(destination_zone_id.id),
            get_zone_name(caller_zone_id.id),
            reset_color());
    }

    void console_telemetry_service::on_service_proxy_deletion(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
    {
        init_logger();
        logger_->info("{}{} service_proxy_deletion: destination_zone={} caller_zone={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            get_zone_name(caller_zone_id.id),
            reset_color());
    }

    void console_telemetry_service::on_service_proxy_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        init_logger();
        logger_->info("{}{} service_proxy_try_cast: destination_zone={} caller_zone={} object_id={} interface_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            get_zone_name(caller_zone_id.id),
            object_id.id,
            interface_id.id,
            reset_color());
    }

    void console_telemetry_service::on_service_proxy_add_ref(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::add_ref_options options) const
    {
        init_logger();
        logger_->info("{}{} service_proxy_add_ref: destination_zone={} destination_channel_zone={} caller_zone={} "
                      "object_id={} options={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            get_zone_name(destination_channel_zone_id.id),
            get_zone_name(caller_zone_id.id),
            object_id.id,
            static_cast<int>(options),
            reset_color());
    }

    void console_telemetry_service::on_service_proxy_release(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id) const
    {
        init_logger();
        logger_->info(
            "{}{} service_proxy_release: destination_zone={} destination_channel_zone={} caller_zone={} object_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            get_zone_name(destination_channel_zone_id.id),
            get_zone_name(caller_zone_id.id),
            object_id.id,
            reset_color());
    }

    void console_telemetry_service::on_service_proxy_add_external_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        init_logger();
        logger_->info("{}{} service_proxy_add_external_ref: destination_channel_zone={} destination_zone={} "
                      "caller_zone={} ref_count={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_channel_zone_id.id),
            get_zone_name(destination_zone_id.id),
            get_zone_name(caller_zone_id.id),
            ref_count,
            reset_color());
    }

    void console_telemetry_service::on_service_proxy_release_external_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        init_logger();
        logger_->info("{}{} service_proxy_release_external_ref: destination_channel_zone={} destination_zone={} "
                      "caller_zone={} ref_count={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_channel_zone_id.id),
            get_zone_name(destination_zone_id.id),
            get_zone_name(caller_zone_id.id),
            ref_count,
            reset_color());
    }

    void console_telemetry_service::on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const
    {
        init_logger();
        logger_->info("{}{} impl_creation: name={} address={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            name,
            address,
            reset_color());
    }

    void console_telemetry_service::on_impl_deletion(uint64_t address, rpc::zone zone_id) const
    {
        init_logger();
        logger_->info("{}{} impl_deletion: address={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            address,
            reset_color());
    }

    void console_telemetry_service::on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const
    {
        init_logger();
        logger_->info("{}{} stub_creation: object_id={} address={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            object_id.id,
            address,
            reset_color());
    }

    void console_telemetry_service::on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const
    {
        init_logger();
        logger_->info("{}{} stub_deletion: object_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            object_id.id,
            reset_color());
    }

    void console_telemetry_service::on_stub_send(
        rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        init_logger();
        logger_->info("{}{} stub_send: object_id={} interface_id={} method_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            object_id.id,
            interface_id.id,
            method_id.id,
            reset_color());
    }

    void console_telemetry_service::on_stub_add_ref(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        init_logger();
        logger_->info("{}{} stub_add_ref: object_id={} interface_id={} count={} caller_zone={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            object_id.id,
            interface_id.id,
            count,
            get_zone_name(caller_zone_id.id),
            reset_color());
    }

    void console_telemetry_service::on_stub_release(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        init_logger();
        logger_->info("{}{} stub_release: object_id={} interface_id={} count={} caller_zone={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            object_id.id,
            interface_id.id,
            count,
            get_zone_name(caller_zone_id.id),
            reset_color());
    }

    void console_telemetry_service::on_object_proxy_creation(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, bool add_ref_done) const
    {
        init_logger();
        logger_->info("{}{} object_proxy_creation: destination_zone={} object_id={} add_ref_done={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            object_id.id,
            (add_ref_done ? "true" : "false"),
            reset_color());
    }

    void console_telemetry_service::on_object_proxy_deletion(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const
    {
        init_logger();
        logger_->info("{}{} object_proxy_deletion: destination_zone={} object_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            object_id.id,
            reset_color());
    }

    void console_telemetry_service::on_interface_proxy_creation(const char* name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        init_logger();
        logger_->info("{}{} interface_proxy_creation: name={} destination_zone={} object_id={} interface_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            name,
            get_zone_name(destination_zone_id.id),
            object_id.id,
            interface_id.id,
            reset_color());
    }

    void console_telemetry_service::on_interface_proxy_deletion(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        init_logger();
        logger_->info("{}{} interface_proxy_deletion: destination_zone={} object_id={} interface_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            get_zone_name(destination_zone_id.id),
            object_id.id,
            interface_id.id,
            reset_color());
    }

    void console_telemetry_service::on_interface_proxy_send(const char* method_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id) const
    {
        init_logger();
        logger_->info(
            "{}{} interface_proxy_send: method_name={} destination_zone={} object_id={} interface_id={} method_id={}{}",
            get_zone_color(zone_id.id),
            get_zone_name(zone_id.id),
            method_name,
            get_zone_name(destination_zone_id.id),
            object_id.id,
            interface_id.id,
            method_id.id,
            reset_color());
    }

    void console_telemetry_service::message(level_enum level, const char* message) const
    {
        const char* level_str;
        switch (level)
        {
        case debug:
            level_str = "DEBUG";
            break;
        case trace:
            level_str = "TRACE";
            break;
        case info:
            level_str = "INFO";
            break;
        case warn:
            level_str = "WARN";
            break;
        case err:
            level_str = "ERROR";
            break;
        case critical:
            level_str = "CRITICAL";
            break;
        case off:
            return;
        default:
            level_str = "UNKNOWN";
            break;
        }

        init_logger();
        std::string level_color = get_level_color(level);
        if (!level_color.empty())
        {
            logger_->info("{}{} {}{}", level_color, level_str, message, reset_color());
        }
        else
        {
            logger_->info("{} {}", level_str, message);
        }
    }
}
