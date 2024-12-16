#pragma once
#include <fmt/format.h>
#include <fmt/ostream.h>
#undef INTERFACE

class writer
{
    std::ostream& strm_;
    int count_ = 0;

public:
    writer(std::ostream& strm)
        : strm_(strm) {};    
        
    writer(std::ostream& strm, int tab_count)
        : strm_(strm), count_(tab_count) {};
        
    int get_tab_count() const {return count_;}
    void set_tab_count(int count) { count_ = count; }
    
    template<typename S, typename... Args> void operator()(const S& format_str, Args&&... args)
    {
        int tmp = 0;
        std::for_each(std::begin(format_str), std::end(format_str),
                      [&](char val)
                      {
                          if(val == '{')
                          {
                              tmp++;
                          }
                          else if(val == '}')
                          {
                              tmp--;
                          }
                      });
        assert(!(tmp % 2)); // should always be in pairs
        if(format_str[0] != '#' && tmp >= 0)
            print_tabs();
        count_ += tmp / 2;
        if(format_str[0] != '#' && tmp < 0)
            print_tabs();
        fmt::print(strm_, format_str, args...);
        fmt::print(strm_, "\n");
    }
    template<typename S, typename... Args> void raw(const S& format_str, Args&&... args)
    {
        fmt::print(strm_, format_str, args...);
    }
    void write_buffer(const std::string& str) { strm_ << str; }
    void print_tabs()
    {
        for(int i = 0; i < count_; i++)
        {
            strm_ << '\t';
        }
    }
};
