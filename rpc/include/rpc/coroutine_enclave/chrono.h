// -*- C++ -*-
//===---------------------------- chrono ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

// #ifndef _LIBCPP_CHRONO
// #define _LIBCPP_CHRONO

// #include "enclave_fix.h"
#include <ratio>
/*
    chrono synopsis

namespace std
{
namespace chrono
{

template <class ToDuration, class Rep, class Period>
constexpr
ToDuration
duration_cast(const duration<Rep, Period>& fd);

template <class Rep> struct treat_as_floating_point : is_floating_point<Rep> {};

template <class Rep> inline constexpr bool treat_as_floating_point_v
    = treat_as_floating_point<Rep>::value;                       // C++17

template <class Rep>
struct duration_values
{
public:
    static constexpr Rep zero(); // noexcept in C++20
    static constexpr Rep max();  // noexcept in C++20
    static constexpr Rep min();  // noexcept in C++20
};

// duration

template <class Rep, class Period = ratio<1>>
class duration
{
    static_assert(!__is_duration<Rep>::value, "A duration representation can not be a duration");
    static_assert(__is_ratio<Period>::value, "Second template parameter of duration must be a std::ratio");
    static_assert(Period::num > 0, "duration period must be positive");
public:
    typedef Rep rep;
    typedef typename _Period::type period;

    constexpr duration() = default;
    template <class Rep2>
        constexpr explicit duration(const Rep2& r,
            typename enable_if
            <
               is_convertible<Rep2, rep>::value &&
               (treat_as_floating_point<rep>::value ||
               !treat_as_floating_point<rep>::value && !treat_as_floating_point<Rep2>::value)
            >::type* = 0);

    // conversions
    template <class Rep2, class Period2>
        constexpr duration(const duration<Rep2, Period2>& d,
            typename enable_if
            <
                treat_as_floating_point<rep>::value ||
                ratio_divide<Period2, period>::type::den == 1
            >::type* = 0);

    // observer

    constexpr rep count() const;

    // arithmetic

    constexpr common_type<duration>::type  operator+() const;
    constexpr common_type<duration>::type  operator-() const;
    constexpr duration& operator++();    // constexpr in C++17
    constexpr duration  operator++(int); // constexpr in C++17
    constexpr duration& operator--();    // constexpr in C++17
    constexpr duration  operator--(int); // constexpr in C++17

    constexpr duration& operator+=(const duration& d);  // constexpr in C++17
    constexpr duration& operator-=(const duration& d);  // constexpr in C++17

    duration& operator*=(const rep& rhs);       // constexpr in C++17
    duration& operator/=(const rep& rhs);       // constexpr in C++17
    duration& operator%=(const rep& rhs);       // constexpr in C++17
    duration& operator%=(const duration& rhs);  // constexpr in C++17

    // special values

    static constexpr duration zero(); // noexcept in C++20
    static constexpr duration min();  // noexcept in C++20
    static constexpr duration max();  // noexcept in C++20
};

typedef duration<long long,         nano> nanoseconds;
typedef duration<long long,        micro> microseconds;
typedef duration<long long,        milli> milliseconds;
typedef duration<long long              > seconds;
typedef duration<     long, ratio<  60> > minutes;
typedef duration<     long, ratio<3600> > hours;

template <class Clock, class Duration = typename Clock::duration>
class time_point
{
public:
    typedef Clock                     clock;
    typedef Duration                  duration;
    typedef typename duration::rep    rep;
    typedef typename duration::period period;
private:
    duration d_;  // exposition only

public:
    time_point();  // has value "epoch" // constexpr in C++14
    explicit time_point(const duration& d);  // same as time_point() + d // constexpr in C++14

    // conversions
    template <class Duration2>
       time_point(const time_point<clock, Duration2>& t); // constexpr in C++14

    // observer

    duration time_since_epoch() const; // constexpr in C++14

    // arithmetic

    time_point& operator+=(const duration& d); // constexpr in C++17
    time_point& operator-=(const duration& d); // constexpr in C++17

    // special values

    static constexpr time_point min();  // noexcept in C++20
    static constexpr time_point max();  // noexcept in C++20
};

} // chrono

// common_type traits
template <class Rep1, class Period1, class Rep2, class Period2>
  struct common_type<chrono::duration<Rep1, Period1>, chrono::duration<Rep2, Period2>>;

template <class Clock, class Duration1, class Duration2>
  struct common_type<chrono::time_point<Clock, Duration1>, chrono::time_point<Clock, Duration2>>;

namespace chrono {


template<class T> struct is_clock;  // C++20
template<class T> inline constexpr bool is_clock_v = is_clock<T>::value;   // C++20


// duration arithmetic
template <class Rep1, class Period1, class Rep2, class Period2>
  constexpr
  typename common_type<duration<Rep1, Period1>, duration<Rep2, Period2>>::type
  operator+(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period1, class Rep2, class Period2>
  constexpr
  typename common_type<duration<Rep1, Period1>, duration<Rep2, Period2>>::type
  operator-(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period, class Rep2>
  constexpr
  duration<typename common_type<Rep1, Rep2>::type, Period>
  operator*(const duration<Rep1, Period>& d, const Rep2& s);
template <class Rep1, class Period, class Rep2>
  constexpr
  duration<typename common_type<Rep1, Rep2>::type, Period>
  operator*(const Rep1& s, const duration<Rep2, Period>& d);
template <class Rep1, class Period, class Rep2>
  constexpr
  duration<typename common_type<Rep1, Rep2>::type, Period>
  operator/(const duration<Rep1, Period>& d, const Rep2& s);
template <class Rep1, class Period1, class Rep2, class Period2>
  constexpr
  typename common_type<Rep1, Rep2>::type
  operator/(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);

// duration comparisons
template <class Rep1, class Period1, class Rep2, class Period2>
   constexpr
   bool operator==(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period1, class Rep2, class Period2>
   constexpr
   bool operator!=(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period1, class Rep2, class Period2>
   constexpr
   bool operator< (const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period1, class Rep2, class Period2>
   constexpr
   bool operator<=(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period1, class Rep2, class Period2>
   constexpr
   bool operator> (const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period1, class Rep2, class Period2>
   constexpr
   bool operator>=(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs);

// duration_cast
template <class ToDuration, class Rep, class Period>
  ToDuration duration_cast(const duration<Rep, Period>& d);

template <class ToDuration, class Rep, class Period>
    constexpr ToDuration floor(const duration<Rep, Period>& d);    // C++17
template <class ToDuration, class Rep, class Period>
    constexpr ToDuration ceil(const duration<Rep, Period>& d);     // C++17
template <class ToDuration, class Rep, class Period>
    constexpr ToDuration round(const duration<Rep, Period>& d);    // C++17

// duration I/O is elsewhere

// time_point arithmetic (all constexpr in C++14)
template <class Clock, class Duration1, class Rep2, class Period2>
  time_point<Clock, typename common_type<Duration1, duration<Rep2, Period2>>::type>
  operator+(const time_point<Clock, Duration1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Rep1, class Period1, class Clock, class Duration2>
  time_point<Clock, typename common_type<duration<Rep1, Period1>, Duration2>::type>
  operator+(const duration<Rep1, Period1>& lhs, const time_point<Clock, Duration2>& rhs);
template <class Clock, class Duration1, class Rep2, class Period2>
  time_point<Clock, typename common_type<Duration1, duration<Rep2, Period2>>::type>
  operator-(const time_point<Clock, Duration1>& lhs, const duration<Rep2, Period2>& rhs);
template <class Clock, class Duration1, class Duration2>
  typename common_type<Duration1, Duration2>::type
  operator-(const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs);

// time_point comparisons (all constexpr in C++14)
template <class Clock, class Duration1, class Duration2>
   bool operator==(const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs);
template <class Clock, class Duration1, class Duration2>
   bool operator!=(const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs);
template <class Clock, class Duration1, class Duration2>
   bool operator< (const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs);
template <class Clock, class Duration1, class Duration2>
   bool operator<=(const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs);
template <class Clock, class Duration1, class Duration2>
   bool operator> (const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs);
template <class Clock, class Duration1, class Duration2>
   bool operator>=(const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs);

// time_point_cast (constexpr in C++14)

template <class ToDuration, class Clock, class Duration>
  time_point<Clock, ToDuration> time_point_cast(const time_point<Clock, Duration>& t);

template <class ToDuration, class Clock, class Duration>
    constexpr time_point<Clock, ToDuration>
    floor(const time_point<Clock, Duration>& tp);                  // C++17

template <class ToDuration, class Clock, class Duration>
    constexpr time_point<Clock, ToDuration>
    ceil(const time_point<Clock, Duration>& tp);                   // C++17

template <class ToDuration, class Clock, class Duration>
    constexpr time_point<Clock, ToDuration>
    round(const time_point<Clock, Duration>& tp);                  // C++17

template <class Rep, class Period>
    constexpr duration<Rep, Period> abs(duration<Rep, Period> d);  // C++17

// Clocks

class system_clock
{
public:
    typedef microseconds                     duration;
    typedef duration::rep                    rep;
    typedef duration::period                 period;
    typedef chrono::time_point<system_clock> time_point;
    static const bool is_steady =            false; // constexpr in C++14

    static time_point now() noexcept;
    static time_t     to_time_t  (const time_point& __t) noexcept;
    static time_point from_time_t(time_t __t) noexcept;
};

template <class Duration>
  using sys_time  = time_point<system_clock, Duration>; // C++20
using sys_seconds = sys_time<seconds>;                  // C++20
using sys_days    = sys_time<days>;                     // C++20

class utc_clock;                                        // C++20

template <class Duration>
  using utc_time  = time_point<utc_clock, Duration>;    // C++20
using utc_seconds = utc_time<seconds>;                  // C++20

class tai_clock;                                        // C++20

template <class Duration>
  using tai_time  = time_point<tai_clock, Duration>;    // C++20
using tai_seconds = tai_time<seconds>;                  // C++20

class file_clock;                                       // C++20

template<class Duration>
  using file_time = time_point<file_clock, Duration>;   // C++20

class steady_clock
{
public:
    typedef nanoseconds                                   duration;
    typedef duration::rep                                 rep;
    typedef duration::period                              period;
    typedef chrono::time_point<steady_clock, duration>    time_point;
    static const bool is_steady =                         true; // constexpr in C++14

    static time_point now() noexcept;
};

typedef steady_clock high_resolution_clock;

// 25.7.8, local time           // C++20
struct local_t {};
template<class Duration>
  using local_time  = time_point<local_t, Duration>;
using local_seconds = local_time<seconds>;
using local_days    = local_time<days>;

// 25.7.9, time_point conversions template<class DestClock, class SourceClock>    // C++20
struct clock_time_conversion;

template<class DestClock, class SourceClock, class Duration>
  auto clock_cast(const time_point<SourceClock, Duration>& t);

// 25.8.2, class last_spec    // C++20
struct last_spec;

// 25.8.3, class day          // C++20

class day;
constexpr bool operator==(const day& x, const day& y) noexcept;
constexpr bool operator!=(const day& x, const day& y) noexcept;
constexpr bool operator< (const day& x, const day& y) noexcept;
constexpr bool operator> (const day& x, const day& y) noexcept;
constexpr bool operator<=(const day& x, const day& y) noexcept;
constexpr bool operator>=(const day& x, const day& y) noexcept;
constexpr day  operator+(const day&  x, const days& y) noexcept;
constexpr day  operator+(const days& x, const day&  y) noexcept;
constexpr day  operator-(const day&  x, const days& y) noexcept;
constexpr days operator-(const day&  x, const day&  y) noexcept;

// 25.8.4, class month    // C++20
class month;
constexpr bool operator==(const month& x, const month& y) noexcept;
constexpr bool operator!=(const month& x, const month& y) noexcept;
constexpr bool operator< (const month& x, const month& y) noexcept;
constexpr bool operator> (const month& x, const month& y) noexcept;
constexpr bool operator<=(const month& x, const month& y) noexcept;
constexpr bool operator>=(const month& x, const month& y) noexcept;
constexpr month  operator+(const month&  x, const months& y) noexcept;
constexpr month  operator+(const months& x,  const month& y) noexcept;
constexpr month  operator-(const month&  x, const months& y) noexcept;
constexpr months operator-(const month&  x,  const month& y) noexcept;

// 25.8.5, class year    // C++20
class year;
constexpr bool operator==(const year& x, const year& y) noexcept;
constexpr bool operator!=(const year& x, const year& y) noexcept;
constexpr bool operator< (const year& x, const year& y) noexcept;
constexpr bool operator> (const year& x, const year& y) noexcept;
constexpr bool operator<=(const year& x, const year& y) noexcept;
constexpr bool operator>=(const year& x, const year& y) noexcept;
constexpr year  operator+(const year&  x, const years& y) noexcept;
constexpr year  operator+(const years& x, const year&  y) noexcept;
constexpr year  operator-(const year&  x, const years& y) noexcept;
constexpr years operator-(const year&  x, const year&  y) noexcept;

// 25.8.6, class weekday    // C++20
class weekday;

constexpr bool operator==(const weekday& x, const weekday& y) noexcept;
constexpr bool operator!=(const weekday& x, const weekday& y) noexcept;
constexpr weekday operator+(const weekday& x, const days&    y) noexcept;
constexpr weekday operator+(const days&    x, const weekday& y) noexcept;
constexpr weekday operator-(const weekday& x, const days&    y) noexcept;
constexpr days    operator-(const weekday& x, const weekday& y) noexcept;

// 25.8.7, class weekday_indexed    // C++20

class weekday_indexed;
constexpr bool operator==(const weekday_indexed& x, const weekday_indexed& y) noexcept;
constexpr bool operator!=(const weekday_indexed& x, const weekday_indexed& y) noexcept;

// 25.8.8, class weekday_last    // C++20
class weekday_last;

constexpr bool operator==(const weekday_last& x, const weekday_last& y) noexcept;
constexpr bool operator!=(const weekday_last& x, const weekday_last& y) noexcept;

// 25.8.9, class month_day    // C++20
class month_day;

constexpr bool operator==(const month_day& x, const month_day& y) noexcept;
constexpr bool operator!=(const month_day& x, const month_day& y) noexcept;
constexpr bool operator< (const month_day& x, const month_day& y) noexcept;
constexpr bool operator> (const month_day& x, const month_day& y) noexcept;
constexpr bool operator<=(const month_day& x, const month_day& y) noexcept;
constexpr bool operator>=(const month_day& x, const month_day& y) noexcept;


// 25.8.10, class month_day_last    // C++20
class month_day_last;

constexpr bool operator==(const month_day_last& x, const month_day_last& y) noexcept;
constexpr bool operator!=(const month_day_last& x, const month_day_last& y) noexcept;
constexpr bool operator< (const month_day_last& x, const month_day_last& y) noexcept;
constexpr bool operator> (const month_day_last& x, const month_day_last& y) noexcept;
constexpr bool operator<=(const month_day_last& x, const month_day_last& y) noexcept;
constexpr bool operator>=(const month_day_last& x, const month_day_last& y) noexcept;

// 25.8.11, class month_weekday    // C++20
class month_weekday;

constexpr bool operator==(const month_weekday& x, const month_weekday& y) noexcept;
constexpr bool operator!=(const month_weekday& x, const month_weekday& y) noexcept;

// 25.8.12, class month_weekday_last    // C++20
class month_weekday_last;

constexpr bool operator==(const month_weekday_last& x, const month_weekday_last& y) noexcept;
constexpr bool operator!=(const month_weekday_last& x, const month_weekday_last& y) noexcept;


// 25.8.13, class year_month    // C++20
class year_month;

constexpr bool operator==(const year_month& x, const year_month& y) noexcept;
constexpr bool operator!=(const year_month& x, const year_month& y) noexcept;
constexpr bool operator< (const year_month& x, const year_month& y) noexcept;
constexpr bool operator> (const year_month& x, const year_month& y) noexcept;
constexpr bool operator<=(const year_month& x, const year_month& y) noexcept;
constexpr bool operator>=(const year_month& x, const year_month& y) noexcept;

constexpr year_month operator+(const year_month& ym, const months& dm) noexcept;
constexpr year_month operator+(const months& dm, const year_month& ym) noexcept;
constexpr year_month operator-(const year_month& ym, const months& dm) noexcept;
constexpr months operator-(const year_month& x, const year_month& y) noexcept;
constexpr year_month operator+(const year_month& ym, const years& dy) noexcept;
constexpr year_month operator+(const years& dy, const year_month& ym) noexcept;
constexpr year_month operator-(const year_month& ym, const years& dy) noexcept;

// 25.8.14, class year_month_day class    // C++20
year_month_day;

constexpr bool operator==(const year_month_day& x, const year_month_day& y) noexcept;
constexpr bool operator!=(const year_month_day& x, const year_month_day& y) noexcept;
constexpr bool operator< (const year_month_day& x, const year_month_day& y) noexcept;
constexpr bool operator> (const year_month_day& x, const year_month_day& y) noexcept;
constexpr bool operator<=(const year_month_day& x, const year_month_day& y) noexcept;
constexpr bool operator>=(const year_month_day& x, const year_month_day& y) noexcept;

constexpr year_month_day operator+(const year_month_day& ymd, const months& dm) noexcept;
constexpr year_month_day operator+(const months& dm, const year_month_day& ymd) noexcept;
constexpr year_month_day operator+(const year_month_day& ymd, const years& dy) noexcept;
constexpr year_month_day operator+(const years& dy, const year_month_day& ymd) noexcept;
constexpr year_month_day operator-(const year_month_day& ymd, const months& dm) noexcept;
constexpr year_month_day operator-(const year_month_day& ymd, const years& dy) noexcept;


// 25.8.15, class year_month_day_last    // C++20
class year_month_day_last;

constexpr bool operator==(const year_month_day_last& x,
                          const year_month_day_last& y) noexcept;
constexpr bool operator!=(const year_month_day_last& x,
                          const year_month_day_last& y) noexcept;
constexpr bool operator< (const year_month_day_last& x,
                          const year_month_day_last& y) noexcept;
constexpr bool operator> (const year_month_day_last& x,
                          const year_month_day_last& y) noexcept;
constexpr bool operator<=(const year_month_day_last& x,
                          const year_month_day_last& y) noexcept;
constexpr bool operator>=(const year_month_day_last& x,
                          const year_month_day_last& y) noexcept;

constexpr year_month_day_last
  operator+(const year_month_day_last& ymdl, const months& dm) noexcept;
constexpr year_month_day_last
  operator+(const months& dm, const year_month_day_last& ymdl) noexcept;
constexpr year_month_day_last
  operator+(const year_month_day_last& ymdl, const years& dy) noexcept;
constexpr year_month_day_last
  operator+(const years& dy, const year_month_day_last& ymdl) noexcept;
constexpr year_month_day_last
  operator-(const year_month_day_last& ymdl, const months& dm) noexcept;
constexpr year_month_day_last
  operator-(const year_month_day_last& ymdl, const years& dy) noexcept;

// 25.8.16, class year_month_weekday    // C++20
class year_month_weekday;

constexpr bool operator==(const year_month_weekday& x,
                          const year_month_weekday& y) noexcept;
constexpr bool operator!=(const year_month_weekday& x,
                          const year_month_weekday& y) noexcept;

constexpr year_month_weekday
  operator+(const year_month_weekday& ymwd, const months& dm) noexcept;
constexpr year_month_weekday
  operator+(const months& dm, const year_month_weekday& ymwd) noexcept;
constexpr year_month_weekday
  operator+(const year_month_weekday& ymwd, const years& dy) noexcept;
constexpr year_month_weekday
  operator+(const years& dy, const year_month_weekday& ymwd) noexcept;
constexpr year_month_weekday
  operator-(const year_month_weekday& ymwd, const months& dm) noexcept;
constexpr year_month_weekday
  operator-(const year_month_weekday& ymwd, const years& dy) noexcept;

// 25.8.17, class year_month_weekday_last    // C++20
class year_month_weekday_last;

constexpr bool operator==(const year_month_weekday_last& x,
                          const year_month_weekday_last& y) noexcept;
constexpr bool operator!=(const year_month_weekday_last& x,
                          const year_month_weekday_last& y) noexcept;
constexpr year_month_weekday_last
  operator+(const year_month_weekday_last& ymwdl, const months& dm) noexcept;
constexpr year_month_weekday_last
  operator+(const months& dm, const year_month_weekday_last& ymwdl) noexcept;
constexpr year_month_weekday_last
  operator+(const year_month_weekday_last& ymwdl, const years& dy) noexcept;
constexpr year_month_weekday_last
  operator+(const years& dy, const year_month_weekday_last& ymwdl) noexcept;
constexpr year_month_weekday_last
  operator-(const year_month_weekday_last& ymwdl, const months& dm) noexcept;
constexpr year_month_weekday_last
  operator-(const year_month_weekday_last& ymwdl, const years& dy) noexcept;

// 25.8.18, civil calendar conventional syntax operators    // C++20
constexpr year_month
  operator/(const year& y, const month& m) noexcept;
constexpr year_month
  operator/(const year& y, int m) noexcept;
constexpr month_day
  operator/(const month& m, const day& d) noexcept;
constexpr month_day
  operator/(const month& m, int d) noexcept;
constexpr month_day
  operator/(int m, const day& d) noexcept;
constexpr month_day
  operator/(const day& d, const month& m) noexcept;
constexpr month_day
  operator/(const day& d, int m) noexcept;
constexpr month_day_last
  operator/(const month& m, last_spec) noexcept;
constexpr month_day_last
  operator/(int m, last_spec) noexcept;
constexpr month_day_last
  operator/(last_spec, const month& m) noexcept;
constexpr month_day_last
  operator/(last_spec, int m) noexcept;
constexpr month_weekday
  operator/(const month& m, const weekday_indexed& wdi) noexcept;
constexpr month_weekday
  operator/(int m, const weekday_indexed& wdi) noexcept;
constexpr month_weekday
  operator/(const weekday_indexed& wdi, const month& m) noexcept;
constexpr month_weekday
  operator/(const weekday_indexed& wdi, int m) noexcept;
constexpr month_weekday_last
  operator/(const month& m, const weekday_last& wdl) noexcept;
constexpr month_weekday_last
  operator/(int m, const weekday_last& wdl) noexcept;
constexpr month_weekday_last
  operator/(const weekday_last& wdl, const month& m) noexcept;
constexpr month_weekday_last
  operator/(const weekday_last& wdl, int m) noexcept;
constexpr year_month_day
  operator/(const year_month& ym, const day& d) noexcept;
constexpr year_month_day
  operator/(const year_month& ym, int d) noexcept;
constexpr year_month_day
  operator/(const year& y, const month_day& md) noexcept;
constexpr year_month_day
  operator/(int y, const month_day& md) noexcept;
constexpr year_month_day
  operator/(const month_day& md, const year& y) noexcept;
constexpr year_month_day
  operator/(const month_day& md, int y) noexcept;
constexpr year_month_day_last
  operator/(const year_month& ym, last_spec) noexcept;
constexpr year_month_day_last
  operator/(const year& y, const month_day_last& mdl) noexcept;
constexpr year_month_day_last
  operator/(int y, const month_day_last& mdl) noexcept;
constexpr year_month_day_last
  operator/(const month_day_last& mdl, const year& y) noexcept;
constexpr year_month_day_last
  operator/(const month_day_last& mdl, int y) noexcept;
constexpr year_month_weekday
  operator/(const year_month& ym, const weekday_indexed& wdi) noexcept;
constexpr year_month_weekday
  operator/(const year& y, const month_weekday& mwd) noexcept;
constexpr year_month_weekday
  operator/(int y, const month_weekday& mwd) noexcept;
constexpr year_month_weekday
  operator/(const month_weekday& mwd, const year& y) noexcept;
constexpr year_month_weekday
  operator/(const month_weekday& mwd, int y) noexcept;
constexpr year_month_weekday_last
  operator/(const year_month& ym, const weekday_last& wdl) noexcept;
constexpr year_month_weekday_last
  operator/(const year& y, const month_weekday_last& mwdl) noexcept;
constexpr year_month_weekday_last
  operator/(int y, const month_weekday_last& mwdl) noexcept;
constexpr year_month_weekday_last
  operator/(const month_weekday_last& mwdl, const year& y) noexcept;
constexpr year_month_weekday_last
  operator/(const month_weekday_last& mwdl, int y) noexcept;

// 26.9, class template hh_mm_ss
template <class Duration>
class hh_mm_ss
{
    bool            is_neg; // exposition only
    chrono::hours   h;      // exposition only
    chrono::minutes m;      // exposition only
    chrono::seconds s;      // exposition only
    precision       ss;     // exposition only

public:
    static unsigned constexpr fractional_width = see below;
    using precision                            = see below;

    constexpr hh_mm_ss() noexcept : hh_mm_ss{Duration::zero()} {}
    constexpr explicit hh_mm_ss(Duration d) noexcept;

    constexpr bool is_negative() const noexcept;
    constexpr chrono::hours hours() const noexcept;
    constexpr chrono::minutes minutes() const noexcept;
    constexpr chrono::seconds seconds() const noexcept;
    constexpr precision subseconds() const noexcept;

    constexpr explicit operator  precision()   const noexcept;
    constexpr          precision to_duration() const noexcept;
};

template <class charT, class traits, class Duration>
  basic_ostream<charT, traits>&
    operator<<(basic_ostream<charT, traits>& os, hh_mm_ss<Duration> const& hms);

// 26.10, 12/24 hour functions
constexpr bool is_am(hours const& h) noexcept;
constexpr bool is_pm(hours const& h) noexcept;
constexpr hours make12(const hours& h) noexcept;
constexpr hours make24(const hours& h, bool is_pm) noexcept;


// 25.10.2, time zone database     // C++20
struct tzdb;
class tzdb_list;

// 25.10.2.3, time zone database access    // C++20
const tzdb& get_tzdb();
tzdb_list& get_tzdb_list();
const time_zone* locate_zone(string_view tz_name);
const time_zone* current_zone();

// 25.10.2.4, remote time zone database support    // C++20
const tzdb& reload_tzdb();
string remote_version();

// 25.10.3, exception classes    // C++20
class nonexistent_local_time;
class ambiguous_local_time;

// 25.10.4, information classes    // C++20
struct sys_info;
struct local_info;

// 25.10.5, class time_zone    // C++20
enum class choose {earliest, latest};
class time_zone;
bool operator==(const time_zone& x, const time_zone& y) noexcept;
bool operator!=(const time_zone& x, const time_zone& y) noexcept;
bool operator<(const time_zone& x, const time_zone& y) noexcept;
bool operator>(const time_zone& x, const time_zone& y) noexcept;
bool operator<=(const time_zone& x, const time_zone& y) noexcept;
bool operator>=(const time_zone& x, const time_zone& y) noexcept;

// 25.10.6, class template zoned_traits    // C++20
template<class T> struct zoned_traits;

// 25.10.7, class template zoned_time    // C++20
template<class Duration, class TimeZonePtr = const time_zone*> class zoned_time;
using zoned_seconds = zoned_time<seconds>;

template<class Duration1, class Duration2, class TimeZonePtr>
  bool operator==(const zoned_time<Duration1, TimeZonePtr>& x,
                  const zoned_time<Duration2, TimeZonePtr>& y);
template<class Duration1, class Duration2, class TimeZonePtr>
  bool operator!=(const zoned_time<Duration1, TimeZonePtr>& x,
                  const zoned_time<Duration2, TimeZonePtr>& y);

// 25.10.8, leap second support    // C++20
class leap;

bool operator==(const leap& x, const leap& y);
bool operator!=(const leap& x, const leap& y);
bool operator< (const leap& x, const leap& y);
bool operator> (const leap& x, const leap& y);
bool operator<=(const leap& x, const leap& y);
bool operator>=(const leap& x, const leap& y);
template<class Duration>
  bool operator==(const leap& x, const sys_time<Duration>& y);
template<class Duration>
  bool operator==(const sys_time<Duration>& x, const leap& y);
template<class Duration>
  bool operator!=(const leap& x, const sys_time<Duration>& y);
template<class Duration>
  bool operator!=(const sys_time<Duration>& x, const leap& y);
template<class Duration>
  bool operator< (const leap& x, const sys_time<Duration>& y);
template<class Duration>
  bool operator< (const sys_time<Duration>& x, const leap& y);
template<class Duration>
  bool operator> (const leap& x, const sys_time<Duration>& y);
template<class Duration>
  bool operator> (const sys_time<Duration>& x, const leap& y);
template<class Duration>
  bool operator<=(const leap& x, const sys_time<Duration>& y);
template<class Duration>
  bool operator<=(const sys_time<Duration>& x, const leap& y);
template<class Duration>
  bool operator>=(const leap& x, const sys_time<Duration>& y);
template<class Duration>
  bool operator>=(const sys_time<Duration>& x, const leap& y);

// 25.10.9, class link    // C++20
class link;
bool operator==(const link& x, const link& y);
bool operator!=(const link& x, const link& y);
bool operator< (const link& x, const link& y);
bool operator> (const link& x, const link& y);
bool operator<=(const link& x, const link& y);
bool operator>=(const link& x, const link& y);

// 25.11, formatting    // C++20
template<class charT, class Streamable>
  basic_string<charT>
    format(const charT* fmt, const Streamable& s);

template<class charT, class Streamable>
  basic_string<charT>
    format(const locale& loc, const charT* fmt, const Streamable& s);

template<class charT, class traits, class Alloc, class Streamable>
  basic_string<charT, traits, Alloc>
    format(const basic_string<charT, traits, Alloc>& fmt, const Streamable& s);

template<class charT, class traits, class Alloc, class Streamable>
  basic_string<charT, traits, Alloc>
    format(const locale& loc, const basic_string<charT, traits, Alloc>& fmt,
           const Streamable& s);

// 25.12, parsing    // C++20
template<class charT, class traits, class Alloc, class Parsable>
unspecified
    parse(const basic_string<charT, traits, Alloc>& format, Parsable& tp);

template<class charT, class traits, class Alloc, class Parsable>
unspecified
    parse(const basic_string<charT, traits, Alloc>& format, Parsable& tp,
          basic_string<charT, traits, Alloc>& abbrev);

template<class charT, class traits, class Alloc, class Parsable>
unspecified
    parse(const basic_string<charT, traits, Alloc>& format, Parsable& tp,
          minutes& offset);

template<class charT, class traits, class Alloc, class Parsable>
unspecified
    parse(const basic_string<charT, traits, Alloc>& format, Parsable& tp,
          basic_string<charT, traits, Alloc>& abbrev, minutes& offset);

// calendrical constants
inline constexpr last_spec                              last{};       // C++20
inline constexpr chrono::weekday                        Sunday{0};    // C++20
inline constexpr chrono::weekday                        Monday{1};    // C++20
inline constexpr chrono::weekday                        Tuesday{2};   // C++20
inline constexpr chrono::weekday                        Wednesday{3}; // C++20
inline constexpr chrono::weekday                        Thursday{4};  // C++20
inline constexpr chrono::weekday                        Friday{5};    // C++20
inline constexpr chrono::weekday                        Saturday{6};  // C++20

inline constexpr chrono::month                          January{1};   // C++20
inline constexpr chrono::month                          February{2};  // C++20
inline constexpr chrono::month                          March{3};     // C++20
inline constexpr chrono::month                          April{4};     // C++20
inline constexpr chrono::month                          May{5};       // C++20
inline constexpr chrono::month                          June{6};      // C++20
inline constexpr chrono::month                          July{7};      // C++20
inline constexpr chrono::month                          August{8};    // C++20
inline constexpr chrono::month                          September{9}; // C++20
inline constexpr chrono::month                          October{10};  // C++20
inline constexpr chrono::month                          November{11}; // C++20
inline constexpr chrono::month                          December{12}; // C++20
}  // chrono

inline namespace literals {
  inline namespace chrono_literals {
constexpr chrono::hours                                 operator ""h(unsigned long long); // C++14
constexpr chrono::duration<unspecified , ratio<3600,1>> operator ""h(long double); // C++14
constexpr chrono::minutes                               operator ""min(unsigned long long); // C++14
constexpr chrono::duration<unspecified , ratio<60,1>>   operator ""min(long double); // C++14
constexpr chrono::seconds                               operator ""s(unsigned long long); // C++14
constexpr chrono::duration<unspecified >                operator ""s(long double); // C++14
constexpr chrono::milliseconds                          operator ""ms(unsigned long long); // C++14
constexpr chrono::duration<unspecified , milli>         operator ""ms(long double); // C++14
constexpr chrono::microseconds                          operator ""us(unsigned long long); // C++14
constexpr chrono::duration<unspecified , micro>         operator ""us(long double); // C++14
constexpr chrono::nanoseconds                           operator ""ns(unsigned long long); // C++14
constexpr chrono::duration<unspecified , nano>          operator ""ns(long double); // C++14
constexpr chrono::day                                   operator ""d(unsigned long long d) noexcept; // C++20
constexpr chrono::year                                  operator ""y(unsigned long long y) noexcept; // C++20
}  // chrono_literals
}  // literals

}  // std
*/

