#pragma once

// standard attributes that are used by the idl parser, other attributes may be used by the generator

namespace rpc_attribute_types
{
    ///////////////////////////////
    // namespace modifiers
    ///////////////////////////////

    // this makes a function const the idl parser does not support modifiers at the end of a function declaration after
    // the last ')'
    constexpr const char* inline_namespace = "inline";

    ///////////////////////////////
    // function modifiers
    ///////////////////////////////

    // this makes a function const the idl parser does not support modifiers at the end of a function declaration after
    // the last ')'
    constexpr const char* deprecated_function = "deprecated";

    // this is to provide backward compatability with various bugs that break fingerprints, dont use these with new idl
    // declarations!

    // when the deprecated keyword modified the fingerprint
    constexpr const char* fingerprint_contaminating_deprecated_function = "buggy_deprecated";

    // some template expansion bug backwards compatability
    constexpr const char* use_legacy_empty_template_struct_id_attr = "buggy_empty_template_struct_id";
    constexpr const char* use_template_param_in_id_attr = "buggy_template_param_in_id";
}
