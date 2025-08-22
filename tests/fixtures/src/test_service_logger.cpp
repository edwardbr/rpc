/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "test_service_logger.h"
#include <string_view>
#include <sstream>

test_service_logger::test_service_logger()
{
    rpc_global_logger::info("************************************");
    std::string test_name = "test " + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    rpc_global_logger::info(test_name);
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
    std::ostringstream oss;
    oss << "caller_zone_id " << caller_zone_id.id 
        << " object_id " << object_id.id 
        << " interface_ordinal " << interface_id.id 
        << " method " << method_id.id 
        << " data " << std::string_view(in_buf_, in_size_);
    rpc_global_logger::info(oss.str());
}

void test_service_logger::after_send(rpc::caller_zone caller_zone_id,
    rpc::object object_id,
    rpc::interface_ordinal interface_id,
    rpc::method method_id,
    int ret,
    const std::vector<char>& out_buf_)
{
    std::ostringstream oss;
    oss << "caller_zone_id " << caller_zone_id.id 
        << " object_id " << object_id.id 
        << " interface_ordinal " << interface_id.id 
        << " method " << method_id.id 
        << " ret " << ret 
        << " data " << std::string_view(out_buf_.data(), out_buf_.size());
    rpc_global_logger::info(oss.str());
}