// Not supported in SGX.
// #include <__config>
// #if !defined(_LIBCPP_SGX_CONFIG)
// #include <__availability>
#include <ctime>
#include <type_traits>
#include <ratio>
#include <limits>
#include <version>

// _LIBCPP_PUSH_MACROS
// #include <__undef_macros>

// #ifndef _LIBCPP_CXX03_LANG
// _LIBCPP_BEGIN_NAMESPACE_FILESYSTEM
// struct _FilesystemClock;
// _LIBCPP_END_NAMESPACE_FILESYSTEM
// #endif // !_LIBCPP_CXX03_LANG

namespace std
{

    namespace chrono
    {
        template<class _Rep, class _Period = ratio<1>> class duration;

        template<class _Tp> struct __is_duration : false_type
        {
        };

        template<class _Rep, class _Period> struct __is_duration<duration<_Rep, _Period>> : true_type
        {
        };

        template<class _Rep, class _Period> struct __is_duration<const duration<_Rep, _Period>> : true_type
        {
        };

        template<class _Rep, class _Period> struct __is_duration<volatile duration<_Rep, _Period>> : true_type
        {
        };

        template<class _Rep, class _Period> struct __is_duration<const volatile duration<_Rep, _Period>> : true_type
        {
        };

    } // chrono

// template <intmax_t _Xp, intmax_t _Yp>
// struct __static_gcd
// {
//     static const intmax_t value = __static_gcd<_Yp, _Xp % _Yp>::value;
// };

// template <intmax_t _Xp>
// struct __static_gcd<_Xp, 0>
// {
//     static const intmax_t value = _Xp;
// };

// template <>
// struct __static_gcd<0, 0>
// {
//     static const intmax_t value = 1;
// };

template <intmax_t _Xp, intmax_t _Yp>
struct __foo_static_lcm
{
    static const intmax_t value = _Xp / __static_gcd<_Xp, _Yp>::value * _Yp;
};
template <class _R1, class _R2>
struct __ratio_gcd
{
    typedef ratio<__static_gcd<_R1::num, _R2::num>::value,
                  __foo_static_lcm<_R1::den, _R2::den>::value> type;
};

