/*
 * Copyright (c) 2025 Edward Boggis-Rolfe
 * All rights reserved.
 */

// Autonomous Instruction-Based Fuzz Test for RPC++ Zone Hierarchies
// Tests autonomous nodes executing instruction sets independently to build specific graph topologies.

// Standard C++ headers
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

// RPC headers
#include <rpc/rpc.h>
#include "rpc/service_proxies/basic_service_proxies.h"
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

// Other headers
#include <args.hxx>
#include <spdlog/spdlog.h>
#include <yas/types/std/string.hpp>
#include <yas/types/std/vector.hpp>
#include "fuzz_test/fuzz_test.h"
#include "fuzz_test/fuzz_test_stub.h"

#ifdef USE_RPC_TELEMETRY
// Use extern declaration to reference the global telemetry service manager from test_fixtures
extern rpc::telemetry_service_manager telemetry_service_manager_;
#endif

using namespace fuzz_test;

std::atomic<uint64_t> g_zone_id_counter;
std::atomic<int> g_instruction_counter;

// Global configuration for replay system
static std::string g_output_directory = "tests/fuzz_test/replays";
static bool g_cleanup_successful_tests = true;

// Global deterministic random number generator for replay consistency
static std::mt19937 g_global_rng;
static bool g_global_rng_initialized = false;

// Initialize global RNG with a seed (for replay) or random seed (for normal execution)
void initialize_global_rng(uint64_t seed = 0)
{
    if (seed == 0)
    {
        std::random_device rd;
        seed = rd();
        RPC_INFO("Using random seed: {}", seed);
    }
    else
    {
        RPC_INFO("Using deterministic seed: {}", seed);
    }
    g_global_rng.seed((unsigned int)seed);
    std::srand(static_cast<unsigned int>(seed)); // Also seed C-style rand() for compatibility
    g_global_rng_initialized = true;
}

// Get reference to global RNG (ensures it's initialized)
std::mt19937& get_global_rng()
{
    if (!g_global_rng_initialized)
    {
        initialize_global_rng(); // Initialize with random seed if not set
    }
    return g_global_rng;
}

// Use IDL-generated structures instead of manual ones
using test_scenario_config = fuzz_test::test_scenario_config;
using runner_target_pair = fuzz_test::runner_target_pair;

// Function to dump test scenario for replay using YAS JSON serialization
void dump_test_scenario(const test_scenario_config& config, const std::string& status = "STARTING");
void dump_failure_scenario(const test_scenario_config& config, const std::string& error_msg);
test_scenario_config load_test_scenario(const std::string& scenario_file);
int replay_test_scenario(const std::string& scenario_file);
void cleanup_successful_test(const test_scenario_config& config);

// Forward declarations
std::vector<instruction> generate_instruction_set(int max_instructions, bool has_parent, bool has_children);

// Shared object implementation
class shared_object_impl : public i_shared_object, public i_cleanup, public rpc::enable_shared_from_this<shared_object_impl>
{
private:
    int id_;
    std::string name_;
    int value_;
    int test_count_;
    bool cleanup_called_;

    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<i_shared_object>(interface_id))
            return static_cast<const i_shared_object*>(this);
        if (rpc::match<i_cleanup>(interface_id))
            return static_cast<const i_cleanup*>(this);
        return nullptr;
    }

public:
    shared_object_impl(int id, const std::string& name, int initial_value)
        : id_(id)
        , name_(name)
        , value_(initial_value)
        , test_count_(0)
        , cleanup_called_(false)
    {
    }

    CORO_TASK(int) test_function(int input_value) override
    {
        RPC_INFO("[SHARED_OBJECT id={}] test_function(input_value={})", id_, input_value);
        test_count_++;
        CO_RETURN input_value * 2 + test_count_;
    }

    CORO_TASK(int) get_stats(int& count) override
    {
        count = test_count_;
        RPC_INFO("[SHARED_OBJECT id={}] get_stats() -> count={}", id_, count);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) set_value(int new_value) override
    {
        RPC_INFO("[SHARED_OBJECT id={}] set_value(new_value={}) old_value={}", id_, new_value, value_);
        value_ = new_value;
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) get_value(int& value) override
    {
        value = value_;
        RPC_INFO("[SHARED_OBJECT id={}] get_value() -> value={}", id_, value);
        CO_RETURN rpc::error::OK();
    }

    // i_cleanup implementation
    CORO_TASK(int) cleanup(rpc::shared_ptr<i_garbage_collector> collector) override
    {
        std::ignore = collector;
        RPC_INFO("[SHARED_OBJECT id={} name={}] cleanup() called, already_cleaned={}", id_, name_, cleanup_called_);
        if (cleanup_called_)
        {
            CO_RETURN rpc::error::OK(); // Already cleaned up
        }
        cleanup_called_ = true;

        RPC_INFO("[SHARED_OBJECT id={} name={}] cleanup completed", id_, name_);
        CO_RETURN rpc::error::OK();
    }
};

// Factory implementation
class factory_impl : public i_fuzz_factory, public i_cleanup, public rpc::enable_shared_from_this<factory_impl>
{
private:
    int objects_created_;
    bool cleanup_called_;

    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<i_fuzz_factory>(interface_id))
            return static_cast<const i_fuzz_factory*>(this);
        if (rpc::match<i_cleanup>(interface_id))
            return static_cast<const i_cleanup*>(this);
        return nullptr;
    }

public:
    factory_impl()
        : objects_created_(0)
        , cleanup_called_(false)
    {
    }

    CORO_TASK(int) create_shared_object(
        int id, std::string name, int initial_value, rpc::shared_ptr<i_shared_object>& created_object) override
    {
        RPC_INFO("[FACTORY] create_shared_object(id={}, name={}, initial_value={})", id, name, initial_value);
        try
        {
            auto obj = rpc::make_shared<shared_object_impl>(id, name, initial_value);
            created_object = rpc::static_pointer_cast<i_shared_object>(obj);
            objects_created_++;
            RPC_INFO("[FACTORY] create_shared_object completed, total_created={}", objects_created_);
            CO_RETURN rpc::error::OK();
        }
        catch (...)
        {
            RPC_INFO("[FACTORY] create_shared_object failed with exception");
            CO_RETURN rpc::error::OUT_OF_MEMORY();
        }
    }

    CORO_TASK(int) place_shared_object(rpc::shared_ptr<i_shared_object> new_object, rpc::shared_ptr<i_shared_object> target_object) override
    {
        RPC_INFO("[FACTORY] place_shared_object() called");
        if (!new_object || !target_object)
        {
            RPC_INFO("[FACTORY] place_shared_object failed: null objects");
            CO_RETURN rpc::error::INVALID_DATA();
        }

        int new_value = 0, target_value = 0;
        new_object->get_value(new_value);
        target_object->get_value(target_value);
        target_object->set_value(new_value + target_value);

        RPC_INFO("[FACTORY] place_shared_object completed with combined value {}", new_value + target_value);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) get_factory_stats(int& total_created, int& current_refs) override
    {
        total_created = objects_created_;
        current_refs = 0; // Simplified
        RPC_INFO("[FACTORY] get_factory_stats() -> total_created={}, current_refs={}", total_created, current_refs);
        CO_RETURN rpc::error::OK();
    }

    // i_cleanup implementation
    CORO_TASK(int) cleanup(rpc::shared_ptr<i_garbage_collector> collector) override
    {
        std::ignore = collector;
        RPC_INFO("[FACTORY] cleanup() called, already_cleaned={}, objects_created={}", cleanup_called_, objects_created_);
        if (cleanup_called_)
        {
            CO_RETURN rpc::error::OK(); // Already cleaned up
        }
        cleanup_called_ = true;

        RPC_INFO("[FACTORY] cleanup completed (created {} objects)", objects_created_);
        CO_RETURN rpc::error::OK();
    }
};

