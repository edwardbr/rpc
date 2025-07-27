/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <rpc/basic_service_proxies.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <gtest/gtest.h>

class test_service_logger : public rpc::service_logger
{
    inline static std::shared_ptr<spdlog::logger> logr
        = spdlog::basic_logger_mt("basic_logger", "./conversation.txt", true);

public:
    test_service_logger();
    virtual ~test_service_logger();

    void before_send(rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id,
        size_t in_size_,
        const char* in_buf_) override;

    void after_send(rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id,
        int ret,
        const std::vector<char>& out_buf_) override;
};