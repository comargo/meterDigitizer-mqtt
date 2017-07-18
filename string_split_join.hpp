#ifndef STRING_SPLIT_JOIN_HPP
#define STRING_SPLIT_JOIN_HPP

#include <vector>
#include <string>


/**************************
 * split/wsplit/basic_split<T>:
 *  Splits input string (argument #1) by separator (argument #2) and returns vector of strings
 * NOTE:
 *  Does not detect if separator is inside a qoutes or escaped!
 *************************/
template <typename T>
inline std::vector<std::basic_string<T> > basic_split(const std::basic_string<T> &str, const std::basic_string<T> &separator)
{
    std::vector<std::basic_string<T> > sub;
    size_t start = 0;
    size_t end = std::basic_string<T>::npos;
    do
    {
        end = str.find(separator, start);
        if(start < end && start < str.size()) {
            sub.push_back(str.substr(start, end == std::basic_string<T>::npos ? end : end - start));
        }
        start = end + separator.size();
    } while(end != std::basic_string<T>::npos);
    return sub;
}
inline std::vector<std::string> split(const std::string &str, const std::string &separator) { return basic_split<char>(str, separator); }
inline std::vector<std::wstring> wsplit(const std::wstring &str, const std::wstring &separator) { return basic_split<wchar_t>(str, separator); }


/**************************
 * join/wjoin/basic_join<T>:
 *  Joins array of strings (argument #1) by separator (argument #2) and returns strings
 *************************/
template <typename T>
inline std::basic_string<T> basic_join(const std::vector<std::basic_string<T> > &array, const std::basic_string<T> &separator )
{
    typename std::basic_string<T>::size_type newSize = 0;
    for(typename std::vector<std::basic_string<T> >::const_iterator i=array.cbegin(); i!=array.cend(); ++i) {
        newSize += i->size();
    }
    newSize += array.size()*separator.size();
    std::basic_string<T> out;
    out.reserve(newSize);
    for(typename std::vector<std::basic_string<T> >::const_iterator i=array.cbegin();
        i!=array.cend(); ++i) {
        out += *i;
        if(i+1 != array.cend())
            out += separator;
    }
    return out;
}

inline std::string join(const std::vector<std::string> &array, const std::string &separator) { return basic_join<char>(array, separator); }
inline std::wstring wjoin(const std::vector<std::wstring> &array, const std::wstring &separator) { return basic_join<wchar_t>(array, separator); }

#endif//STRING_SPLIT_JOIN_HPP
