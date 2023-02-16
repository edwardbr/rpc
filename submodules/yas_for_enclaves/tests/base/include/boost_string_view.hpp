
// Copyright (c) 2010-2021 niXman (github dot nixman at pm dot me). All
// rights reserved.
//
// This file is part of YAS(https://github.com/niXman/yas) project.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef __yas__tests__base__include__boost_string_view_hpp
#define __yas__tests__base__include__boost_string_view_hpp

#include <boost/utility/string_view.hpp>

/***************************************************************************/

template<typename archive_traits>
bool boost_string_view_test(std::ostream &log, const char *archive_type, const char *test_name) {
    typename archive_traits::oarchive oa;
    archive_traits::ocreate(oa, archive_type);
    boost::string_view s("发送日期 string");
    oa & YAS_OBJECT_NVP("obj", ("s", s));

    typename archive_traits::iarchive ia;
    archive_traits::icreate(ia, oa, archive_type);
    std::string ss;
    ia & YAS_OBJECT_NVP("obj", ("s", ss));

    if ( ss.compare(0, s.length(), s.data()) ) {
        YAS_TEST_REPORT(log, archive_type, test_name);
        return false;
    }

    return true;
}

/***************************************************************************/

#endif // __yas__tests__base__include__boost_string_view_hpp