// Cache implementation
class cache_impl : public i_fuzz_cache, public i_cleanup, public rpc::enable_shared_from_this<cache_impl>
{
private:
    std::map<int, rpc::shared_ptr<i_shared_object>> cache_storage_;
    bool cleanup_called_;

    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<i_fuzz_cache>(interface_id))
            return static_cast<const i_fuzz_cache*>(this);
        if (rpc::match<i_cleanup>(interface_id))
            return static_cast<const i_cleanup*>(this);
        return nullptr;
    }

public:
    cache_impl()
        : cleanup_called_(false)
    {
    }

    CORO_TASK(int) store_object(int cache_key, rpc::shared_ptr<i_shared_object> object) override
    {
        RPC_INFO("[CACHE] store_object(cache_key={})", cache_key);
        if (!object)
        {
            RPC_INFO("[CACHE] store_object failed: null object");
            CO_RETURN rpc::error::INVALID_DATA();
        }

        cache_storage_[cache_key] = object;
        RPC_INFO("[CACHE] store_object completed, cache_size={}", cache_storage_.size());
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) retrieve_object(int cache_key, rpc::shared_ptr<i_shared_object>& object) override
    {
        RPC_INFO("[CACHE] retrieve_object(cache_key={})", cache_key);
        auto it = cache_storage_.find(cache_key);
        if (it != cache_storage_.end())
        {
            object = it->second;
            RPC_INFO("[CACHE] retrieve_object found object");
            CO_RETURN rpc::error::OK();
        }

        object.reset();
        RPC_INFO("[CACHE] retrieve_object not found");
        CO_RETURN rpc::error::OBJECT_NOT_FOUND();
    }

    CORO_TASK(int) has_object(int cache_key, bool& exists) override
    {
        exists = cache_storage_.find(cache_key) != cache_storage_.end();
        RPC_INFO("[CACHE] has_object(cache_key={}) -> exists={}", cache_key, exists);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) get_cache_size(int& size) override
    {
        size = static_cast<int>(cache_storage_.size());
        RPC_INFO("[CACHE] get_cache_size() -> size={}", size);
        CO_RETURN rpc::error::OK();
    }

    // i_cleanup implementation
    CORO_TASK(int) cleanup(rpc::shared_ptr<i_garbage_collector> collector) override
    {
        RPC_INFO("[CACHE] cleanup() called, already_cleaned={}, cache_size={}", cleanup_called_, cache_storage_.size());
        if (cleanup_called_)
        {
            CO_RETURN rpc::error::OK(); // Already cleaned up
        }
        cleanup_called_ = true;

        // Cleanup all cached objects first
        for (auto& pair : cache_storage_)
        {
            RPC_INFO("[CACHE] cleaning cached object with key={}", pair.first);
            auto cleanup_obj = CO_AWAIT rpc::dynamic_pointer_cast<i_cleanup>(pair.second);
            if (cleanup_obj)
            {
                cleanup_obj->cleanup(collector);
                collector->collect(cleanup_obj);
            }
        }

        // Clear cache to prevent circular dependencies
        cache_storage_.clear();

        RPC_INFO("[CACHE] cleanup completed");
        CO_RETURN rpc::error::OK();
    }
};

// Worker implementation
class worker_impl : public i_fuzz_worker, public i_cleanup, public rpc::enable_shared_from_this<worker_impl>
{
private:
    int objects_processed_;
    int total_increments_;
    bool cleanup_called_;

    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<i_fuzz_worker>(interface_id))
            return static_cast<const i_fuzz_worker*>(this);
        if (rpc::match<i_cleanup>(interface_id))
            return static_cast<const i_cleanup*>(this);
        return nullptr;
    }

public:
    worker_impl()
        : objects_processed_(0)
        , total_increments_(0)
        , cleanup_called_(false)
    {
    }

    CORO_TASK(int) process_object(rpc::shared_ptr<i_shared_object> object, int increment) override
    {
        RPC_INFO("[WORKER] process_object(increment={})", increment);
        if (!object)
        {
            RPC_INFO("[WORKER] process_object failed: null object");
            CO_RETURN rpc::error::INVALID_DATA();
        }

        int current_value;
        auto get_result = CO_AWAIT object->get_value(current_value);
        if (get_result == rpc::error::OK())
        {
            auto set_result = object->set_value(current_value + increment);
            if (set_result == rpc::error::OK())
            {
                objects_processed_++;
                total_increments_ += increment;
                RPC_INFO("[WORKER] process_object completed, new value: {}, processed_count={}",
                    current_value + increment,
                    objects_processed_);
                CO_RETURN rpc::error::OK();
            }
        }

        RPC_INFO("[WORKER] process_object failed during value operations");
        CO_RETURN rpc::error::INVALID_DATA();
    }

    CORO_TASK(int) get_worker_stats(int& objects_processed, int& total_increments) override
    {
        objects_processed = objects_processed_;
        total_increments = total_increments_;
        RPC_INFO("[WORKER] get_worker_stats() -> objects_processed={}, total_increments={}",
            objects_processed,
            total_increments);
        CO_RETURN rpc::error::OK();
    }

    // i_cleanup implementation
    CORO_TASK(int) cleanup(rpc::shared_ptr<i_garbage_collector> collector) override
    {
        std::ignore = collector;
        RPC_INFO(
            "[WORKER] cleanup() called, already_cleaned={}, objects_processed={}", cleanup_called_, objects_processed_);
        if (cleanup_called_)
        {
            CO_RETURN rpc::error::OK(); // Already cleaned up
        }
        cleanup_called_ = true;

        RPC_INFO("[WORKER] cleanup completed (processed {} objects)", objects_processed_);
        CO_RETURN rpc::error::OK();
    }
};

