//
// (c) 2018 - 2022 by dbj@dbj.org
// Disclaimer, Terms and Conditions:
// https://dbj.org/dbj_license
// or
// CC BY SA 4.0
//

#include <array>
#include <assert.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <vector>

namespace dbj
{

    template<typename C>
    struct is_char :
        std::integral_constant<bool,
            std::is_same<C, char>::value || std::is_same<C, char8_t>::value || std::is_same<C, char16_t>::value || std::is_same<C, char32_t>::value ||
                std::is_same<C, wchar_t>::value>
    {};

    // inspired by https://nlitsme.github.io/2019/10/c-type_tests/
    struct not_this_one
    {};  // Tag type for detecting which begin/ end are being selected

    // Import begin/ end from std here so they are considered
    // alongside the fallback (...) overloads in this namespace
    using std::begin;
    using std::end;

    not_this_one begin(...);
    not_this_one end(...);

    template<typename T>
    struct is_range final
    {
        constexpr static const bool value =
            !std::is_same<decltype(begin(std::declval<T>())), not_this_one>::value && !std::is_same<decltype(end(std::declval<T>())), not_this_one>::value;
    };

    // make it a bit more palatable
    template<typename T>
    constexpr inline bool is_range_v = dbj::is_range<T>::value;

    namespace inner
    {

        template<typename return_type>
        struct meta_converter final
        {
            template<typename T>
            return_type operator() (T arg) noexcept
            {
                if constexpr (dbj::is_range_v<T>)
                {
                    static_assert(
                        // arg must have this typedef
                        dbj::is_char<typename T::value_type>{}(),
                        "can not transform ranges not made of char like types");
                    return {arg.begin(), arg.end()};
                }
                else
                {
                    using actual_type = std::remove_cv_t<std::remove_pointer_t<T>>;
                    return this->operator() (std::basic_string<actual_type>{arg});
                }
            }
        };  // meta_converter
    }       // namespace inner

    // the convertor types implicit instantiations
    using char_range_to_string    = inner::meta_converter<std::string>;
    using wchar_range_to_string   = inner::meta_converter<std::wstring>;
    using u16char_range_to_string = inner::meta_converter<std::u16string>;
    using u32char_range_to_string = inner::meta_converter<std::u32string>;

}  // namespace dbj