    template<class _Rep1, class _Period1, class _Rep2, class _Period2>
    struct common_type<chrono::duration<_Rep1, _Period1>, chrono::duration<_Rep2, _Period2>>
    {
        typedef chrono::duration<typename common_type<_Rep1, _Rep2>::type,
                                 typename __ratio_gcd<_Period1, _Period2>::type> type;
    };

    namespace chrono
    {

        // duration_cast

        template<class _FromDuration, class _ToDuration,
                 class _Period =
                     typename ratio_divide<typename _FromDuration::period, typename _ToDuration::period>::type,
                 bool = _Period::num == 1, bool = _Period::den == 1>
        struct __duration_cast;

        template<class _FromDuration, class _ToDuration, class _Period>
        struct __duration_cast<_FromDuration, _ToDuration, _Period, true, true>
        {
            inline constexpr _ToDuration operator()(const _FromDuration& __fd) const
            {
                return _ToDuration(static_cast<typename _ToDuration::rep>(__fd.count()));
            }
        };

        template<class _FromDuration, class _ToDuration, class _Period>
        struct __duration_cast<_FromDuration, _ToDuration, _Period, true, false>
        {
            inline constexpr _ToDuration operator()(const _FromDuration& __fd) const
            {
                typedef
                    typename common_type<typename _ToDuration::rep, typename _FromDuration::rep, intmax_t>::type _Ct;
                return _ToDuration(static_cast<typename _ToDuration::rep>(static_cast<_Ct>(__fd.count())
                                                                          / static_cast<_Ct>(_Period::den)));
            }
        };

        template<class _FromDuration, class _ToDuration, class _Period>
        struct __duration_cast<_FromDuration, _ToDuration, _Period, false, true>
        {
            inline constexpr _ToDuration operator()(const _FromDuration& __fd) const
            {
                typedef
                    typename common_type<typename _ToDuration::rep, typename _FromDuration::rep, intmax_t>::type _Ct;
                return _ToDuration(static_cast<typename _ToDuration::rep>(static_cast<_Ct>(__fd.count())
                                                                          * static_cast<_Ct>(_Period::num)));
            }
        };

        template<class _FromDuration, class _ToDuration, class _Period>
        struct __duration_cast<_FromDuration, _ToDuration, _Period, false, false>
        {
            inline constexpr _ToDuration operator()(const _FromDuration& __fd) const
            {
                typedef
                    typename common_type<typename _ToDuration::rep, typename _FromDuration::rep, intmax_t>::type _Ct;
                return _ToDuration(static_cast<typename _ToDuration::rep>(
                    static_cast<_Ct>(__fd.count()) * static_cast<_Ct>(_Period::num) / static_cast<_Ct>(_Period::den)));
            }
        };

        template<class _ToDuration, class _Rep, class _Period>
        inline constexpr
            typename enable_if<__is_duration<_ToDuration>::value, _ToDuration>::type
            duration_cast(const duration<_Rep, _Period>& __fd)
        {
            return __duration_cast<duration<_Rep, _Period>, _ToDuration>()(__fd);
        }

        template<class _Rep> struct treat_as_floating_point : is_floating_point<_Rep>
        {
        };

        template<class _Rep>
        inline constexpr bool treat_as_floating_point_v = treat_as_floating_point<_Rep>::value;

        template<class _Rep> struct duration_values
        {
        public:
            inline static constexpr _Rep zero() noexcept { return _Rep(0); }
            inline static constexpr _Rep max() noexcept
            {
                return numeric_limits<_Rep>::max();
            }
            inline static constexpr _Rep min() noexcept
            {
                return numeric_limits<_Rep>::lowest();
            }
        };

        template<class _ToDuration, class _Rep, class _Period>
        inline constexpr
            typename enable_if<__is_duration<_ToDuration>::value, _ToDuration>::type
            floor(const duration<_Rep, _Period>& __d)
        {
            _ToDuration __t = duration_cast<_ToDuration>(__d);
            if(__t > __d)
                __t = __t - _ToDuration {1};
            return __t;
        }

        template<class _ToDuration, class _Rep, class _Period>
        inline constexpr
            typename enable_if<__is_duration<_ToDuration>::value, _ToDuration>::type
            ceil(const duration<_Rep, _Period>& __d)
        {
            _ToDuration __t = duration_cast<_ToDuration>(__d);
            if(__t < __d)
                __t = __t + _ToDuration {1};
            return __t;
        }

        template<class _ToDuration, class _Rep, class _Period>
        inline constexpr
            typename enable_if<__is_duration<_ToDuration>::value, _ToDuration>::type
            round(const duration<_Rep, _Period>& __d)
        {
            _ToDuration __lower = floor<_ToDuration>(__d);
            _ToDuration __upper = __lower + _ToDuration {1};
            auto __lowerDiff = __d - __lower;
            auto __upperDiff = __upper - __d;
            if(__lowerDiff < __upperDiff)
                return __lower;
            if(__lowerDiff > __upperDiff)
                return __upper;
            return __lower.count() & 1 ? __upper : __lower;
        }

        // duration