// Fully autonomous node implementation
class autonomous_node_impl : public i_autonomous_node,
                             public i_cleanup,
                             public rpc::enable_shared_from_this<autonomous_node_impl>
{
private:
    node_type node_type_;
    uint64_t node_id_;
    int connections_count_;
    int signals_received_;
    bool cleanup_called_;

    // Parent node for hierarchy navigation
    rpc::shared_ptr<i_autonomous_node> parent_node_;

    // Created child nodes and objects for instruction execution
    std::vector<rpc::shared_ptr<i_autonomous_node>> child_nodes_;
    std::vector<rpc::shared_ptr<i_shared_object>> created_objects_;
    rpc::shared_ptr<i_fuzz_factory> local_factory_;
    rpc::shared_ptr<i_fuzz_cache> local_cache_;
    rpc::shared_ptr<i_fuzz_worker> local_worker_;

    // Required casting_interface methods
    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<i_autonomous_node>(interface_id))
            return static_cast<const i_autonomous_node*>(this);
        if (rpc::match<i_cleanup>(interface_id))
            return static_cast<const i_cleanup*>(this);
        return nullptr;
    }

    void create_local_factory()
    {
        if (local_factory_)
            return; // Already created
        auto current_service = rpc::service::get_current_service();
        if (!current_service)
            return;

        auto zone_id = ++g_zone_id_counter;
        std::string factory_zone_name = "factory_" + std::to_string(node_id_) + "_" + std::to_string(zone_id);

        current_service->connect_to_zone<rpc::local_child_service_proxy<i_fuzz_factory, i_fuzz_factory>>(
            factory_zone_name.c_str(),
            {zone_id},
            rpc::shared_ptr<i_fuzz_factory>(),
            local_factory_,
            [](const rpc::shared_ptr<i_fuzz_factory>&,
                rpc::shared_ptr<i_fuzz_factory>& new_factory,
                const rpc::shared_ptr<rpc::service>& child_service_ptr) -> int
            {
                fuzz_test_idl_register_stubs(child_service_ptr);
                new_factory = rpc::make_shared<factory_impl>();
                return rpc::error::OK();
            });
    }

    void create_local_cache()
    {
        if (local_cache_)
            return;
        auto current_service = rpc::service::get_current_service();
        if (!current_service)
            return;

        auto zone_id = ++g_zone_id_counter;
        std::string cache_zone_name = "cache_" + std::to_string(node_id_) + "_" + std::to_string(zone_id);

        current_service->connect_to_zone<rpc::local_child_service_proxy<i_fuzz_cache, i_fuzz_cache>>(
            cache_zone_name.c_str(),
            {zone_id},
            rpc::shared_ptr<i_fuzz_cache>(),
            local_cache_,
            [](const rpc::shared_ptr<i_fuzz_cache>&,
                rpc::shared_ptr<i_fuzz_cache>& new_cache,
                const rpc::shared_ptr<rpc::service>& child_service_ptr) -> int
            {
                fuzz_test_idl_register_stubs(child_service_ptr);
                new_cache = rpc::make_shared<cache_impl>();
                CO_RETURN rpc::error::OK();
            });
    }

    void create_local_worker()
    {
        if (local_worker_)
            return;
        auto current_service = rpc::service::get_current_service();
        if (!current_service)
            return;

        auto zone_id = ++g_zone_id_counter;
        std::string worker_zone_name = "worker_" + std::to_string(node_id_) + "_" + std::to_string(zone_id);

        current_service->connect_to_zone<rpc::local_child_service_proxy<i_fuzz_worker, i_fuzz_worker>>(
            worker_zone_name.c_str(),
            {zone_id},
            rpc::shared_ptr<i_fuzz_worker>(),
            local_worker_,
            [](const rpc::shared_ptr<i_fuzz_worker>&,
                rpc::shared_ptr<i_fuzz_worker>& new_worker,
                const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
            {
                fuzz_test_idl_register_stubs(child_service_ptr);
                new_worker = rpc::make_shared<worker_impl>();
                return rpc::error::OK();
            });
    }

    rpc::shared_ptr<i_shared_object> create_shared_object_via_factory(int object_id)
    {
        if (!local_factory_)
        {
            RPC_INFO("Node {} cannot create object - no factory available", node_id_);
            return nullptr;
        }

        rpc::shared_ptr<i_shared_object> new_obj;
        std::string obj_name = "obj_" + std::to_string(node_id_) + "_" + std::to_string(object_id);

        if (local_factory_->create_shared_object(object_id, obj_name, object_id * 10, new_obj) == rpc::error::OK() && new_obj)
        {
            created_objects_.push_back(new_obj);
            return new_obj;
        }
        return nullptr;
    }

    void store_object_in_cache(int cache_key, rpc::shared_ptr<i_shared_object> object)
    {
        if (!local_cache_)
            return;
        if (!object && !created_objects_.empty())
        {
            object = created_objects_[cache_key % created_objects_.size()];
        }
        if (object)
        {
            local_cache_->store_object(cache_key, object);
        }
    }

    void process_object_via_worker(int increment, rpc::shared_ptr<i_shared_object> object)
    {
        if (!local_worker_)
            return;
        if (!object && !created_objects_.empty())
        {
            object = created_objects_[increment % created_objects_.size()];
        }
        if (object)
        {
            local_worker_->process_object(object, increment);
        }
    }

public:
    rpc::shared_ptr<i_autonomous_node> fork_child_node()
    {
        rpc::shared_ptr<i_autonomous_node> child_node;
        auto child_zone_id = ++g_zone_id_counter;
        if (create_child_node(node_type::WORKER_NODE, child_zone_id, true, child_node) == rpc::error::OK() && child_node)
        {
            child_nodes_.push_back(child_node);
            RPC_INFO("Node {} FORKED child node in zone {}", node_id_, child_zone_id);
            return child_node;
        }
        return nullptr;
    }

    autonomous_node_impl(node_type type, uint64_t node_id)
        : node_type_(type)
        , node_id_(node_id)
        , connections_count_(0)
        , signals_received_(0)
        , cleanup_called_(false)
    {
    }

    CORO_TASK(int) initialize_node(node_type type, uint64_t node_id) override
    {
        RPC_INFO("[NODE {}] initialize_node(type={}, node_id={})", node_id_, static_cast<int>(type), node_id);
        node_type_ = type;
        node_id_ = node_id;
        RPC_INFO("[NODE {}] initialize_node completed", node_id_);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) run_script(rpc::shared_ptr<i_autonomous_node> target_node, int instruction_count) override
    {
        if (!target_node)
        {
            RPC_INFO("Node {} cannot run script: no target specified.", node_id_);
            CO_RETURN rpc::error::INVALID_DATA();
        }

        [[maybe_unused]] uint64_t target_id = 0;
        if (target_node)
        {
            node_type type;
            uint64_t id;
            int conn, obj_held;
            target_node->get_node_status(type, id, conn, obj_held);
            target_id = id;
        }

        RPC_INFO("Node {} starting script execution targeting node {}.", node_id_, target_id);

        bool has_children = !child_nodes_.empty();
        bool has_parent = (parent_node_ != nullptr);
        auto instructions = generate_instruction_set(instruction_count, has_parent, has_children);

        rpc::shared_ptr<i_shared_object> current_object;
        for (const auto& instruction : instructions)
        {
            if (g_instruction_counter >= 50)
            {
                RPC_INFO("Instruction limit reached. Halting script on node {}.", node_id_);
                break;
            }

            // Replace PASS_TO... operations to always use the assigned target
            if (instruction.operation.rfind("PASS_TO", 0) == 0)
            {
                target_node->receive_object(current_object, node_id_);
            }
            else
            {
                rpc::shared_ptr<i_shared_object> output_object;
                execute_instruction(instruction, current_object, output_object);
                if (output_object)
                {
                    current_object = output_object;
                }
            }
        }
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) execute_instruction(instruction instruction,
        rpc::shared_ptr<i_shared_object> input_object,
        rpc::shared_ptr<i_shared_object>& output_object) override
    {
        if (g_instruction_counter >= 50)
        {
            CO_RETURN rpc::error::OK(); // Stop execution gracefully
        }
        g_instruction_counter++;
        RPC_INFO("Node {} executing: {} (val={}) [Count={}]",
            node_id_,
            instruction.operation,
            instruction.target_value,
            g_instruction_counter.load());
        output_object = input_object; // Default pass-through

        try
        {
            if (instruction.operation == "CREATE_CAPABILITY")
            {
                switch (instruction.target_value % 3)
                {
                case 0:
                    create_local_factory();
                    break;
                case 1:
                    create_local_cache();
                    break;
                case 2:
                    create_local_worker();
                    break;
                }
            }
            else if (instruction.operation == "CREATE_OBJECT")
            {
                output_object = create_shared_object_via_factory(instruction.target_value);
            }
            else if (instruction.operation == "STORE_OBJECT")
            {
                store_object_in_cache(instruction.target_value, input_object);
            }
            else if (instruction.operation == "PROCESS_OBJECT")
            {
                process_object_via_worker(instruction.target_value, input_object);
            }
            else if (instruction.operation == "FORK_CHILD")
            {
                fork_child_node();
            }
            else if (instruction.operation == "PASS_TO_RANDOM_CHILD")
            {
                if (!child_nodes_.empty())
                {
                    std::mt19937& gen = get_global_rng();
                    std::uniform_int_distribution<size_t> dist(0, child_nodes_.size() - 1);
                    auto& target_child = child_nodes_[dist(gen)];
                    target_child->receive_object(input_object, node_id_);
                }
            }
            else if (instruction.operation == "PASS_TO_PARENT")
            {
                if (parent_node_)
                {
                    parent_node_->receive_object(input_object, node_id_);
                }
            }
            else if (instruction.operation == "PASS_TO_RANDOM_SIBLING")
            {
                if (parent_node_)
                {
                    int sibling_count = 0;
                    parent_node_->get_cached_children_count(sibling_count);
                    if (sibling_count > 1)
                    {
                        rpc::shared_ptr<i_autonomous_node> target_sibling;
                        std::mt19937& gen = get_global_rng();
                        std::uniform_int_distribution<int> dist(0, sibling_count - 1);

                        do
                        {
                            int index = dist(gen);
                            parent_node_->get_cached_child_by_index(index, target_sibling);

                            if (target_sibling)
                            {
                                node_type type;
                                uint64_t id;
                                int conn, obj_held;
                                target_sibling->get_node_status(type, id, conn, obj_held);
                                if (static_cast<uint64_t>(id) == node_id_)
                                {
                                    target_sibling.reset();
                                }
                            }
                        } while (!target_sibling);

                        target_sibling->receive_object(input_object, node_id_);
                    }
                }
            }
            else
            {
                RPC_WARNING("Node {} unknown instruction: {}", node_id_, instruction.operation);
                CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }
        catch (...)
        {
            spdlog::error("Node {} exception executing instruction", node_id_);
            CO_RETURN rpc::error::EXCEPTION();
        }
    }

    CORO_TASK(int) receive_object(rpc::shared_ptr<i_shared_object> object, uint64_t sender_node_id) override
    {
        std::ignore = object;
        std::ignore = sender_node_id;
        RPC_INFO("[NODE {}] receive_object from sender_node_id={}", node_id_, sender_node_id);
        signals_received_++;
        RPC_INFO("[NODE {}] receive_object completed, total signals: {}", node_id_, signals_received_);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) get_node_status(node_type& current_type, uint64_t& current_id, int& connections_count, int& objects_held) override
    {
        current_type = node_type_;
        current_id = node_id_;
        connections_count = connections_count_;
        objects_held = signals_received_;
        RPC_INFO("[NODE {}] get_node_status() -> type={}, id={}, connections={}, objects_held={}",
            node_id_,
            static_cast<int>(current_type),
            current_id,
            connections_count,
            objects_held);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) create_child_node(
        node_type child_type, uint64_t child_zone_id, bool cache_locally, rpc::shared_ptr<i_autonomous_node>& child_node) override
    {
        std::ignore = cache_locally;
        std::ignore = child_node;
        RPC_INFO("[NODE {}] create_child_node(child_type={}, child_zone_id={}, cache_locally={})",
            node_id_,
            static_cast<int>(child_type),
            child_zone_id,
            cache_locally);
        try
        {
            auto current_service = rpc::service::get_current_service();
            if (!current_service)
            {
                spdlog::error("[NODE {}] create_child_node failed: ZONE_NOT_INITIALISED", node_id_);
                CO_RETURN rpc::error::ZONE_NOT_INITIALISED();
            }

            std::string child_zone_name = "child_" + std::to_string(node_id_) + "_" + std::to_string(child_zone_id);
            RPC_INFO("[NODE {}] create_child_node creating zone: {}", node_id_, child_zone_name);
            auto self = rpc::static_pointer_cast<i_autonomous_node>(shared_from_this());

            auto result
                = current_service->connect_to_zone<rpc::local_child_service_proxy<i_autonomous_node, i_autonomous_node>>(
                    child_zone_name.c_str(),
                    {child_zone_id},
                    self,
                    child_node,
                    [=](const rpc::shared_ptr<i_autonomous_node>& parent,
                        rpc::shared_ptr<i_autonomous_node>& new_child,
                        const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
                    {
                        RPC_INFO("[NODE {}] setup callback for child zone {} starting", node_id_, child_zone_id);
                        fuzz_test_idl_register_stubs(child_service_ptr);
                        new_child = rpc::make_shared<autonomous_node_impl>(child_type, child_zone_id);
                        new_child->initialize_node(child_type, child_zone_id);
                        new_child->set_parent_node(parent);
                        RPC_INFO("[NODE {}] setup callback for child zone {} completed", node_id_, child_zone_id);
                        CO_RETURN rpc::error::OK();
                    });
            RPC_INFO("[NODE {}] create_child_node result={}", node_id_, result);
            CO_RETURN result;
        }
        catch (const std::exception& e)
        {
            spdlog::error("[NODE {}] Exception in child creation: {}", node_id_, e.what());
            CO_RETURN rpc::error::EXCEPTION();
        }
    }

    CORO_TASK(int) get_cached_children_count(int& count) override
    {
        count = static_cast<int>(child_nodes_.size());
        RPC_INFO("[NODE {}] get_cached_children_count() -> count={}", node_id_, count);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) get_cached_child_by_index(int index, rpc::shared_ptr<i_autonomous_node>& child) override
    {
        std::ignore = child;
        RPC_INFO("[NODE {}] get_cached_child_by_index(index={}), children_size={}", node_id_, index, child_nodes_.size());
        if (index < 0 || static_cast<size_t>(index) >= child_nodes_.size())
        {
            child.reset();
            RPC_INFO("[NODE {}] get_cached_child_by_index failed: invalid index", node_id_);
            CO_RETURN rpc::error::INVALID_DATA();
        }
        child = child_nodes_[index];
        RPC_INFO("[NODE {}] get_cached_child_by_index completed", node_id_);
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) get_parent_node(rpc::shared_ptr<i_autonomous_node>& parent) override
    {
        parent = parent_node_;
        RPC_INFO("[NODE {}] get_parent_node() -> parent={}", node_id_, (parent ? "exists" : "null"));
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) set_parent_node(rpc::shared_ptr<i_autonomous_node> parent) override
    {
        RPC_INFO("[NODE {}] set_parent_node(parent={})", node_id_, (parent ? "exists" : "null"));
        parent_node_ = parent;
        RPC_INFO("[NODE {}] set_parent_node completed", node_id_);
        CO_RETURN rpc::error::OK();
    }

    // Unused legacy methods
    CORO_TASK(int) connect_to_node([[maybe_unused]] rpc::shared_ptr<i_autonomous_node> target_node) override { CO_RETURN rpc::error::OK(); }
    CORO_TASK(int) pass_object_to_connected([[maybe_unused]] int connection_index, [[maybe_unused]] rpc::shared_ptr<i_shared_object> object) override
    {
        CO_RETURN rpc::error::OK();
    }
    CORO_TASK(int) request_child_creation([[maybe_unused]] rpc::shared_ptr<i_autonomous_node> target_parent,
        [[maybe_unused]] node_type child_type,
        [[maybe_unused]] uint64_t child_zone_id,
        [[maybe_unused]] rpc::shared_ptr<i_autonomous_node>& child_proxy) override
    {
        CO_RETURN rpc::error::OK();
    }

    // i_cleanup implementation
    CORO_TASK(int) cleanup(rpc::shared_ptr<i_garbage_collector> collector) override
    {
        RPC_INFO("[NODE {}] cleanup() called, already_cleaned={}, child_nodes_size={}, created_objects_size={}",
            node_id_,
            cleanup_called_,
            child_nodes_.size(),
            created_objects_.size());
        if (cleanup_called_)
        {
            RPC_INFO("[NODE {}] cleanup already called, returning", node_id_);
            CO_RETURN rpc::error::OK(); // Already cleaned up
        }
        cleanup_called_ = true;

        // Cleanup child nodes first
        RPC_INFO("[NODE {}] cleaning {} child nodes", node_id_, child_nodes_.size());
        for (size_t i = 0; i < child_nodes_.size(); ++i)
        {
            auto& child = child_nodes_[i];
            RPC_INFO("[NODE {}] cleaning child node index {}", node_id_, i);
            auto cleanup_child = rpc::dynamic_pointer_cast<i_cleanup>(child);
            if (cleanup_child)
            {
                cleanup_child->cleanup(collector);
                collector->collect(cleanup_child);
            }
            else
            {
                RPC_INFO("[NODE {}] child node index {} does not support cleanup", node_id_, i);
            }
        }

        // Cleanup created objects
        RPC_INFO("[NODE {}] cleaning {} created objects", node_id_, created_objects_.size());
        for (size_t i = 0; i < created_objects_.size(); ++i)
        {
            auto& obj = created_objects_[i];
            RPC_INFO("[NODE {}] cleaning created object index {}", node_id_, i);
            auto cleanup_obj = rpc::dynamic_pointer_cast<i_cleanup>(obj);
            if (cleanup_obj)
            {
                cleanup_obj->cleanup(collector);
                collector->collect(cleanup_obj);
            }
            else
            {
                RPC_INFO("[NODE {}] created object index {} does not support cleanup", node_id_, i);
            }
        }

        // Cleanup local services
        if (local_factory_)
        {
            RPC_INFO("[NODE {}] cleaning local factory", node_id_);
            auto cleanup_factory = rpc::dynamic_pointer_cast<i_cleanup>(local_factory_);
            if (cleanup_factory)
            {
                cleanup_factory->cleanup(collector);
                collector->collect(cleanup_factory);
            }
        }

        if (local_cache_)
        {
            RPC_INFO("[NODE {}] cleaning local cache", node_id_);
            auto cleanup_cache = rpc::dynamic_pointer_cast<i_cleanup>(local_cache_);
            if (cleanup_cache)
            {
                cleanup_cache->cleanup(collector);
                collector->collect(cleanup_cache);
            }
        }

        if (local_worker_)
        {
            RPC_INFO("[NODE {}] cleaning local worker", node_id_);
            auto cleanup_worker = rpc::dynamic_pointer_cast<i_cleanup>(local_worker_);
            if (cleanup_worker)
            {
                cleanup_worker->cleanup(collector);
                collector->collect(cleanup_worker);
            }
        }

        // Clear all references to prevent circular dependencies
        RPC_INFO("[NODE {}] clearing all references", node_id_);
        child_nodes_.clear();
        created_objects_.clear();
        local_factory_.reset();
        local_cache_.reset();
        local_worker_.reset();
        parent_node_.reset(); // Don't cleanup parent, just clear reference

        RPC_INFO("[NODE {}] cleanup completed successfully", node_id_);
        CO_RETURN rpc::error::OK();
    }
};

// Garbage collector implementation
class garbage_collector_impl : public i_garbage_collector, public rpc::enable_shared_from_this<garbage_collector_impl>
{
private:
    std::set<rpc::shared_ptr<i_cleanup>> collected_objects_;

    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<i_garbage_collector>(interface_id))
            return static_cast<const i_garbage_collector*>(this);
        return nullptr;
    }

public:
    garbage_collector_impl() = default;

    CORO_TASK(int) collect(rpc::shared_ptr<i_cleanup> obj) override
    {
        RPC_INFO("[GARBAGE_COLLECTOR] collect() called");
        if (!obj)
        {
            RPC_INFO("[GARBAGE_COLLECTOR] collect failed: null object");
            CO_RETURN rpc::error::INVALID_DATA();
        }

        collected_objects_.insert(obj);
        RPC_INFO("[GARBAGE_COLLECTOR] collected object (total: {})", collected_objects_.size());
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) get_collected_count(int& count) override
    {
        count = static_cast<int>(collected_objects_.size());
        RPC_INFO("[GARBAGE_COLLECTOR] get_collected_count() -> count={}", count);
        CO_RETURN rpc::error::OK();
    }

    // Clear all collected objects - this releases them for destruction
    void release_all()
    {
        RPC_INFO("[GARBAGE_COLLECTOR] release_all() called - releasing {} objects for destruction",
            collected_objects_.size());
        collected_objects_.clear();
        RPC_INFO("[GARBAGE_COLLECTOR] release_all() completed - all objects released");
    }

    // Debug method to print all collected objects with their details
    void debug_print_collected_objects()
    {
        RPC_INFO("[GARBAGE_COLLECTOR] === DEBUG: Collected Objects ===");
        for (auto obj : collected_objects_)
        {
            if (!obj)
            {
                continue;
            }

            auto* proxy = obj->query_proxy_base();
            if (!proxy)
            {
                continue;
            }

            auto op = proxy->get_object_proxy();
            [[maybe_unused]] auto object_id = op->get_object_id();
            [[maybe_unused]] auto zone_id = op->get_service_proxy()->get_zone_id();
            RPC_INFO("[GARBAGE_COLLECTOR] Object zone id: {} object_id: {}", zone_id.get_val(), object_id.get_val());

            // Try to cast to interface types to get more information
            auto autonomous_node = rpc::dynamic_pointer_cast<i_autonomous_node>(obj);
            if (autonomous_node)
            {
                node_type type;
                uint64_t id;
                int conn, obj_held;
                if (autonomous_node->get_node_status(type, id, conn, obj_held) == rpc::error::OK())
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: AUTONOMOUS_NODE id={} type={} connections={} objects_held={}",
                        id,
                        static_cast<int>(type),
                        conn,
                        obj_held);
                }
                else
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: AUTONOMOUS_NODE (status call failed)");
                }
                continue;
            }

            auto shared_obj = rpc::dynamic_pointer_cast<i_shared_object>(obj);
            if (shared_obj)
            {
                int stats = 0;
                int value = 0;
                if (shared_obj->get_stats(stats) == rpc::error::OK() && shared_obj->get_value(value) == rpc::error::OK())
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: SHARED_OBJECT stats={} value={}", stats, value);
                }
                else
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: SHARED_OBJECT (details not accessible)");
                }
                continue;
            }

            auto factory = rpc::dynamic_pointer_cast<i_fuzz_factory>(obj);
            if (factory)
            {
                int total_created = 0, current_refs = 0;
                if (factory->get_factory_stats(total_created, current_refs) == rpc::error::OK())
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: FACTORY total_created={} current_refs={}",
                        total_created,
                        current_refs);
                }
                else
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: FACTORY (stats not accessible)");
                }
                continue;
            }

            auto cache = rpc::dynamic_pointer_cast<i_fuzz_cache>(obj);
            if (cache)
            {
                int cache_size = 0;
                if (cache->get_cache_size(cache_size) == rpc::error::OK())
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: CACHE size={}", cache_size);
                }
                else
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: CACHE (size not accessible)");
                }
                continue;
            }

            auto worker = rpc::dynamic_pointer_cast<i_fuzz_worker>(obj);
            if (worker)
            {
                int objects_processed = 0, total_increments = 0;
                if (worker->get_worker_stats(objects_processed, total_increments) == rpc::error::OK())
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: WORKER processed={} increments={}",
                        objects_processed,
                        total_increments);
                }
                else
                {
                    RPC_INFO("[GARBAGE_COLLECTOR] Object: WORKER (stats not accessible)");
                }
                continue;
            }

            RPC_INFO("[GARBAGE_COLLECTOR] Object: UNKNOWN TYPE (implements i_cleanup only)");
        }
        RPC_INFO("[GARBAGE_COLLECTOR] === END DEBUG: {} total objects ===", collected_objects_.size());
    }
};

