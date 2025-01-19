#pragma once

#include <string>
#include <list>

#include "cpp_parser.h"

bool is_in_param(const std::list<std::string>& attributes);
bool is_out_param(const std::list<std::string>& attributes);
bool is_const_param(const std::list<std::string>& attributes);

bool is_reference(std::string type_name);
bool is_rvalue(std::string type_name);
bool is_pointer(std::string type_name);
bool is_pointer_reference(std::string type_name);
bool is_pointer_to_pointer(std::string type_name);

std::string get_encapsulated_shared_ptr_type(const std::string& type_name);

bool is_interface_param(const class_entity& lib, const std::string& type);
