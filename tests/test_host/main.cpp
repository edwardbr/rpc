/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <args.hxx>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <rpc/rpc.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/telemetry/multiplexing_telemetry_service.h>
#include <rpc/telemetry/console_telemetry_service.h>
#include <rpc/telemetry/sequence_diagram_telemetry_service.h>
#endif

#include "rpc_global_logger.h"
#include "crash_handler.h"

#include "test_globals.h"

#include <example/example.h>

bool enable_multithreaded_tests = false;

// This line tests that we can define tests in an unnamed namespace.
namespace
{

    extern "C" int main(int argc, char* argv[])
    {
        args::ArgumentParser parser("RPC++ Test Suite - Comprehensive RPC testing framework");

        // Basic test control flags
        args::Flag enable_multithreaded_flag(
            parser, "multithreaded", "Enable multithreaded tests", {'m', "enable-multithreaded"});

        // Telemetry service flags
        args::Flag enable_console_telemetry(
            parser, "console", "Add console telemetry service", {"telemetry-console", "console"});
        args::ValueFlag<std::string> console_path(
            parser, "console-path", "Console telemetry output path", {"console-path"}, "../../rpc_test_diagram/");
        args::Flag enable_sequence_diagram_telemetry(
            parser, "sequence", "Add sequence diagram telemetry service", {"telemetry-sequence"});
        args::ValueFlag<std::string> sequence_path(
            parser, "sequence-path", "Sequence diagram output path", {"sequence-path"}, "../../rpc_test_diagram/");
        args::Flag enable_animation_telemetry(
            parser, "animation", "Add animation telemetry service", {"animation-sequence"});
        args::ValueFlag<std::string> animation_path(
            parser, "animation-path", "Animation diagram output path", {"animation-path"}, "../../rpc_test_diagram/");

        args::Flag help(parser, "help", "Display this help menu", {'h', "help"});

        try
        {
            parser.ParseCLI(argc, argv);
        }
        catch (const args::ParseError& e)
        {
            // std::cout << "Continuing to pass all arguments to Google Test: " << e.what() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error during argument parsing: " << e.what() << std::endl;
            return 1;
        }
        
        if(help.HasFlag())
        {   
            std::cout << parser;
            std::cout << "\nGoogle Test Integration:\n";
            std::cout << "  All Google Test flags are supported and will be passed through.\n";
            std::cout << "  Use --gtest_help to see Google Test specific options.\n\n";
            std::cout << "Examples:\n";
            std::cout << "  ./rpc_test --telemetry-console\n";
            std::cout << "  ./rpc_test --telemetry-console --telemetry-sequence\n";
            std::cout << "  ./rpc_test --telemetry-console --gtest_filter=\"*standard_tests*\"\n";
        }

        // Extract parsed values
        enable_multithreaded_tests = args::get(enable_multithreaded_flag);

#ifdef USE_RPC_TELEMETRY
        // Ensure we have a multiplexing telemetry service when any telemetry flags are provided
        if ((args::get(enable_console_telemetry) || args::get(enable_sequence_diagram_telemetry)
                || args::get(enable_animation_telemetry)))
        {
            // Create empty multiplexing service
            std::vector<std::shared_ptr<rpc::i_telemetry_service>> empty_services;
            if (rpc::multiplexing_telemetry_service::create(std::move(empty_services)))
            {
                std::cout << "Created multiplexing telemetry service" << std::endl;
            }
        }

        // Assume telemetry_service_ is always a multiplexing service and register configurations
        if (rpc::get_telemetry_service())
        {
            auto multiplexing_service
                = std::static_pointer_cast<rpc::multiplexing_telemetry_service>(rpc::get_telemetry_service());

            if (args::get(enable_console_telemetry))
            {
                multiplexing_service->register_service_config("console", console_path.Get());
                std::cout << "Registered console telemetry service configuration" << std::endl;
            }

            if (args::get(enable_sequence_diagram_telemetry))
            {
                multiplexing_service->register_service_config("sequence", sequence_path.Get());
                std::cout << "Registered sequence diagram telemetry service configuration" << std::endl;
            }

            if (args::get(enable_animation_telemetry))
            {
                multiplexing_service->register_service_config("animation", animation_path.Get());
                std::cout << "Registered animation telemetry service configuration" << std::endl;
            }
        }
#endif

        // Print configuration summary
        std::cout << "\n=== RPC++ Test Configuration ===" << std::endl;
        std::cout << "Multithreaded tests: " << (enable_multithreaded_tests ? "YES" : "NO") << std::endl;
        std::cout << "==================================\n" << std::endl;

#ifndef _WIN32
        // Initialize comprehensive crash handler with multi-threaded support
        // (Only available on POSIX systems - Windows not supported)
        crash_handler::crash_handler::Config crash_config;
        crash_config.enable_multithreaded_traces = true;
        crash_config.enable_symbol_resolution = true;
        crash_config.enable_threading_debug_info = true;
        crash_config.enable_pattern_detection = true;
        crash_config.max_stack_frames = 64;
        crash_config.max_threads = 50;
        crash_config.save_crash_dump = true;
        crash_config.crash_dump_path = "./build/crash";

        // Set up custom crash analysis callback
        crash_handler::crash_handler::set_analysis_callback(
            [](const crash_handler::crash_handler::crash_report& report)
            {
                std::cout << "\n=== CUSTOM CRASH ANALYSIS ===" << std::endl;
                std::cout << "Crash occurred at: "
                          << std::chrono::duration_cast<std::chrono::seconds>(report.crash_time.time_since_epoch()).count()
                          << std::endl;

                // Check for threading debug patterns
                bool threading_bug_detected = false;
                for (const auto& pattern : report.detected_patterns)
                {
                    if (pattern.find("THREADING") != std::string::npos || pattern.find("ZONE PROXY") != std::string::npos)
                    {
                        threading_bug_detected = true;
                        break;
                    }
                }

                if (threading_bug_detected)
                {
                    std::cout << "🐛 THREADING BUG CONFIRMED: This crash was caused by a race condition!" << std::endl;
                    std::cout << "   The threading debug system successfully detected the issue." << std::endl;
                }
                else
                {
                    std::cout << "ℹ️  Standard crash - not detected as threading-related." << std::endl;
                }

                std::cout << "=== END CUSTOM ANALYSIS ===" << std::endl;
            });

        if (!crash_handler::crash_handler::initialize(crash_config))
        {
            std::cerr << "Failed to initialize crash handler" << std::endl;
            return 1;
        }

        std::cout << "[Main] Comprehensive crash handler initialized" << std::endl;
        std::cout << "[Main] - Multi-threaded stack traces: ENABLED" << std::endl;
        std::cout << "[Main] - Symbol resolution: ENABLED" << std::endl;
        std::cout << "[Main] - Threading debug integration: ENABLED" << std::endl;
        std::cout << "[Main] - Pattern detection: ENABLED" << std::endl;
        std::cout << "[Main] - Crash dumps will be saved to: " << crash_config.crash_dump_path << std::endl;
#else
        std::cout << "[Main] Crash handler not available on Windows" << std::endl;
#endif

        int ret = 0;
        try
        {
            // Initialize global logger for consistent logging
            rpc_global_logger::get_logger();

            ::testing::InitGoogleTest(&argc, argv);
            ret = RUN_ALL_TESTS();
            rpc_global_logger::reset_logger();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception caught in main: " << e.what() << std::endl;
            ret = 1;
        }
        catch (...)
        {
            std::cerr << "Unknown exception caught in main" << std::endl;
            ret = 1;
        }

        // Cleanup crash handler
#ifndef _WIN32
        crash_handler::crash_handler::shutdown();
#endif
        std::cout << "[Main] test shutdown complete" << std::endl;

        return ret;
    }
}


static_assert(rpc::id<std::string>::get(rpc::VERSION_2) == rpc::STD_STRING_ID);

static_assert(rpc::id<xxx::test_template<std::string>>::get(rpc::VERSION_2) == 0xAFFFFFEB79FBFBFB);
static_assert(rpc::id<xxx::test_template_without_params_in_id<std::string>>::get(rpc::VERSION_2) == 0x62C84BEB07545E2B);
static_assert(rpc::id<xxx::test_template_use_legacy_empty_template_struct_id<std::string>>::get(rpc::VERSION_2)
              == 0x2E7E56276F6E36BE);
static_assert(rpc::id<xxx::test_template_use_old<std::string>>::get(rpc::VERSION_2) == 0x66D71EBFF8C6FFA7);