// Generate random instruction sets for autonomous execution with weighted probabilities
std::vector<instruction> generate_instruction_set(int max_instructions, bool has_parent, bool has_children)
{
    std::vector<instruction> instructions;
    std::mt19937& gen = get_global_rng();
    std::uniform_int_distribution<int> count_dist(1, max_instructions);
    std::uniform_int_distribution<int> value_dist(1, 100);

    // Use weighted probabilities for more realistic scenarios
    std::map<std::string, int> op_weights;
    op_weights["CREATE_CAPABILITY"] = 20;
    op_weights["CREATE_OBJECT"] = 15;
    op_weights["STORE_OBJECT"] = 10;
    op_weights["PROCESS_OBJECT"] = 10;
    op_weights["FORK_CHILD"] = 5; // Lower probability now that structure is deterministic

    if (has_children)
    {
        op_weights["PASS_TO_RANDOM_CHILD"] = 20;
    }
    if (has_parent)
    {
        op_weights["PASS_TO_PARENT"] = 20;
        op_weights["PASS_TO_RANDOM_SIBLING"] = 20;
    }

    int total_weight = 0;
    for (const auto& pair : op_weights)
        total_weight += pair.second;

    if (total_weight == 0)
        return instructions; // No possible operations

    std::uniform_int_distribution<int> op_dist(1, total_weight);

    int instruction_count = count_dist(gen);
    for (int i = 0; i < instruction_count; ++i)
    {
        instruction instr;
        instr.instruction_id = i + 1;

        int rand_val = op_dist(gen);
        int cumulative_weight = 0;
        for (const auto& pair : op_weights)
        {
            cumulative_weight += pair.second;
            if (rand_val <= cumulative_weight)
            {
                instr.operation = pair.first;
                break;
            }
        }
        instr.target_value = value_dist(gen);
        instructions.push_back(instr);
    }

    return instructions;
}

