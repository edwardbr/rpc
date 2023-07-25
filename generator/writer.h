#pragma once
#include <fmt/format.h>
#include <fmt/ostream.h>
#undef  INTERFACE

class writer
{
    std::ostream& strm_;
    int count_ = 0;
public:
    writer(std::ostream& strm) : strm_(strm){};
    template <
        typename S, 
        typename... Args>
    void operator()(const S& format_str, Args&&... args)
    {
        int tmp = 0;
        std::for_each(std::begin(format_str), std::end(format_str), [&](char val){
            if(val == '{')
            {
                tmp++;
            }
            else if(val == '}')
            {
                tmp--;
            }
        });
        assert(!(tmp%2));//should always be in pairs
        if(format_str[0] != '#' && tmp >= 0)
            print_tabs();
        count_ += tmp/2;
        if(format_str[0] != '#' && tmp < 0)
            print_tabs();
        fmt::print(strm_, format_str, args...);
        fmt::print(strm_, "\n");
    }
    template <
        typename S, 
        typename... Args>
    void raw(const S& format_str, Args&&... args)
    {
        fmt::print(strm_, format_str, args...);
    }
    void write_buffer(const std::string& str)
    {
        auto buffer = fmt::basic_memory_buffer<char>();
        buffer.append(str);
        fmt::detail::write_buffer<char>(strm_, buffer);
    }
    void print_tabs()
    {
        for(int i = 0;i < count_;i++)
        {
            strm_ << '\t';
        }
    }

    int get_count(){return count_;}
    void set_count(int count){count_ = count;}
};

