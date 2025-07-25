#pragma once

#include <string>
#include <list>

#include "cpp_parser.h"
#include "coreclasses.h"
#include "writer.h"

bool is_in_param(const attributes& attribs);
bool is_out_param(const attributes& attribs);
bool is_const_param(const attributes& attribs);

bool is_reference(std::string type_name);
bool is_rvalue(std::string type_name);
bool is_pointer(std::string type_name);
bool is_pointer_reference(std::string type_name);
bool is_pointer_to_pointer(std::string type_name);

std::string get_encapsulated_shared_ptr_type(const std::string& type_name);

bool is_interface_param(const class_entity& lib, const std::string& type);

bool is_type_and_parameter_the_same(std::string type, std::string name);

void render_parameter(writer& header, const class_entity& m_ob, const parameter_entity& parameter);

void render_function(writer& header, const class_entity& m_ob, const function_entity& function);