// Helper function to create a chain of nodes using interface methods
rpc::shared_ptr<i_autonomous_node> create_deep_branch(
    rpc::shared_ptr<i_autonomous_node> parent, int depth, std::vector<rpc::shared_ptr<i_autonomous_node>>& all_nodes)
{
    rpc::shared_ptr<i_autonomous_node> current_node = parent;
    RPC_INFO("Creating deep branch from parent with depth {}", depth);

    for (int i = 0; i < depth; ++i)
    {
        rpc::shared_ptr<i_autonomous_node> new_child;
        auto child_zone_id = ++g_zone_id_counter;

        RPC_INFO("Attempting to create child {} of {} (zone_id={})", i + 1, depth, child_zone_id);

        // Use the interface method instead of trying to cast to implementation
        auto result = current_node->create_child_node(node_type::WORKER_NODE, child_zone_id, true, new_child);

        if (result == rpc::error::OK() && new_child)
        {
            all_nodes.push_back(new_child);
            current_node = new_child;
            RPC_INFO("Successfully created child {} of {} (zone_id={})", i + 1, depth, child_zone_id);
        }
        else
        {
            spdlog::error("Failed to create child node {} of {} (zone_id={}, result={})", i + 1, depth, child_zone_id, result);
            break;
        }
    }
    return current_node; // Return the last node in the chain
}

