/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "test_service_logger.h"
#include <string_view>

test_service_logger::test_service_logger()
{
    logr->info("************************************");
    logr->info("test {}", ::testing::UnitTest::GetInstance()->current_test_info()->name());
}

test_service_logger::~test_service_logger()
{
}

void test_service_logger::before_send(rpc::caller_zone caller_zone_id,
    rpc::object object_id,
    rpc::interface_ordinal interface_id,
    rpc::method method_id,
    size_t in_size_,
    const char* in_buf_)
{
    logr->info("caller_zone_id {} object_id {} interface_ordinal {} method {} data {}",
        caller_zone_id.id,
        object_id.id,
        interface_id.id,
        method_id.id,
        std::string_view(in_buf_, in_size_));
}

void test_service_logger::after_send(rpc::caller_zone caller_zone_id,
    rpc::object object_id,
    rpc::interface_ordinal interface_id,
    rpc::method method_id,
    int ret,
    const std::vector<char>& out_buf_)
{
    logr->info("caller_zone_id {} object_id {} interface_ordinal {} method {} ret {} data {}",
        caller_zone_id.id,
        object_id.id,
        interface_id.id,
        method_id.id,
        ret,
        std::string_view(out_buf_.data(), out_buf_.size()));
}