        template<class _Rep, class _Period> class duration
        {
            static_assert(!__is_duration<_Rep>::value, "A duration representation can not be a duration");
            static_assert(__is_ratio<_Period>::value, "Second template parameter of duration must be a std::ratio");
            static_assert(_Period::num > 0, "duration period must be positive");

            template<class _R1, class _R2> struct __no_overflow
            {
            private:
                static const intmax_t __gcd_n1_n2 = __static_gcd<_R1::num, _R2::num>::value;
                static const intmax_t __gcd_d1_d2 = __static_gcd<_R1::den, _R2::den>::value;
                static const intmax_t __n1 = _R1::num / __gcd_n1_n2;
                static const intmax_t __d1 = _R1::den / __gcd_d1_d2;
                static const intmax_t __n2 = _R2::num / __gcd_n1_n2;
                static const intmax_t __d2 = _R2::den / __gcd_d1_d2;
                static const intmax_t max = -((intmax_t(1) << (sizeof(intmax_t) * CHAR_BIT - 1)) + 1);

                template<intmax_t _Xp, intmax_t _Yp, bool __overflow> struct __mul // __overflow == false
                {
                    static const intmax_t value = _Xp * _Yp;
                };

                template<intmax_t _Xp, intmax_t _Yp> struct __mul<_Xp, _Yp, true>
                {
                    static const intmax_t value = 1;
                };

            public:
                static const bool value = (__n1 <= max / __d2) && (__n2 <= max / __d1);
                typedef ratio<__mul<__n1, __d2, !value>::value, __mul<__n2, __d1, !value>::value> type;
            };

        public:
            typedef _Rep rep;
            typedef typename _Period::type period;

        private:
            rep __rep_;

        public:
            inline constexpr duration() = default;
            
            template<class _Rep2>
            inline constexpr explicit duration(
                const _Rep2& __r, typename enable_if<is_convertible<_Rep2, rep>::value
                                                     && (treat_as_floating_point<rep>::value
                                                         || !treat_as_floating_point<_Rep2>::value)>::type* = nullptr)
                : __rep_(__r)
            {
            }

            // conversions
            template<class _Rep2, class _Period2>
            inline constexpr
            duration(const duration<_Rep2, _Period2>& __d,
                     typename enable_if<__no_overflow<_Period2, period>::value
                                        && (treat_as_floating_point<rep>::value
                                            || (__no_overflow<_Period2, period>::type::den == 1
                                                && !treat_as_floating_point<_Rep2>::value))>::type* = nullptr)
                : __rep_(std::chrono::duration_cast<duration>(__d).count())
            {
            }

            // observer

            inline constexpr rep count() const { return __rep_; }

            // arithmetic

            inline constexpr typename common_type<duration>::type operator+() const
            {
                return typename common_type<duration>::type(*this);
            }
            inline constexpr typename common_type<duration>::type operator-() const
            {
                return typename common_type<duration>::type(-__rep_);
            }
            inline constexpr duration& operator++()
            {
                ++__rep_;
                return *this;
            }
            inline constexpr duration operator++(int)
            {
                return duration(__rep_++);
            }
            inline constexpr duration& operator--()
            {
                --__rep_;
                return *this;
            }
            inline constexpr duration operator--(int)
            {
                return duration(__rep_--);
            }

            inline constexpr duration& operator+=(const duration& __d)
            {
                __rep_ += __d.count();
                return *this;
            }
            inline constexpr duration& operator-=(const duration& __d)
            {
                __rep_ -= __d.count();
                return *this;
            }

            inline constexpr duration& operator*=(const rep& rhs)
            {
                __rep_ *= rhs;
                return *this;
            }
            inline constexpr duration& operator/=(const rep& rhs)
            {
                __rep_ /= rhs;
                return *this;
            }
            inline constexpr duration& operator%=(const rep& rhs)
            {
                __rep_ %= rhs;
                return *this;
            }
            inline constexpr duration& operator%=(const duration& rhs)
            {
                __rep_ %= rhs.count();
                return *this;
            }

            // special values

            inline static constexpr duration zero() noexcept
            {
                return duration(duration_values<rep>::zero());
            }
            inline static constexpr duration min() noexcept
            {
                return duration(duration_values<rep>::min());
            }
            inline static constexpr duration max() noexcept
            {
                return duration(duration_values<rep>::max());
            }
        };

        typedef duration<long long, nano> nanoseconds;
        typedef duration<long long, micro> microseconds;
        typedef duration<long long, milli> milliseconds;
        typedef duration<long long> seconds;
        typedef duration<long, ratio<60>> minutes;
        typedef duration<long, ratio<3600>> hours;
        typedef duration<int, ratio_multiply<ratio<24>, hours::period>> days;
        typedef duration<int, ratio_multiply<ratio<7>, days::period>> weeks;
        typedef duration<int, ratio_multiply<ratio<146097, 400>, days::period>> years;
        typedef duration<int, ratio_divide<years::period, ratio<12>>> months;
        // Duration ==

        template<class _LhsDuration, class _RhsDuration> struct __duration_eq
        {
            inline constexpr bool operator()(const _LhsDuration& __lhs,
                                                                        const _RhsDuration& __rhs) const
            {
                typedef typename common_type<_LhsDuration, _RhsDuration>::type _Ct;
                return _Ct(__lhs).count() == _Ct(__rhs).count();
            }
        };

        template<class _LhsDuration> struct __duration_eq<_LhsDuration, _LhsDuration>
        {
            inline constexpr bool operator()(const _LhsDuration& __lhs,
                                                                        const _LhsDuration& __rhs) const
            {
                return __lhs.count() == __rhs.count();
            }
        };

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr bool operator==(const duration<_Rep1, _Period1>& __lhs,
                                                                           const duration<_Rep2, _Period2>& __rhs)
        {
            return __duration_eq<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>()(__lhs, __rhs);
        }

        // Duration !=

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr bool operator!=(const duration<_Rep1, _Period1>& __lhs,
                                                                           const duration<_Rep2, _Period2>& __rhs)
        {
            return !(__lhs == __rhs);
        }

        // Duration <

        template<class _LhsDuration, class _RhsDuration> struct __duration_lt
        {
            inline constexpr bool operator()(const _LhsDuration& __lhs,
                                                                        const _RhsDuration& __rhs) const
            {
                typedef typename common_type<_LhsDuration, _RhsDuration>::type _Ct;
                return _Ct(__lhs).count() < _Ct(__rhs).count();
            }
        };

        template<class _LhsDuration> struct __duration_lt<_LhsDuration, _LhsDuration>
        {
            inline constexpr bool operator()(const _LhsDuration& __lhs,
                                                                        const _LhsDuration& __rhs) const
            {
                return __lhs.count() < __rhs.count();
            }
        };

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr bool operator<(const duration<_Rep1, _Period1>& __lhs,
                                                                          const duration<_Rep2, _Period2>& __rhs)
        {
            return __duration_lt<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>()(__lhs, __rhs);
        }

        // Duration >

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr bool operator>(const duration<_Rep1, _Period1>& __lhs,
                                                                          const duration<_Rep2, _Period2>& __rhs)
        {
            return __rhs < __lhs;
        }

        // Duration <=

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr bool operator<=(const duration<_Rep1, _Period1>& __lhs,
                                                                           const duration<_Rep2, _Period2>& __rhs)
        {
            return !(__rhs < __lhs);
        }

        // Duration >=

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr bool operator>=(const duration<_Rep1, _Period1>& __lhs,
                                                                           const duration<_Rep2, _Period2>& __rhs)
        {
            return !(__lhs < __rhs);
        }

        // Duration +

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr
            typename common_type<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>::type
            operator+(const duration<_Rep1, _Period1>& __lhs, const duration<_Rep2, _Period2>& __rhs)
        {
            typedef typename common_type<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>::type _Cd;
            return _Cd(_Cd(__lhs).count() + _Cd(__rhs).count());
        }

        // Duration -

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr
            typename common_type<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>::type
            operator-(const duration<_Rep1, _Period1>& __lhs, const duration<_Rep2, _Period2>& __rhs)
        {
            typedef typename common_type<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>::type _Cd;
            return _Cd(_Cd(__lhs).count() - _Cd(__rhs).count());
        }

        // Duration *

        template<class _Rep1, class _Period, class _Rep2>
        inline constexpr
            typename enable_if<is_convertible<_Rep2, typename common_type<_Rep1, _Rep2>::type>::value,
                               duration<typename common_type<_Rep1, _Rep2>::type, _Period>>::type
            operator*(const duration<_Rep1, _Period>& __d, const _Rep2& __s)
        {
            typedef typename common_type<_Rep1, _Rep2>::type _Cr;
            typedef duration<_Cr, _Period> _Cd;
            return _Cd(_Cd(__d).count() * static_cast<_Cr>(__s));
        }

        template<class _Rep1, class _Period, class _Rep2>
        inline constexpr
            typename enable_if<is_convertible<_Rep1, typename common_type<_Rep1, _Rep2>::type>::value,
                               duration<typename common_type<_Rep1, _Rep2>::type, _Period>>::type
            operator*(const _Rep1& __s, const duration<_Rep2, _Period>& __d)
        {
            return __d * __s;
        }

        // Duration /

        template<class _Rep1, class _Period, class _Rep2>
        inline constexpr
            typename enable_if<!__is_duration<_Rep2>::value
                                   && is_convertible<_Rep2, typename common_type<_Rep1, _Rep2>::type>::value,
                               duration<typename common_type<_Rep1, _Rep2>::type, _Period>>::type
            operator/(const duration<_Rep1, _Period>& __d, const _Rep2& __s)
        {
            typedef typename common_type<_Rep1, _Rep2>::type _Cr;
            typedef duration<_Cr, _Period> _Cd;
            return _Cd(_Cd(__d).count() / static_cast<_Cr>(__s));
        }

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr typename common_type<_Rep1, _Rep2>::type
        operator/(const duration<_Rep1, _Period1>& __lhs, const duration<_Rep2, _Period2>& __rhs)
        {
            typedef typename common_type<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>::type _Ct;
            return _Ct(__lhs).count() / _Ct(__rhs).count();
        }

        // Duration %

        template<class _Rep1, class _Period, class _Rep2>
        inline constexpr
            typename enable_if<!__is_duration<_Rep2>::value
                                   && is_convertible<_Rep2, typename common_type<_Rep1, _Rep2>::type>::value,
                               duration<typename common_type<_Rep1, _Rep2>::type, _Period>>::type
            operator%(const duration<_Rep1, _Period>& __d, const _Rep2& __s)
        {
            typedef typename common_type<_Rep1, _Rep2>::type _Cr;
            typedef duration<_Cr, _Period> _Cd;
            return _Cd(_Cd(__d).count() % static_cast<_Cr>(__s));
        }

        template<class _Rep1, class _Period1, class _Rep2, class _Period2>
        inline constexpr
            typename common_type<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>::type
            operator%(const duration<_Rep1, _Period1>& __lhs, const duration<_Rep2, _Period2>& __rhs)
        {
            typedef typename common_type<_Rep1, _Rep2>::type _Cr;
            typedef typename common_type<duration<_Rep1, _Period1>, duration<_Rep2, _Period2>>::type _Cd;
            return _Cd(static_cast<_Cr>(_Cd(__lhs).count()) % static_cast<_Cr>(_Cd(__rhs).count()));
        }

        //////////////////////////////////////////////////////////
        ///////////////////// time_point /////////////////////////
        //////////////////////////////////////////////////////////

        template<class _Clock, class _Duration = typename _Clock::duration> class time_point
        {
            static_assert(__is_duration<_Duration>::value,
                          "Second template parameter of time_point must be a std::chrono::duration");

        public:
            typedef _Clock clock;
            typedef _Duration duration;
            typedef typename duration::rep rep;
            typedef typename duration::period period;

        private:
            duration __d_;

        public:
            inline constexpr time_point()
                : __d_(duration::zero())
            {
            }
            inline constexpr explicit time_point(const duration& __d)
                : __d_(__d)
            {
            }

            // conversions
            template<class _Duration2>
            inline constexpr
            time_point(const time_point<clock, _Duration2>& t,
                       typename enable_if<is_convertible<_Duration2, duration>::value>::type* = nullptr)
                : __d_(t.time_since_epoch())
            {
            }

            // observer

            inline constexpr duration time_since_epoch() const { return __d_; }

            // arithmetic

            inline constexpr time_point& operator+=(const duration& __d)
            {
                __d_ += __d;
                return *this;
            }
            inline constexpr time_point& operator-=(const duration& __d)
            {
                __d_ -= __d;
                return *this;
            }

            // special values

            inline static constexpr time_point min() noexcept
            {
                return time_point(duration::min());
            }
            inline static constexpr time_point max() noexcept
            {
                return time_point(duration::max());
            }
        };

    } // chrono

    template<class _Clock, class _Duration1, class _Duration2>
    struct common_type<chrono::time_point<_Clock, _Duration1>, chrono::time_point<_Clock, _Duration2>>
    {
        typedef chrono::time_point<_Clock, typename common_type<_Duration1, _Duration2>::type> type;
    };

    namespace chrono
    {

        template<class _ToDuration, class _Clock, class _Duration>
        inline constexpr time_point<_Clock, _ToDuration>
        time_point_cast(const time_point<_Clock, _Duration>& __t)
        {
            return time_point<_Clock, _ToDuration>(std::chrono::duration_cast<_ToDuration>(__t.time_since_epoch()));
        }

        template<class _ToDuration, class _Clock, class _Duration>
        inline constexpr
            typename enable_if<__is_duration<_ToDuration>::value, time_point<_Clock, _ToDuration>>::type
            floor(const time_point<_Clock, _Duration>& __t)
        {
            return time_point<_Clock, _ToDuration> {floor<_ToDuration>(__t.time_since_epoch())};
        }

        template<class _ToDuration, class _Clock, class _Duration>
        inline constexpr
            typename enable_if<__is_duration<_ToDuration>::value, time_point<_Clock, _ToDuration>>::type
            ceil(const time_point<_Clock, _Duration>& __t)
        {
            return time_point<_Clock, _ToDuration> {ceil<_ToDuration>(__t.time_since_epoch())};
        }

        template<class _ToDuration, class _Clock, class _Duration>
        inline constexpr
            typename enable_if<__is_duration<_ToDuration>::value, time_point<_Clock, _ToDuration>>::type
            round(const time_point<_Clock, _Duration>& __t)
        {
            return time_point<_Clock, _ToDuration> {round<_ToDuration>(__t.time_since_epoch())};
        }

        template<class _Rep, class _Period>
        inline constexpr
            typename enable_if<numeric_limits<_Rep>::is_signed, duration<_Rep, _Period>>::type
            abs(duration<_Rep, _Period> __d)
        {
            return __d >= __d.zero() ? +__d : -__d;
        }

        // time_point ==

        template<class _Clock, class _Duration1, class _Duration2>
        inline constexpr bool
        operator==(const time_point<_Clock, _Duration1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return __lhs.time_since_epoch() == __rhs.time_since_epoch();
        }

        // time_point !=

        template<class _Clock, class _Duration1, class _Duration2>
        inline constexpr bool
        operator!=(const time_point<_Clock, _Duration1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return !(__lhs == __rhs);
        }