// Run a complete autonomous instruction test cycle
void run_autonomous_instruction_test(int test_cycle, int instruction_count, uint64_t override_seed = 0)
{
    spdlog::info("=== Starting Autonomous Instruction Test Cycle {} ===", test_cycle);

    // Reset counters to match original state
    g_zone_id_counter = 0;
    g_instruction_counter = 0;

    // Create root service
    auto root_service = rpc::make_shared<rpc::service>("AUTONOMOUS_ROOT", rpc::zone{++g_zone_id_counter});
    fuzz_test_idl_register_stubs(root_service);

    // Initialize test scenario configuration for replay system
    test_scenario_config scenario_config;
    scenario_config.test_cycle = test_cycle;
    scenario_config.instruction_count = instruction_count;

    // Use override seed if provided (for replay), otherwise generate new seed
    if (override_seed != 0)
    {
        scenario_config.random_seed = override_seed;
        spdlog::info("Using replay seed: {}", override_seed);
    }
    else
    {
        scenario_config.random_seed
            = static_cast<uint64_t>(std::time(nullptr)) + test_cycle; // Deterministic but unique seed
    }

    // Set timestamp as string instead of chrono time_point
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp_ss;
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    timestamp_ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    scenario_config.timestamp = timestamp_ss.str();

    // Initialize global RNG with the scenario seed
    initialize_global_rng(scenario_config.random_seed);

    RPC_INFO("Test configuration:");

    // Declare cleanup objects outside try block so they're accessible in catch block
    rpc::shared_ptr<garbage_collector_impl> garbage_collector;

    try
    {
        garbage_collector = rpc::make_shared<garbage_collector_impl>();

        {
            rpc::shared_ptr<i_autonomous_node> root_node;
            std::vector<rpc::shared_ptr<i_autonomous_node>> all_nodes;

            // 1. Create the root node
            std::string zone_name = "autonomous_root_" + std::to_string(test_cycle);
            auto zone_id = ++g_zone_id_counter;
            scenario_config.zone_sequence.push_back(zone_id); // Track zone creation

            root_service->connect_to_zone<rpc::local_child_service_proxy<i_autonomous_node, i_autonomous_node>>(
                zone_name.c_str(),
                {zone_id},
                rpc::shared_ptr<i_autonomous_node>(),
                root_node,
                [=](const rpc::shared_ptr<i_autonomous_node>&,
                    rpc::shared_ptr<i_autonomous_node>& new_node,
                    const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
                {
                    fuzz_test_idl_register_stubs(child_service_ptr);
                    new_node = rpc::make_shared<autonomous_node_impl>(node_type::ROOT_NODE, zone_id);
                    CO_RETURN CO_AWAIT new_node->initialize_node(node_type::ROOT_NODE, zone_id);
                });

            if (!root_node)
            {
                spdlog::error("Failed to create root node.");
                return;
            }
            all_nodes.push_back(root_node);

            // 2. Build the deterministic graph structure
            RPC_INFO("Building deterministic graph structure...");
            // Create main branch 5 nodes deep
            auto main_branch_end = create_deep_branch(root_node, 5, all_nodes);

            // Create 5 sub-branches, each 5 nodes deep
            for (int i = 0; i < 5; ++i)
            {
                RPC_INFO("Creating sub-branch {}...", i + 1);
                create_deep_branch(main_branch_end, 5, all_nodes);
            }
            RPC_INFO("Graph construction complete. Total nodes: {}", all_nodes.size());

            // 3. Assign scripts to 3 random nodes
            if (all_nodes.size() > 1)
            {
                RPC_INFO("Assigning scripts to 3 random nodes...");
                std::mt19937& gen = get_global_rng(); // Use global deterministic RNG
                std::vector<int> indices(all_nodes.size());
                std::iota(indices.begin(), indices.end(), 0);
                std::shuffle(indices.begin(), indices.end(), gen);

                int runners_count = std::min(3, (int)all_nodes.size());
                scenario_config.runners_count = runners_count;

                // Collect runner-target pairs before execution
                for (int i = 0; i < runners_count; ++i)
                {
                    auto& runner_node = all_nodes[indices[i]];

                    // Select a random target node for this runner
                    std::uniform_int_distribution<size_t> target_dist(0, all_nodes.size() - 1);
                    auto& target_node = all_nodes[target_dist(gen)];

                    uint64_t runner_id = 0;
                    node_type type;
                    uint64_t id;
                    int conn, obj_held;

                    runner_node->get_node_status(type, id, conn, obj_held);
                    runner_id = id;

                    uint64_t target_id = 0;
                    target_node->get_node_status(type, id, conn, obj_held);
                    target_id = id;

                    // Store runner-target pair for replay using IDL structure
                    runner_target_pair pair;
                    pair.runner_id = runner_id;
                    pair.target_id = target_id;
                    scenario_config.runner_target_pairs.push_back(pair);

                    RPC_INFO("Runner node {} ({}) will target node {}.", i + 1, runner_id, target_id);
                }

                // DUMP SCENARIO BEFORE EXECUTION FOR REPLAY
                dump_test_scenario(scenario_config, "ABOUT_TO_EXECUTE");
                RPC_INFO("Test scenario dumped for potential replay");

                // Now execute the runners with the collected configuration
                for (int i = 0; i < runners_count; ++i)
                {
                    auto& runner_node = all_nodes[indices[i]];
                    std::uniform_int_distribution<size_t> target_dist(0, all_nodes.size() - 1);
                    auto& target_node = all_nodes[target_dist(gen)];

                    RPC_INFO("Executing runner {} (zone {}) → target {} (zone {}) with {} instructions",
                        i + 1,
                        scenario_config.runner_target_pairs[i].runner_id,
                        i + 1,
                        scenario_config.runner_target_pairs[i].target_id,
                        scenario_config.instruction_count);
                    runner_node->run_script(target_node, scenario_config.instruction_count);
                }
            }

            auto root_cleanup = rpc::dynamic_pointer_cast<i_cleanup>(root_node);
            if (root_cleanup)
            {
                root_cleanup->cleanup(garbage_collector);
                garbage_collector->collect(root_cleanup);
            }
            else
            {
                spdlog::error("Root node does NOT support i_cleanup interface!");
                return; // Cannot proceed without cleanup support
            }

            for (auto node : all_nodes)
            {
                auto node_cleanup = rpc::dynamic_pointer_cast<i_cleanup>(node);
                if (node_cleanup)
                {
                    node_cleanup->cleanup(garbage_collector);
                    garbage_collector->collect(node_cleanup);
                }
            }

            // DEBUG: Print all objects in garbage collector
            garbage_collector->debug_print_collected_objects();

        } // End of inner scope - automatic C++ destructors run here

        // Get count of objects transferred to garbage collector
        int collected_count = 0;
        garbage_collector->get_collected_count(collected_count);
        RPC_INFO("Garbage collector now owns {} objects", collected_count);

        // Release all collected objects for destruction (this triggers final cleanup)
        RPC_INFO("About to call garbage_collector->release_all()");
        garbage_collector->release_all();
        RPC_INFO("garbage_collector->release_all() completed");

        // Clear garbage collector reference last
        RPC_INFO("Clearing garbage collector reference");
        garbage_collector.reset();
        RPC_INFO("All references cleared - cleanup complete");

        // Mark scenario as successfully completed
        dump_test_scenario(scenario_config, "COMPLETED_SUCCESS");

        // Clean up successful test file if option is enabled
        cleanup_successful_test(scenario_config);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception in autonomous test cycle {}: {}", test_cycle, e.what());

        // Dump failure scenario for replay
        std::string error_msg = "Exception: " + std::string(e.what());
        dump_failure_scenario(scenario_config, error_msg);

        // EMERGENCY CLEANUP: Reset any remaining references to prevent garbage collection issues
        RPC_INFO("Exception cleanup: Attempting to clear any remaining references...");

        // Try to clear garbage collector references if they still exist
        try
        {
            if (garbage_collector)
            {
                RPC_INFO("Force-clearing garbage collector in exception handler");
                garbage_collector.reset();
            }
        }
        catch (...)
        {
            spdlog::error("Exception during emergency cleanup - ignoring");
        }

        RPC_INFO("Exception cleanup completed");

        // Re-throw so the main process can handle it appropriately
        throw;
    }

    spdlog::info("=== Autonomous Test Cycle {} Completed ===", test_cycle);
}

int main(int argc, char** argv)
{
    try
    {
        args::ArgumentParser parser("RPC++ Fuzz Tester - Tests zone hierarchies and cross-zone object passing");

        args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

        // Mode selection flags (mutually exclusive)
        args::Group mode_group(parser, "Mode Selection (choose one):", args::Group::Validators::AtMostOne);
        args::ValueFlag<std::string> replay_file(
            mode_group, "file", "Replay saved test scenario from JSON file", {'r', "replay"});

        // Main test options
        args::ValueFlag<int> test_cycles(parser, "cycles", "Number of test cycles to run (default: 5)", {'c', "cycles"}, 5);
        args::ValueFlag<int> instruction_count(parser, "instructions", "Number of instructions per runner (default: 10)", {'i', "instructions"}, 10);
        args::ValueFlag<std::string> output_dir(parser,
            "directory",
            "Directory for JSON scenario files (default: tests/fuzz_test/replays)",
            {'o', "output-dir"},
            "tests/fuzz_test/replays");
        args::Flag keep_success(
            parser, "keep", "Keep successful test files (default: delete them)", {"keep-success"});
        args::Flag enable_telemetry(
            parser, "enable telemetry", "Enable telemetry output", {'t', "enable-telemetry"});

        try
        {
            parser.ParseCLI(argc, argv);
        }
        catch (const args::Help&)
        {
            std::cout << parser;
            return 0;
        }
        catch (const args::ParseError& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << parser;
            return 1;
        }

        // Set global configuration from parsed arguments
        g_output_directory = args::get(output_dir);
        g_cleanup_successful_tests = !args::get(keep_success);

        // Handle mode selection
        if (replay_file)
        {
            std::string file = args::get(replay_file);
            spdlog::info("REPLAY MODE: Replaying scenario from {}", file);
#ifdef USE_RPC_TELEMETRY
            if(args::get(enable_telemetry))
                telemetry_service_manager_.create("autonomous_test", "autonomous_test", "../../rpc_test_diagram/");
#endif
            return replay_test_scenario(file);
        }

        // Default: run fuzz test
        int cycles = args::get(test_cycles);
        int instructions = args::get(instruction_count);

        spdlog::info("Starting Autonomous Instruction-Based Fuzz Test");
        spdlog::info("Configuration:");
        spdlog::info("  Test cycles: {}", cycles);
        spdlog::info("  Instructions per runner: {}", instructions);
        spdlog::info("  Output directory: {}", g_output_directory);
        spdlog::info("  Cleanup successful tests: {}", g_cleanup_successful_tests ? "enabled" : "disabled");

        for (int cycle = 1; cycle <= cycles; ++cycle)
        {
#ifdef USE_RPC_TELEMETRY
            if(args::get(enable_telemetry))
                telemetry_service_manager_.create("autonomous_test", "autonomous_test", "../../rpc_test_diagram/");
#endif
            try
            {
                // Initialize root service for the cycle
                run_autonomous_instruction_test(cycle, instructions);
            }
            catch (const std::exception& e)
            {
                spdlog::error("Exception in test cycle {}: {}", cycle, e.what());

                // Note: root_service will be automatically cleaned up by destructor
                // when it goes out of scope, but the exception means the garbage
                // collection in run_autonomous_instruction_test didn't complete

#ifdef USE_RPC_TELEMETRY
                telemetry_service_manager_.reset();
#endif
                throw; // Re-throw to let main catch block handle it
            }

#ifdef USE_RPC_TELEMETRY
            telemetry_service_manager_.reset();
#endif
        }

        spdlog::info("All autonomous instruction test cycles completed successfully!");

        return 0;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception in main: {}", e.what());
        return 1;
    }
}

// ============================================================================
// REPLAY SYSTEM IMPLEMENTATION
// ============================================================================

void dump_test_scenario(const test_scenario_config& config, const std::string& status)
{
    std::filesystem::create_directories(g_output_directory);

    std::stringstream filename;
    filename << g_output_directory << "/scenario_" << config.test_cycle << "_" << config.random_seed << ".json";

    // Create a copy with the current status
    test_scenario_config config_copy = config;
    config_copy.status = status;

    try
    {
        // Use YAS JSON serialization
        auto json_data = rpc::to_yas_json<std::string>(config_copy);

        std::ofstream file(filename.str());
        if (!file.is_open())
        {
            spdlog::error("Failed to create scenario file: {}", filename.str());
            return;
        }

        file << json_data;
        file.close();

        RPC_INFO("Test scenario {} dumped to: {}", status, filename.str());
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to serialize scenario: {}", e.what());
    }
}

void dump_failure_scenario(const test_scenario_config& config, const std::string& error_msg)
{
    std::filesystem::create_directories(g_output_directory);

    std::stringstream filename;
    filename << g_output_directory << "/FAILURE_" << config.test_cycle << "_" << config.random_seed << ".json";

    // Create a copy with failure information
    test_scenario_config config_copy = config;
    config_copy.status = "FAILED";
    config_copy.error_message = error_msg;

    std::stringstream reproduction_cmd;
    reproduction_cmd << "./build/output/debug/fuzz_test_main --replay " << filename.str();
    config_copy.reproduction_command = reproduction_cmd.str();

    try
    {
        // Use YAS JSON serialization
        auto json_data = rpc::to_yas_json<std::string>(config_copy);

        std::ofstream file(filename.str());
        if (!file.is_open())
        {
            spdlog::error("Failed to create failure scenario file: {}", filename.str());
            return;
        }

        file << json_data;
        file.close();

        spdlog::error("FAILURE scenario dumped to: {}", filename.str());
        spdlog::error("To replay this failure: {}", config_copy.reproduction_command);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to serialize failure scenario: {}", e.what());
    }
}

test_scenario_config load_test_scenario(const std::string& scenario_file)
{
    std::ifstream file(scenario_file);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open scenario file: " + scenario_file);
    }

    // Read the entire file content
    std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    try
    {
        // Use YAS JSON deserialization
        test_scenario_config config;
        rpc::span data{reinterpret_cast<const uint8_t*>(json_content.data()),
            reinterpret_cast<const uint8_t*>(json_content.data() + json_content.size())};

        std::string error = rpc::from_yas_json(data, config);
        if (!error.empty())
        {
            throw std::runtime_error("YAS deserialization error: " + error);
        }

        return config;
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to deserialize scenario file " + scenario_file + ": " + e.what());
    }
}

int replay_test_scenario(const std::string& scenario_file)
{
    spdlog::info("=== REPLAYING SCENARIO: {} ===", scenario_file);

    try
    {
        test_scenario_config config = load_test_scenario(scenario_file);

        spdlog::info("Loaded scenario:");
        spdlog::info("  Test cycle: {}", config.test_cycle);
        spdlog::info("  Random seed: {}", config.random_seed);
        spdlog::info("  Runners count: {}", config.runners_count);
        spdlog::info("  Instruction count: {}", config.instruction_count);
        spdlog::info("  Runner-target pairs: {}", config.runner_target_pairs.size());

        RPC_INFO("Starting replay execution...");

        // Run the exact same test scenario with the saved seed
        run_autonomous_instruction_test(config.test_cycle, config.instruction_count, config.random_seed);

        spdlog::info("=== REPLAY COMPLETED SUCCESSFULLY ===");
        return 0;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Replay failed with exception: {}", e.what());
        return 1;
    }
}

void cleanup_successful_test(const test_scenario_config& config)
{
    if (!g_cleanup_successful_tests)
    {
        return; // Cleanup disabled
    }

    std::stringstream filename;
    filename << g_output_directory << "/scenario_" << config.test_cycle << "_" << config.random_seed << ".json";

    if (std::filesystem::exists(filename.str()))
    {
        std::filesystem::remove(filename.str());
        RPC_INFO("🗑️ Cleaned up successful test file: {}", filename.str());
    }
}