        // time_point <

        template<class _Clock, class _Duration1, class _Duration2>
        inline constexpr bool
        operator<(const time_point<_Clock, _Duration1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return __lhs.time_since_epoch() < __rhs.time_since_epoch();
        }

        // time_point >

        template<class _Clock, class _Duration1, class _Duration2>
        inline constexpr bool
        operator>(const time_point<_Clock, _Duration1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return __rhs < __lhs;
        }

        // time_point <=

        template<class _Clock, class _Duration1, class _Duration2>
        inline constexpr bool
        operator<=(const time_point<_Clock, _Duration1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return !(__rhs < __lhs);
        }

        // time_point >=

        template<class _Clock, class _Duration1, class _Duration2>
        inline constexpr bool
        operator>=(const time_point<_Clock, _Duration1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return !(__lhs < __rhs);
        }

        // time_point operator+(time_point x, duration y);

        template<class _Clock, class _Duration1, class _Rep2, class _Period2>
        inline constexpr
            time_point<_Clock, typename common_type<_Duration1, duration<_Rep2, _Period2>>::type>
            operator+(const time_point<_Clock, _Duration1>& __lhs, const duration<_Rep2, _Period2>& __rhs)
        {
            typedef time_point<_Clock, typename common_type<_Duration1, duration<_Rep2, _Period2>>::type> _Tr;
            return _Tr(__lhs.time_since_epoch() + __rhs);
        }

        // time_point operator+(duration x, time_point y);

        template<class _Rep1, class _Period1, class _Clock, class _Duration2>
        inline constexpr
            time_point<_Clock, typename common_type<duration<_Rep1, _Period1>, _Duration2>::type>
            operator+(const duration<_Rep1, _Period1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return __rhs + __lhs;
        }

        // time_point operator-(time_point x, duration y);

        template<class _Clock, class _Duration1, class _Rep2, class _Period2>
        inline constexpr
            time_point<_Clock, typename common_type<_Duration1, duration<_Rep2, _Period2>>::type>
            operator-(const time_point<_Clock, _Duration1>& __lhs, const duration<_Rep2, _Period2>& __rhs)
        {
            typedef time_point<_Clock, typename common_type<_Duration1, duration<_Rep2, _Period2>>::type> _Ret;
            return _Ret(__lhs.time_since_epoch() - __rhs);
        }

        // duration operator-(time_point x, time_point y);

        template<class _Clock, class _Duration1, class _Duration2>
        inline constexpr
            typename common_type<_Duration1, _Duration2>::type
            operator-(const time_point<_Clock, _Duration1>& __lhs, const time_point<_Clock, _Duration2>& __rhs)
        {
            return __lhs.time_since_epoch() - __rhs.time_since_epoch();
        }

        //////////////////////////////////////////////////////////
        /////////////////////// clocks ///////////////////////////
        //////////////////////////////////////////////////////////

        class  system_clock
        {
        public:
            typedef microseconds duration;
            typedef duration::rep rep;
            typedef duration::period period;
            typedef chrono::time_point<system_clock> time_point;
            static constexpr const bool is_steady = false;

            static time_point now() noexcept;
            static time_t to_time_t(const time_point& __t) noexcept;
            static time_point from_time_t(time_t __t) noexcept;
        };

#ifndef _LIBCPP_HAS_NO_MONOTONIC_CLOCK
        class  steady_clock
        {
        public:
            typedef nanoseconds duration;
            typedef duration::rep rep;
            typedef duration::period period;
            typedef chrono::time_point<steady_clock, duration> time_point;
            static constexpr const bool is_steady = true;

            static time_point now() noexcept;
        };

        typedef steady_clock high_resolution_clock;
#else
        typedef system_clock high_resolution_clock;
#endif

        // [time.clock.file], type file_clock
        // using file_clock = std_FS::_FilesystemClock;

        // template<class _Duration> using file_time = time_point<file_clock, _Duration>;

        template<class _Duration> using sys_time = time_point<system_clock, _Duration>;
        using sys_seconds = sys_time<seconds>;
        using sys_days = sys_time<days>;

        struct local_t
        {
        };
        template<class Duration> using local_time = time_point<local_t, Duration>;
        using local_seconds = local_time<seconds>;
        using local_days = local_time<days>;

        struct last_spec
        {
            explicit last_spec() = default;
        };

        class day
        {
        private:
            unsigned char __d;

        public:
            day() = default;
            explicit inline constexpr day(unsigned __val) noexcept
                : __d(static_cast<unsigned char>(__val))
            {
            }
            inline constexpr day& operator++() noexcept
            {
                ++__d;
                return *this;
            }
            inline constexpr day operator++(int) noexcept
            {
                day __tmp = *this;
                ++(*this);
                return __tmp;
            }
            inline constexpr day& operator--() noexcept
            {
                --__d;
                return *this;
            }
            inline constexpr day operator--(int) noexcept
            {
                day __tmp = *this;
                --(*this);
                return __tmp;
            }
            constexpr day& operator+=(const days& __dd) noexcept;
            constexpr day& operator-=(const days& __dd) noexcept;
            explicit inline constexpr operator unsigned() const noexcept { return __d; }
            inline constexpr bool ok() const noexcept { return __d >= 1 && __d <= 31; }
        };

        inline constexpr bool operator==(const day& __lhs, const day& __rhs) noexcept
        {
            return static_cast<unsigned>(__lhs) == static_cast<unsigned>(__rhs);
        }

        inline constexpr bool operator!=(const day& __lhs, const day& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const day& __lhs, const day& __rhs) noexcept
        {
            return static_cast<unsigned>(__lhs) < static_cast<unsigned>(__rhs);
        }

        inline constexpr bool operator>(const day& __lhs, const day& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const day& __lhs, const day& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const day& __lhs, const day& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        inline constexpr day operator+(const day& __lhs, const days& __rhs) noexcept
        {
            return day(static_cast<unsigned>(__lhs) + __rhs.count());
        }

        inline constexpr day operator+(const days& __lhs, const day& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr day operator-(const day& __lhs, const days& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        inline constexpr days operator-(const day& __lhs, const day& __rhs) noexcept
        {
            return days(static_cast<int>(static_cast<unsigned>(__lhs))
                        - static_cast<int>(static_cast<unsigned>(__rhs)));
        }

        inline constexpr day& day::operator+=(const days& __dd) noexcept
        {
            *this = *this + __dd;
            return *this;
        }

        inline constexpr day& day::operator-=(const days& __dd) noexcept
        {
            *this = *this - __dd;
            return *this;
        }

        class month
        {
        private:
            unsigned char __m;

        public:
            month() = default;
            explicit inline constexpr month(unsigned __val) noexcept
                : __m(static_cast<unsigned char>(__val))
            {
            }
            inline constexpr month& operator++() noexcept
            {
                ++__m;
                return *this;
            }
            inline constexpr month operator++(int) noexcept
            {
                month __tmp = *this;
                ++(*this);
                return __tmp;
            }
            inline constexpr month& operator--() noexcept
            {
                --__m;
                return *this;
            }
            inline constexpr month operator--(int) noexcept
            {
                month __tmp = *this;
                --(*this);
                return __tmp;
            }
            constexpr month& operator+=(const months& __m1) noexcept;
            constexpr month& operator-=(const months& __m1) noexcept;
            explicit inline constexpr operator unsigned() const noexcept { return __m; }
            inline constexpr bool ok() const noexcept { return __m >= 1 && __m <= 12; }
        };

        inline constexpr bool operator==(const month& __lhs, const month& __rhs) noexcept
        {
            return static_cast<unsigned>(__lhs) == static_cast<unsigned>(__rhs);
        }

        inline constexpr bool operator!=(const month& __lhs, const month& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const month& __lhs, const month& __rhs) noexcept
        {
            return static_cast<unsigned>(__lhs) < static_cast<unsigned>(__rhs);
        }

        inline constexpr bool operator>(const month& __lhs, const month& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const month& __lhs, const month& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const month& __lhs, const month& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        inline constexpr month operator+(const month& __lhs, const months& __rhs) noexcept
        {
            auto const __mu = static_cast<long long>(static_cast<unsigned>(__lhs)) + (__rhs.count() - 1);
            auto const __yr = (__mu >= 0 ? __mu : __mu - 11) / 12;
            return month {static_cast<unsigned>(__mu - __yr * 12 + 1)};
        }

        inline constexpr month operator+(const months& __lhs, const month& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr month operator-(const month& __lhs, const months& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        inline constexpr months operator-(const month& __lhs, const month& __rhs) noexcept
        {
            auto const __dm = static_cast<unsigned>(__lhs) - static_cast<unsigned>(__rhs);
            return months(__dm <= 11 ? __dm : __dm + 12);
        }

        inline constexpr month& month::operator+=(const months& __dm) noexcept
        {
            *this = *this + __dm;
            return *this;
        }

        inline constexpr month& month::operator-=(const months& __dm) noexcept
        {
            *this = *this - __dm;
            return *this;
        }

        class year
        {
        private:
            short __y;

        public:
            year() = default;
            explicit inline constexpr year(int __val) noexcept
                : __y(static_cast<short>(__val))
            {
            }

            inline constexpr year& operator++() noexcept
            {
                ++__y;
                return *this;
            }
            inline constexpr year operator++(int) noexcept
            {
                year __tmp = *this;
                ++(*this);
                return __tmp;
            }
            inline constexpr year& operator--() noexcept
            {
                --__y;
                return *this;
            }
            inline constexpr year operator--(int) noexcept
            {
                year __tmp = *this;
                --(*this);
                return __tmp;
            }
            constexpr year& operator+=(const years& __dy) noexcept;
            constexpr year& operator-=(const years& __dy) noexcept;
            inline constexpr year operator+() const noexcept { return *this; }
            inline constexpr year operator-() const noexcept { return year {-__y}; }

            inline constexpr bool is_leap() const noexcept
            {
                return __y % 4 == 0 && (__y % 100 != 0 || __y % 400 == 0);
            }
            explicit inline constexpr operator int() const noexcept { return __y; }
            constexpr bool ok() const noexcept;
            static inline constexpr year min() noexcept { return year {-32767}; }
            static inline constexpr year max() noexcept { return year {32767}; }
        };

        inline constexpr bool operator==(const year& __lhs, const year& __rhs) noexcept
        {
            return static_cast<int>(__lhs) == static_cast<int>(__rhs);
        }

        inline constexpr bool operator!=(const year& __lhs, const year& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const year& __lhs, const year& __rhs) noexcept
        {
            return static_cast<int>(__lhs) < static_cast<int>(__rhs);
        }

        inline constexpr bool operator>(const year& __lhs, const year& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const year& __lhs, const year& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const year& __lhs, const year& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        inline constexpr year operator+(const year& __lhs, const years& __rhs) noexcept
        {
            return year(static_cast<int>(__lhs) + __rhs.count());
        }

        inline constexpr year operator+(const years& __lhs, const year& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year operator-(const year& __lhs, const years& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        inline constexpr years operator-(const year& __lhs, const year& __rhs) noexcept
        {
            return years {static_cast<int>(__lhs) - static_cast<int>(__rhs)};
        }

        inline constexpr year& year::operator+=(const years& __dy) noexcept
        {
            *this = *this + __dy;
            return *this;
        }

        inline constexpr year& year::operator-=(const years& __dy) noexcept
        {
            *this = *this - __dy;
            return *this;
        }

        inline constexpr bool year::ok() const noexcept
        {
            return static_cast<int>(min()) <= __y && __y <= static_cast<int>(max());
        }

        class weekday_indexed;
        class weekday_last;

        class weekday
        {
        private:
            unsigned char __wd;

        public:
            weekday() = default;
            inline explicit constexpr weekday(unsigned __val) noexcept
                : __wd(static_cast<unsigned char>(__val == 7 ? 0 : __val))
            {
            }
            inline constexpr weekday(const sys_days& __sysd) noexcept
                : __wd(__weekday_from_days(__sysd.time_since_epoch().count()))
            {
            }
            inline explicit constexpr weekday(const local_days& __locd) noexcept
                : __wd(__weekday_from_days(__locd.time_since_epoch().count()))
            {
            }

            inline constexpr weekday& operator++() noexcept
            {
                __wd = (__wd == 6 ? 0 : __wd + 1);
                return *this;
            }
            inline constexpr weekday operator++(int) noexcept
            {
                weekday __tmp = *this;
                ++(*this);
                return __tmp;
            }
            inline constexpr weekday& operator--() noexcept
            {
                __wd = (__wd == 0 ? 6 : __wd - 1);
                return *this;
            }
            inline constexpr weekday operator--(int) noexcept
            {
                weekday __tmp = *this;
                --(*this);
                return __tmp;
            }
            constexpr weekday& operator+=(const days& __dd) noexcept;
            constexpr weekday& operator-=(const days& __dd) noexcept;
            inline constexpr unsigned c_encoding() const noexcept { return __wd; }
            inline constexpr unsigned iso_encoding() const noexcept { return __wd == 0u ? 7 : __wd; }
            inline constexpr bool ok() const noexcept { return __wd <= 6; }
            constexpr weekday_indexed operator[](unsigned __index) const noexcept;
            constexpr weekday_last operator[](last_spec) const noexcept;

            // TODO: Make private?
            static constexpr unsigned char __weekday_from_days(int __days) noexcept;
        };

        // https://howardhinnant.github.io/date_algorithms.html#weekday_from_days
        inline constexpr unsigned char weekday::__weekday_from_days(int __days) noexcept
        {
            return static_cast<unsigned char>(
                static_cast<unsigned>(__days >= -4 ? (__days + 4) % 7 : (__days + 5) % 7 + 6));
        }

        inline constexpr bool operator==(const weekday& __lhs, const weekday& __rhs) noexcept
        {
            return __lhs.c_encoding() == __rhs.c_encoding();
        }

        inline constexpr bool operator!=(const weekday& __lhs, const weekday& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const weekday& __lhs, const weekday& __rhs) noexcept
        {
            return __lhs.c_encoding() < __rhs.c_encoding();
        }

        inline constexpr bool operator>(const weekday& __lhs, const weekday& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const weekday& __lhs, const weekday& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const weekday& __lhs, const weekday& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        constexpr weekday operator+(const weekday& __lhs, const days& __rhs) noexcept
        {
            auto const __mu = static_cast<long long>(__lhs.c_encoding()) + __rhs.count();
            auto const __yr = (__mu >= 0 ? __mu : __mu - 6) / 7;
            return weekday {static_cast<unsigned>(__mu - __yr * 7)};
        }

        constexpr weekday operator+(const days& __lhs, const weekday& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        constexpr weekday operator-(const weekday& __lhs, const days& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        constexpr days operator-(const weekday& __lhs, const weekday& __rhs) noexcept
        {
            const int __wdu = __lhs.c_encoding() - __rhs.c_encoding();
            const int __wk = (__wdu >= 0 ? __wdu : __wdu - 6) / 7;
            return days {__wdu - __wk * 7};
        }

        inline constexpr weekday& weekday::operator+=(const days& __dd) noexcept
        {
            *this = *this + __dd;
            return *this;
        }

        inline constexpr weekday& weekday::operator-=(const days& __dd) noexcept
        {
            *this = *this - __dd;
            return *this;
        }

        class weekday_indexed
        {
        private:
            std::chrono::weekday __wd;
            unsigned char __idx;

        public:
            weekday_indexed() = default;
            inline constexpr weekday_indexed(const std::chrono::weekday& __wdval, unsigned __idxval) noexcept
                : __wd {__wdval}
                , __idx(__idxval)
            {
            }
            inline constexpr std::chrono::weekday weekday() const noexcept { return __wd; }
            inline constexpr unsigned index() const noexcept { return __idx; }
            inline constexpr bool ok() const noexcept { return __wd.ok() && __idx >= 1 && __idx <= 5; }
        };

        inline constexpr bool operator==(const weekday_indexed& __lhs, const weekday_indexed& __rhs) noexcept
        {
            return __lhs.weekday() == __rhs.weekday() && __lhs.index() == __rhs.index();
        }

        inline constexpr bool operator!=(const weekday_indexed& __lhs, const weekday_indexed& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        class weekday_last
        {
        private:
            std::chrono::weekday __wd;

        public:
            explicit constexpr weekday_last(const std::chrono::weekday& __val) noexcept
                : __wd {__val}
            {
            }
            constexpr std::chrono::weekday weekday() const noexcept { return __wd; }
            constexpr bool ok() const noexcept { return __wd.ok(); }
        };

        inline constexpr bool operator==(const weekday_last& __lhs, const weekday_last& __rhs) noexcept
        {
            return __lhs.weekday() == __rhs.weekday();
        }

        inline constexpr bool operator!=(const weekday_last& __lhs, const weekday_last& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr weekday_indexed weekday::operator[](unsigned __index) const noexcept
        {
            return weekday_indexed {*this, __index};
        }

        inline constexpr weekday_last weekday::operator[](last_spec) const noexcept
        {
            return weekday_last {*this};
        }

        inline constexpr last_spec last {};
        inline constexpr weekday Sunday {0};
        inline constexpr weekday Monday {1};
        inline constexpr weekday Tuesday {2};
        inline constexpr weekday Wednesday {3};
        inline constexpr weekday Thursday {4};
        inline constexpr weekday Friday {5};
        inline constexpr weekday Saturday {6};

        inline constexpr month January {1};
        inline constexpr month February {2};
        inline constexpr month March {3};
        inline constexpr month April {4};
        inline constexpr month May {5};
        inline constexpr month June {6};
        inline constexpr month July {7};
        inline constexpr month August {8};
        inline constexpr month September {9};
        inline constexpr month October {10};
        inline constexpr month November {11};
        inline constexpr month December {12};

        class month_day
        {
        private:
            chrono::month __m;
            chrono::day __d;

        public:
            month_day() = default;
            constexpr month_day(const chrono::month& __mval, const chrono::day& __dval) noexcept
                : __m {__mval}
                , __d {__dval}
            {
            }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr chrono::day day() const noexcept { return __d; }
            constexpr bool ok() const noexcept;
        };

        inline constexpr bool month_day::ok() const noexcept
        {
            if(!__m.ok())
                return false;
            const unsigned __dval = static_cast<unsigned>(__d);
            if(__dval < 1 || __dval > 31)
                return false;
            if(__dval <= 29)
                return true;
            //  Now we've got either 30 or 31
            const unsigned __mval = static_cast<unsigned>(__m);
            if(__mval == 2)
                return false;
            if(__mval == 4 || __mval == 6 || __mval == 9 || __mval == 11)
                return __dval == 30;
            return true;
        }

        inline constexpr bool operator==(const month_day& __lhs, const month_day& __rhs) noexcept
        {
            return __lhs.month() == __rhs.month() && __lhs.day() == __rhs.day();
        }

        inline constexpr bool operator!=(const month_day& __lhs, const month_day& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr month_day operator/(const month& __lhs, const day& __rhs) noexcept
        {
            return month_day {__lhs, __rhs};
        }

        constexpr month_day operator/(const day& __lhs, const month& __rhs) noexcept
        {
            return __rhs / __lhs;
        }

        inline constexpr month_day operator/(const month& __lhs, int __rhs) noexcept
        {
            return __lhs / day(__rhs);
        }

        constexpr month_day operator/(int __lhs, const day& __rhs) noexcept
        {
            return month(__lhs) / __rhs;
        }

        constexpr month_day operator/(const day& __lhs, int __rhs) noexcept
        {
            return month(__rhs) / __lhs;
        }

        inline constexpr bool operator<(const month_day& __lhs, const month_day& __rhs) noexcept
        {
            return __lhs.month() != __rhs.month() ? __lhs.month() < __rhs.month() : __lhs.day() < __rhs.day();
        }

        inline constexpr bool operator>(const month_day& __lhs, const month_day& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const month_day& __lhs, const month_day& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const month_day& __lhs, const month_day& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        class month_day_last
        {
        private:
            chrono::month __m;

        public:
            explicit constexpr month_day_last(const chrono::month& __val) noexcept
                : __m {__val}
            {
            }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr bool ok() const noexcept { return __m.ok(); }
        };

        inline constexpr bool operator==(const month_day_last& __lhs, const month_day_last& __rhs) noexcept
        {
            return __lhs.month() == __rhs.month();
        }

        inline constexpr bool operator!=(const month_day_last& __lhs, const month_day_last& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const month_day_last& __lhs, const month_day_last& __rhs) noexcept
        {
            return __lhs.month() < __rhs.month();
        }

        inline constexpr bool operator>(const month_day_last& __lhs, const month_day_last& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const month_day_last& __lhs, const month_day_last& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const month_day_last& __lhs, const month_day_last& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        inline constexpr month_day_last operator/(const month& __lhs, last_spec) noexcept
        {
            return month_day_last {__lhs};
        }

        inline constexpr month_day_last operator/(last_spec, const month& __rhs) noexcept
        {
            return month_day_last {__rhs};
        }

        inline constexpr month_day_last operator/(int __lhs, last_spec) noexcept
        {
            return month_day_last {month(__lhs)};
        }

        inline constexpr month_day_last operator/(last_spec, int __rhs) noexcept
        {
            return month_day_last {month(__rhs)};
        }

        class month_weekday
        {
        private:
            chrono::month __m;
            chrono::weekday_indexed __wdi;

        public:
            month_weekday() = default;
            constexpr month_weekday(const chrono::month& __mval, const chrono::weekday_indexed& __wdival) noexcept
                : __m {__mval}
                , __wdi {__wdival}
            {
            }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr chrono::weekday_indexed weekday_indexed() const noexcept { return __wdi; }
            inline constexpr bool ok() const noexcept { return __m.ok() && __wdi.ok(); }
        };

        inline constexpr bool operator==(const month_weekday& __lhs, const month_weekday& __rhs) noexcept
        {
            return __lhs.month() == __rhs.month() && __lhs.weekday_indexed() == __rhs.weekday_indexed();
        }

        inline constexpr bool operator!=(const month_weekday& __lhs, const month_weekday& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr month_weekday operator/(const month& __lhs, const weekday_indexed& __rhs) noexcept
        {
            return month_weekday {__lhs, __rhs};
        }

        inline constexpr month_weekday operator/(int __lhs, const weekday_indexed& __rhs) noexcept
        {
            return month_weekday {month(__lhs), __rhs};
        }

        inline constexpr month_weekday operator/(const weekday_indexed& __lhs, const month& __rhs) noexcept
        {
            return month_weekday {__rhs, __lhs};
        }

        inline constexpr month_weekday operator/(const weekday_indexed& __lhs, int __rhs) noexcept
        {
            return month_weekday {month(__rhs), __lhs};
        }

        class month_weekday_last
        {
            chrono::month __m;
            chrono::weekday_last __wdl;

        public:
            constexpr month_weekday_last(const chrono::month& __mval, const chrono::weekday_last& __wdlval) noexcept
                : __m {__mval}
                , __wdl {__wdlval}
            {
            }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr chrono::weekday_last weekday_last() const noexcept { return __wdl; }
            inline constexpr bool ok() const noexcept { return __m.ok() && __wdl.ok(); }
        };

        inline constexpr bool operator==(const month_weekday_last& __lhs, const month_weekday_last& __rhs) noexcept
        {
            return __lhs.month() == __rhs.month() && __lhs.weekday_last() == __rhs.weekday_last();
        }

        inline constexpr bool operator!=(const month_weekday_last& __lhs, const month_weekday_last& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr month_weekday_last operator/(const month& __lhs, const weekday_last& __rhs) noexcept
        {
            return month_weekday_last {__lhs, __rhs};
        }

        inline constexpr month_weekday_last operator/(int __lhs, const weekday_last& __rhs) noexcept
        {
            return month_weekday_last {month(__lhs), __rhs};
        }

        inline constexpr month_weekday_last operator/(const weekday_last& __lhs, const month& __rhs) noexcept
        {
            return month_weekday_last {__rhs, __lhs};
        }

        inline constexpr month_weekday_last operator/(const weekday_last& __lhs, int __rhs) noexcept
        {
            return month_weekday_last {month(__rhs), __lhs};
        }

        class year_month
        {
            chrono::year __y;
            chrono::month __m;

        public:
            year_month() = default;
            constexpr year_month(const chrono::year& __yval, const chrono::month& __mval) noexcept
                : __y {__yval}
                , __m {__mval}
            {
            }
            inline constexpr chrono::year year() const noexcept { return __y; }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr year_month& operator+=(const months& __dm) noexcept
            {
                this->__m += __dm;
                return *this;
            }
            inline constexpr year_month& operator-=(const months& __dm) noexcept
            {
                this->__m -= __dm;
                return *this;
            }
            inline constexpr year_month& operator+=(const years& __dy) noexcept
            {
                this->__y += __dy;
                return *this;
            }
            inline constexpr year_month& operator-=(const years& __dy) noexcept
            {
                this->__y -= __dy;
                return *this;
            }
            inline constexpr bool ok() const noexcept { return __y.ok() && __m.ok(); }
        };

        inline constexpr year_month operator/(const year& __y, const month& __m) noexcept
        {
            return year_month {__y, __m};
        }

        inline constexpr year_month operator/(const year& __y, int __m) noexcept
        {
            return year_month {__y, month(__m)};
        }

        inline constexpr bool operator==(const year_month& __lhs, const year_month& __rhs) noexcept
        {
            return __lhs.year() == __rhs.year() && __lhs.month() == __rhs.month();
        }

        inline constexpr bool operator!=(const year_month& __lhs, const year_month& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const year_month& __lhs, const year_month& __rhs) noexcept
        {
            return __lhs.year() != __rhs.year() ? __lhs.year() < __rhs.year() : __lhs.month() < __rhs.month();
        }

        inline constexpr bool operator>(const year_month& __lhs, const year_month& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const year_month& __lhs, const year_month& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const year_month& __lhs, const year_month& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        constexpr year_month operator+(const year_month& __lhs, const months& __rhs) noexcept
        {
            int __dmi = static_cast<int>(static_cast<unsigned>(__lhs.month())) - 1 + __rhs.count();
            const int __dy = (__dmi >= 0 ? __dmi : __dmi - 11) / 12;
            __dmi = __dmi - __dy * 12 + 1;
            return (__lhs.year() + years(__dy)) / month(static_cast<unsigned>(__dmi));
        }

        constexpr year_month operator+(const months& __lhs, const year_month& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        constexpr year_month operator+(const year_month& __lhs, const years& __rhs) noexcept
        {
            return (__lhs.year() + __rhs) / __lhs.month();
        }

        constexpr year_month operator+(const years& __lhs, const year_month& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        constexpr months operator-(const year_month& __lhs, const year_month& __rhs) noexcept
        {
            return (__lhs.year() - __rhs.year())
                   + months(static_cast<unsigned>(__lhs.month()) - static_cast<unsigned>(__rhs.month()));
        }

        constexpr year_month operator-(const year_month& __lhs, const months& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        constexpr year_month operator-(const year_month& __lhs, const years& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        class year_month_day_last;

        class year_month_day
        {
        private:
            chrono::year __y;
            chrono::month __m;
            chrono::day __d;

        public:
            year_month_day() = default;
            inline constexpr year_month_day(const chrono::year& __yval, const chrono::month& __mval,
                                            const chrono::day& __dval) noexcept
                : __y {__yval}
                , __m {__mval}
                , __d {__dval}
            {
            }
            constexpr year_month_day(const year_month_day_last& __ymdl) noexcept;
            inline constexpr year_month_day(const sys_days& __sysd) noexcept
                : year_month_day(__from_days(__sysd.time_since_epoch()))
            {
            }
            inline explicit constexpr year_month_day(const local_days& __locd) noexcept
                : year_month_day(__from_days(__locd.time_since_epoch()))
            {
            }

            constexpr year_month_day& operator+=(const months& __dm) noexcept;
            constexpr year_month_day& operator-=(const months& __dm) noexcept;
            constexpr year_month_day& operator+=(const years& __dy) noexcept;
            constexpr year_month_day& operator-=(const years& __dy) noexcept;

            inline constexpr chrono::year year() const noexcept { return __y; }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr chrono::day day() const noexcept { return __d; }
            inline constexpr operator sys_days() const noexcept { return sys_days {__to_days()}; }
            inline explicit constexpr operator local_days() const noexcept { return local_days {__to_days()}; }

            constexpr bool ok() const noexcept;

            static constexpr year_month_day __from_days(days __d) noexcept;
            constexpr days __to_days() const noexcept;
        };

        // https://howardhinnant.github.io/date_algorithms.html#civil_from_days
        inline constexpr year_month_day year_month_day::__from_days(days __d) noexcept
        {
            static_assert(std::numeric_limits<unsigned>::digits >= 18, "");
            static_assert(std::numeric_limits<int>::digits >= 20, "");
            const int __z = __d.count() + 719468;
            const int __era = (__z >= 0 ? __z : __z - 146096) / 146097;
            const unsigned __doe = static_cast<unsigned>(__z - __era * 146097);                   // [0, 146096]
            const unsigned __yoe = (__doe - __doe / 1460 + __doe / 36524 - __doe / 146096) / 365; // [0, 399]
            const int __yr = static_cast<int>(__yoe) + __era * 400;
            const unsigned __doy = __doe - (365 * __yoe + __yoe / 4 - __yoe / 100); // [0, 365]
            const unsigned __mp = (5 * __doy + 2) / 153;                            // [0, 11]
            const unsigned __dy = __doy - (153 * __mp + 2) / 5 + 1;                 // [1, 31]
            const unsigned __mth = __mp + (__mp < 10 ? 3 : -9);                     // [1, 12]
            return year_month_day {chrono::year {__yr + (__mth <= 2)}, chrono::month {__mth}, chrono::day {__dy}};
        }

        // https://howardhinnant.github.io/date_algorithms.html#days_from_civil
        inline constexpr days year_month_day::__to_days() const noexcept
        {
            static_assert(std::numeric_limits<unsigned>::digits >= 18, "");
            static_assert(std::numeric_limits<int>::digits >= 20, "");

            const int __yr = static_cast<int>(__y) - (__m <= February);
            const unsigned __mth = static_cast<unsigned>(__m);
            const unsigned __dy = static_cast<unsigned>(__d);

            const int __era = (__yr >= 0 ? __yr : __yr - 399) / 400;
            const unsigned __yoe = static_cast<unsigned>(__yr - __era * 400);                 // [0, 399]
            const unsigned __doy = (153 * (__mth + (__mth > 2 ? -3 : 9)) + 2) / 5 + __dy - 1; // [0, 365]
            const unsigned __doe = __yoe * 365 + __yoe / 4 - __yoe / 100 + __doy;             // [0, 146096]
            return days {__era * 146097 + static_cast<int>(__doe) - 719468};
        }

        inline constexpr bool operator==(const year_month_day& __lhs, const year_month_day& __rhs) noexcept
        {
            return __lhs.year() == __rhs.year() && __lhs.month() == __rhs.month() && __lhs.day() == __rhs.day();
        }

        inline constexpr bool operator!=(const year_month_day& __lhs, const year_month_day& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const year_month_day& __lhs, const year_month_day& __rhs) noexcept
        {
            if(__lhs.year() < __rhs.year())
                return true;
            if(__lhs.year() > __rhs.year())
                return false;
            if(__lhs.month() < __rhs.month())
                return true;
            if(__lhs.month() > __rhs.month())
                return false;
            return __lhs.day() < __rhs.day();
        }

        inline constexpr bool operator>(const year_month_day& __lhs, const year_month_day& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const year_month_day& __lhs, const year_month_day& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const year_month_day& __lhs, const year_month_day& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        inline constexpr year_month_day operator/(const year_month& __lhs, const day& __rhs) noexcept
        {
            return year_month_day {__lhs.year(), __lhs.month(), __rhs};
        }

        inline constexpr year_month_day operator/(const year_month& __lhs, int __rhs) noexcept
        {
            return __lhs / day(__rhs);
        }

        inline constexpr year_month_day operator/(const year& __lhs, const month_day& __rhs) noexcept
        {
            return __lhs / __rhs.month() / __rhs.day();
        }

        inline constexpr year_month_day operator/(int __lhs, const month_day& __rhs) noexcept
        {
            return year(__lhs) / __rhs;
        }

        inline constexpr year_month_day operator/(const month_day& __lhs, const year& __rhs) noexcept
        {
            return __rhs / __lhs;
        }

        inline constexpr year_month_day operator/(const month_day& __lhs, int __rhs) noexcept
        {
            return year(__rhs) / __lhs;
        }

        inline constexpr year_month_day operator+(const year_month_day& __lhs, const months& __rhs) noexcept
        {
            return (__lhs.year() / __lhs.month() + __rhs) / __lhs.day();
        }

        inline constexpr year_month_day operator+(const months& __lhs, const year_month_day& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_day operator-(const year_month_day& __lhs, const months& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        inline constexpr year_month_day operator+(const year_month_day& __lhs, const years& __rhs) noexcept
        {
            return (__lhs.year() + __rhs) / __lhs.month() / __lhs.day();
        }

        inline constexpr year_month_day operator+(const years& __lhs, const year_month_day& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_day operator-(const year_month_day& __lhs, const years& __rhs) noexcept
        {
            return __lhs + -__rhs;
        }

        inline constexpr year_month_day& year_month_day::operator+=(const months& __dm) noexcept
        {
            *this = *this + __dm;
            return *this;
        }
        inline constexpr year_month_day& year_month_day::operator-=(const months& __dm) noexcept
        {
            *this = *this - __dm;
            return *this;
        }
        inline constexpr year_month_day& year_month_day::operator+=(const years& __dy) noexcept
        {
            *this = *this + __dy;
            return *this;
        }
        inline constexpr year_month_day& year_month_day::operator-=(const years& __dy) noexcept
        {
            *this = *this - __dy;
            return *this;
        }

        class year_month_day_last
        {
        private:
            chrono::year __y;
            chrono::month_day_last __mdl;

        public:
            constexpr year_month_day_last(const year& __yval, const month_day_last& __mdlval) noexcept
                : __y {__yval}
                , __mdl {__mdlval}
            {
            }

            constexpr year_month_day_last& operator+=(const months& __m) noexcept;
            constexpr year_month_day_last& operator-=(const months& __m) noexcept;
            constexpr year_month_day_last& operator+=(const years& __y) noexcept;
            constexpr year_month_day_last& operator-=(const years& __y) noexcept;

            inline constexpr chrono::year year() const noexcept { return __y; }
            inline constexpr chrono::month month() const noexcept { return __mdl.month(); }
            inline constexpr chrono::month_day_last month_day_last() const noexcept { return __mdl; }
            constexpr chrono::day day() const noexcept;
            inline constexpr operator sys_days() const noexcept { return sys_days {year() / month() / day()}; }
            inline explicit constexpr operator local_days() const noexcept
            {
                return local_days {year() / month() / day()};
            }
            inline constexpr bool ok() const noexcept { return __y.ok() && __mdl.ok(); }
        };

        inline constexpr chrono::day year_month_day_last::day() const noexcept
        {
            constexpr chrono::day __d[] = {chrono::day(31), chrono::day(28), chrono::day(31), chrono::day(30),
                                           chrono::day(31), chrono::day(30), chrono::day(31), chrono::day(31),
                                           chrono::day(30), chrono::day(31), chrono::day(30), chrono::day(31)};
            return (month() != February || !__y.is_leap()) && month().ok() ? __d[static_cast<unsigned>(month()) - 1]
                                                                           : chrono::day {29};
        }

        inline constexpr bool operator==(const year_month_day_last& __lhs, const year_month_day_last& __rhs) noexcept
        {
            return __lhs.year() == __rhs.year() && __lhs.month_day_last() == __rhs.month_day_last();
        }

        inline constexpr bool operator!=(const year_month_day_last& __lhs, const year_month_day_last& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr bool operator<(const year_month_day_last& __lhs, const year_month_day_last& __rhs) noexcept
        {
            if(__lhs.year() < __rhs.year())
                return true;
            if(__lhs.year() > __rhs.year())
                return false;
            return __lhs.month_day_last() < __rhs.month_day_last();
        }

        inline constexpr bool operator>(const year_month_day_last& __lhs, const year_month_day_last& __rhs) noexcept
        {
            return __rhs < __lhs;
        }

        inline constexpr bool operator<=(const year_month_day_last& __lhs, const year_month_day_last& __rhs) noexcept
        {
            return !(__rhs < __lhs);
        }

        inline constexpr bool operator>=(const year_month_day_last& __lhs, const year_month_day_last& __rhs) noexcept
        {
            return !(__lhs < __rhs);
        }

        inline constexpr year_month_day_last operator/(const year_month& __lhs, last_spec) noexcept
        {
            return year_month_day_last {__lhs.year(), month_day_last {__lhs.month()}};
        }

        inline constexpr year_month_day_last operator/(const year& __lhs, const month_day_last& __rhs) noexcept
        {
            return year_month_day_last {__lhs, __rhs};
        }

        inline constexpr year_month_day_last operator/(int __lhs, const month_day_last& __rhs) noexcept
        {
            return year_month_day_last {year {__lhs}, __rhs};
        }

        inline constexpr year_month_day_last operator/(const month_day_last& __lhs, const year& __rhs) noexcept
        {
            return __rhs / __lhs;
        }

        inline constexpr year_month_day_last operator/(const month_day_last& __lhs, int __rhs) noexcept
        {
            return year {__rhs} / __lhs;
        }

        inline constexpr year_month_day_last operator+(const year_month_day_last& __lhs, const months& __rhs) noexcept
        {
            return (__lhs.year() / __lhs.month() + __rhs) / last;
        }

        inline constexpr year_month_day_last operator+(const months& __lhs, const year_month_day_last& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_day_last operator-(const year_month_day_last& __lhs, const months& __rhs) noexcept
        {
            return __lhs + (-__rhs);
        }

        inline constexpr year_month_day_last operator+(const year_month_day_last& __lhs, const years& __rhs) noexcept
        {
            return year_month_day_last {__lhs.year() + __rhs, __lhs.month_day_last()};
        }

        inline constexpr year_month_day_last operator+(const years& __lhs, const year_month_day_last& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_day_last operator-(const year_month_day_last& __lhs, const years& __rhs) noexcept
        {
            return __lhs + (-__rhs);
        }

        inline constexpr year_month_day_last& year_month_day_last::operator+=(const months& __dm) noexcept
        {
            *this = *this + __dm;
            return *this;
        }
        inline constexpr year_month_day_last& year_month_day_last::operator-=(const months& __dm) noexcept
        {
            *this = *this - __dm;
            return *this;
        }
        inline constexpr year_month_day_last& year_month_day_last::operator+=(const years& __dy) noexcept
        {
            *this = *this + __dy;
            return *this;
        }
        inline constexpr year_month_day_last& year_month_day_last::operator-=(const years& __dy) noexcept
        {
            *this = *this - __dy;
            return *this;
        }

        inline constexpr year_month_day::year_month_day(const year_month_day_last& __ymdl) noexcept
            : __y {__ymdl.year()}
            , __m {__ymdl.month()}
            , __d {__ymdl.day()}
        {
        }

        inline constexpr bool year_month_day::ok() const noexcept
        {
            if(!__y.ok() || !__m.ok())
                return false;
            return chrono::day {1} <= __d && __d <= (__y / __m / last).day();
        }

        class year_month_weekday
        {
            chrono::year __y;
            chrono::month __m;
            chrono::weekday_indexed __wdi;

        public:
            year_month_weekday() = default;
            constexpr year_month_weekday(const chrono::year& __yval, const chrono::month& __mval,
                                         const chrono::weekday_indexed& __wdival) noexcept
                : __y {__yval}
                , __m {__mval}
                , __wdi {__wdival}
            {
            }
            constexpr year_month_weekday(const sys_days& __sysd) noexcept
                : year_month_weekday(__from_days(__sysd.time_since_epoch()))
            {
            }
            inline explicit constexpr year_month_weekday(const local_days& __locd) noexcept
                : year_month_weekday(__from_days(__locd.time_since_epoch()))
            {
            }
            constexpr year_month_weekday& operator+=(const months& m) noexcept;
            constexpr year_month_weekday& operator-=(const months& m) noexcept;
            constexpr year_month_weekday& operator+=(const years& y) noexcept;
            constexpr year_month_weekday& operator-=(const years& y) noexcept;

            inline constexpr chrono::year year() const noexcept { return __y; }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr chrono::weekday weekday() const noexcept { return __wdi.weekday(); }
            inline constexpr unsigned index() const noexcept { return __wdi.index(); }
            inline constexpr chrono::weekday_indexed weekday_indexed() const noexcept { return __wdi; }

            inline constexpr operator sys_days() const noexcept { return sys_days {__to_days()}; }
            inline explicit constexpr operator local_days() const noexcept { return local_days {__to_days()}; }
            inline constexpr bool ok() const noexcept
            {
                if(!__y.ok() || !__m.ok() || !__wdi.ok())
                    return false;
                if(__wdi.index() <= 4)
                    return true;
                auto __nth_weekday_day = __wdi.weekday() - chrono::weekday {static_cast<sys_days>(__y / __m / 1)}
                                         + days {(__wdi.index() - 1) * 7 + 1};
                return static_cast<unsigned>(__nth_weekday_day.count())
                       <= static_cast<unsigned>((__y / __m / last).day());
            }

            static constexpr year_month_weekday __from_days(days __d) noexcept;
            constexpr days __to_days() const noexcept;
        };

        inline constexpr year_month_weekday year_month_weekday::__from_days(days __d) noexcept
        {
            const sys_days __sysd {__d};
            const chrono::weekday __wd = chrono::weekday(__sysd);
            const year_month_day __ymd = year_month_day(__sysd);
            return year_month_weekday {__ymd.year(), __ymd.month(),
                                       __wd[(static_cast<unsigned>(__ymd.day()) - 1) / 7 + 1]};
        }

        inline constexpr days year_month_weekday::__to_days() const noexcept
        {
            const sys_days __sysd = sys_days(__y / __m / 1);
            return (__sysd + (__wdi.weekday() - chrono::weekday(__sysd) + days {(__wdi.index() - 1) * 7}))
                .time_since_epoch();
        }

        inline constexpr bool operator==(const year_month_weekday& __lhs, const year_month_weekday& __rhs) noexcept
        {
            return __lhs.year() == __rhs.year() && __lhs.month() == __rhs.month()
                   && __lhs.weekday_indexed() == __rhs.weekday_indexed();
        }

        inline constexpr bool operator!=(const year_month_weekday& __lhs, const year_month_weekday& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr year_month_weekday operator/(const year_month& __lhs, const weekday_indexed& __rhs) noexcept
        {
            return year_month_weekday {__lhs.year(), __lhs.month(), __rhs};
        }

        inline constexpr year_month_weekday operator/(const year& __lhs, const month_weekday& __rhs) noexcept
        {
            return year_month_weekday {__lhs, __rhs.month(), __rhs.weekday_indexed()};
        }

        inline constexpr year_month_weekday operator/(int __lhs, const month_weekday& __rhs) noexcept
        {
            return year(__lhs) / __rhs;
        }

        inline constexpr year_month_weekday operator/(const month_weekday& __lhs, const year& __rhs) noexcept
        {
            return __rhs / __lhs;
        }

        inline constexpr year_month_weekday operator/(const month_weekday& __lhs, int __rhs) noexcept
        {
            return year(__rhs) / __lhs;
        }

        inline constexpr year_month_weekday operator+(const year_month_weekday& __lhs, const months& __rhs) noexcept
        {
            return (__lhs.year() / __lhs.month() + __rhs) / __lhs.weekday_indexed();
        }

        inline constexpr year_month_weekday operator+(const months& __lhs, const year_month_weekday& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_weekday operator-(const year_month_weekday& __lhs, const months& __rhs) noexcept
        {
            return __lhs + (-__rhs);
        }

        inline constexpr year_month_weekday operator+(const year_month_weekday& __lhs, const years& __rhs) noexcept
        {
            return year_month_weekday {__lhs.year() + __rhs, __lhs.month(), __lhs.weekday_indexed()};
        }

        inline constexpr year_month_weekday operator+(const years& __lhs, const year_month_weekday& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_weekday operator-(const year_month_weekday& __lhs, const years& __rhs) noexcept
        {
            return __lhs + (-__rhs);
        }

        inline constexpr year_month_weekday& year_month_weekday::operator+=(const months& __dm) noexcept
        {
            *this = *this + __dm;
            return *this;
        }
        inline constexpr year_month_weekday& year_month_weekday::operator-=(const months& __dm) noexcept
        {
            *this = *this - __dm;
            return *this;
        }
        inline constexpr year_month_weekday& year_month_weekday::operator+=(const years& __dy) noexcept
        {
            *this = *this + __dy;
            return *this;
        }
        inline constexpr year_month_weekday& year_month_weekday::operator-=(const years& __dy) noexcept
        {
            *this = *this - __dy;
            return *this;
        }

        class year_month_weekday_last
        {
        private:
            chrono::year __y;
            chrono::month __m;
            chrono::weekday_last __wdl;

        public:
            constexpr year_month_weekday_last(const chrono::year& __yval, const chrono::month& __mval,
                                              const chrono::weekday_last& __wdlval) noexcept
                : __y {__yval}
                , __m {__mval}
                , __wdl {__wdlval}
            {
            }
            constexpr year_month_weekday_last& operator+=(const months& __dm) noexcept;
            constexpr year_month_weekday_last& operator-=(const months& __dm) noexcept;
            constexpr year_month_weekday_last& operator+=(const years& __dy) noexcept;
            constexpr year_month_weekday_last& operator-=(const years& __dy) noexcept;

            inline constexpr chrono::year year() const noexcept { return __y; }
            inline constexpr chrono::month month() const noexcept { return __m; }
            inline constexpr chrono::weekday weekday() const noexcept { return __wdl.weekday(); }
            inline constexpr chrono::weekday_last weekday_last() const noexcept { return __wdl; }
            inline constexpr operator sys_days() const noexcept { return sys_days {__to_days()}; }
            inline explicit constexpr operator local_days() const noexcept { return local_days {__to_days()}; }
            inline constexpr bool ok() const noexcept { return __y.ok() && __m.ok() && __wdl.ok(); }

            constexpr days __to_days() const noexcept;
        };

        inline constexpr days year_month_weekday_last::__to_days() const noexcept
        {
            const sys_days __last = sys_days {__y / __m / last};
            return (__last - (chrono::weekday {__last} - __wdl.weekday())).time_since_epoch();
        }

        inline constexpr bool operator==(const year_month_weekday_last& __lhs,
                                         const year_month_weekday_last& __rhs) noexcept
        {
            return __lhs.year() == __rhs.year() && __lhs.month() == __rhs.month()
                   && __lhs.weekday_last() == __rhs.weekday_last();
        }

        inline constexpr bool operator!=(const year_month_weekday_last& __lhs,
                                         const year_month_weekday_last& __rhs) noexcept
        {
            return !(__lhs == __rhs);
        }

        inline constexpr year_month_weekday_last operator/(const year_month& __lhs, const weekday_last& __rhs) noexcept
        {
            return year_month_weekday_last {__lhs.year(), __lhs.month(), __rhs};
        }

        inline constexpr year_month_weekday_last operator/(const year& __lhs, const month_weekday_last& __rhs) noexcept
        {
            return year_month_weekday_last {__lhs, __rhs.month(), __rhs.weekday_last()};
        }

        inline constexpr year_month_weekday_last operator/(int __lhs, const month_weekday_last& __rhs) noexcept
        {
            return year(__lhs) / __rhs;
        }

        inline constexpr year_month_weekday_last operator/(const month_weekday_last& __lhs, const year& __rhs) noexcept
        {
            return __rhs / __lhs;
        }

        inline constexpr year_month_weekday_last operator/(const month_weekday_last& __lhs, int __rhs) noexcept
        {
            return year(__rhs) / __lhs;
        }

        inline constexpr year_month_weekday_last operator+(const year_month_weekday_last& __lhs,
                                                           const months& __rhs) noexcept
        {
            return (__lhs.year() / __lhs.month() + __rhs) / __lhs.weekday_last();
        }

        inline constexpr year_month_weekday_last operator+(const months& __lhs,
                                                           const year_month_weekday_last& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_weekday_last operator-(const year_month_weekday_last& __lhs,
                                                           const months& __rhs) noexcept
        {
            return __lhs + (-__rhs);
        }

        inline constexpr year_month_weekday_last operator+(const year_month_weekday_last& __lhs,
                                                           const years& __rhs) noexcept
        {
            return year_month_weekday_last {__lhs.year() + __rhs, __lhs.month(), __lhs.weekday_last()};
        }

        inline constexpr year_month_weekday_last operator+(const years& __lhs,
                                                           const year_month_weekday_last& __rhs) noexcept
        {
            return __rhs + __lhs;
        }

        inline constexpr year_month_weekday_last operator-(const year_month_weekday_last& __lhs,
                                                           const years& __rhs) noexcept
        {
            return __lhs + (-__rhs);
        }

        inline constexpr year_month_weekday_last& year_month_weekday_last::operator+=(const months& __dm) noexcept
        {
            *this = *this + __dm;
            return *this;
        }
        inline constexpr year_month_weekday_last& year_month_weekday_last::operator-=(const months& __dm) noexcept
        {
            *this = *this - __dm;
            return *this;
        }
        inline constexpr year_month_weekday_last& year_month_weekday_last::operator+=(const years& __dy) noexcept
        {
            *this = *this + __dy;
            return *this;
        }
        inline constexpr year_month_weekday_last& year_month_weekday_last::operator-=(const years& __dy) noexcept
        {
            *this = *this - __dy;
            return *this;
        }

        template<class _Duration> class hh_mm_ss
        {
        private:
            static_assert(__is_duration<_Duration>::value,
                          "template parameter of hh_mm_ss must be a std::chrono::duration");
            using __CommonType = common_type_t<_Duration, chrono::seconds>;

            static constexpr uint64_t __pow10(unsigned __exp)
            {
                uint64_t __ret = 1;
                for(unsigned __i = 0; __i < __exp; ++__i)
                    __ret *= 10U;
                return __ret;
            }

            static constexpr unsigned __width(uint64_t __n, uint64_t __d = 10, unsigned __w = 0)
            {
                if(__n >= 2 && __d != 0 && __w < 19)
                    return 1 + __width(__n, __d % __n * 10, __w + 1);
                return 0;
            }

        public:
            static unsigned constexpr fractional_width
                = __width(__CommonType::period::den) < 19 ? __width(__CommonType::period::den) : 6u;
            using precision = duration<typename __CommonType::rep, ratio<1, __pow10(fractional_width)>>;

            constexpr hh_mm_ss() noexcept
                : hh_mm_ss {_Duration::zero()}
            {
            }

            constexpr explicit hh_mm_ss(_Duration __d) noexcept
                : __is_neg(__d < _Duration(0))
                , __h(duration_cast<chrono::hours>(abs(__d)))
                , __m(duration_cast<chrono::minutes>(abs(__d) - hours()))
                , __s(duration_cast<chrono::seconds>(abs(__d) - hours() - minutes()))
                , __f(duration_cast<precision>(abs(__d) - hours() - minutes() - seconds()))
            {
            }

            constexpr bool is_negative() const noexcept { return __is_neg; }
            constexpr chrono::hours hours() const noexcept { return __h; }
            constexpr chrono::minutes minutes() const noexcept { return __m; }
            constexpr chrono::seconds seconds() const noexcept { return __s; }
            constexpr precision subseconds() const noexcept { return __f; }

            constexpr precision to_duration() const noexcept
            {
                auto __dur = __h + __m + __s + __f;
                return __is_neg ? -__dur : __dur;
            }

            constexpr explicit operator precision() const noexcept { return to_duration(); }

        private:
            bool __is_neg;
            chrono::hours __h;
            chrono::minutes __m;
            chrono::seconds __s;
            precision __f;
        };

        constexpr bool is_am(const hours& __h) noexcept
        {
            return __h >= hours(0) && __h < hours(12);
        }
        constexpr bool is_pm(const hours& __h) noexcept
        {
            return __h >= hours(12) && __h < hours(24);
        }

        constexpr hours make12(const hours& __h) noexcept
        {
            if(__h == hours(0))
                return hours(12);
            else if(__h <= hours(12))
                return __h;
            else
                return __h - hours(12);
        }

        constexpr hours make24(const hours& __h, bool __is_pm) noexcept
        {
            if(__is_pm)
                return __h == hours(12) ? __h : __h + hours(12);
            else
                return __h == hours(12) ? hours(0) : __h;
        }

    } // chrono

    // Suffixes for duration literals [time.duration.literals]
    inline namespace literals
    {
        inline namespace chrono_literals
        {

            constexpr chrono::hours operator""h(unsigned long long __h)
            {
                return chrono::hours(static_cast<chrono::hours::rep>(__h));
            }

            constexpr chrono::duration<long double, ratio<3600, 1>> operator""h(long double __h)
            {
                return chrono::duration<long double, ratio<3600, 1>>(__h);
            }

            constexpr chrono::minutes operator""min(unsigned long long __m)
            {
                return chrono::minutes(static_cast<chrono::minutes::rep>(__m));
            }

            constexpr chrono::duration<long double, ratio<60, 1>> operator""min(long double __m)
            {
                return chrono::duration<long double, ratio<60, 1>>(__m);
            }

            constexpr chrono::seconds operator""s(unsigned long long __s)
            {
                return chrono::seconds(static_cast<chrono::seconds::rep>(__s));
            }

            constexpr chrono::duration<long double> operator""s(long double __s)
            {
                return chrono::duration<long double>(__s);
            }

            constexpr chrono::milliseconds operator""ms(unsigned long long __ms)
            {
                return chrono::milliseconds(static_cast<chrono::milliseconds::rep>(__ms));
            }

            constexpr chrono::duration<long double, milli> operator""ms(long double __ms)
            {
                return chrono::duration<long double, milli>(__ms);
            }

            constexpr chrono::microseconds operator""us(unsigned long long __us)
            {
                return chrono::microseconds(static_cast<chrono::microseconds::rep>(__us));
            }

            constexpr chrono::duration<long double, micro> operator""us(long double __us)
            {
                return chrono::duration<long double, micro>(__us);
            }

            constexpr chrono::nanoseconds operator""ns(unsigned long long __ns)
            {
                return chrono::nanoseconds(static_cast<chrono::nanoseconds::rep>(__ns));
            }

            constexpr chrono::duration<long double, nano> operator""ns(long double __ns)
            {
                return chrono::duration<long double, nano>(__ns);
            }

            constexpr chrono::day operator""d(unsigned long long __d) noexcept
            {
                return chrono::day(static_cast<unsigned>(__d));
            }

            constexpr chrono::year operator""y(unsigned long long __y) noexcept
            {
                return chrono::year(static_cast<int>(__y));
            }
        }
    }

    namespace chrono
    { // hoist the literals into namespace std::chrono
        using namespace literals::chrono_literals;
    }
}
// _LIBCPP_POP_MACROS

// #endif // !defined(_LIBCPP_SGX_CONFIG)

// #endif // _LIBCPP_CHRONO
