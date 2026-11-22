#ifndef YK_GIF20_DETAIL_FORMAT_HPP
#define YK_GIF20_DETAIL_FORMAT_HPP

#include <yk/lzw20/options.hpp>

#include <type_traits>
#include <utility>

#include <cstddef>
#include <cstdint>

namespace yk::gif20::detail {

static_assert(std::is_same_v<std::uint8_t, unsigned char>, "palette indices are passed to and from yk::lzw20 as unsigned char");

inline constexpr unsigned char extension_introducer = 0x21;
inline constexpr unsigned char image_separator = 0x2C;
inline constexpr unsigned char trailer = 0x3B;

// the packed fields of the logical screen descriptor and the image descriptor
inline constexpr unsigned char color_table_flag = 0x80;
inline constexpr unsigned char interlace_flag = 0x40;
inline constexpr unsigned char color_table_size_mask = 0x07;


inline constexpr unsigned min_lzw_code_size = 2;
inline constexpr unsigned max_lzw_code_size = 8;

template<unsigned CodeSize>
inline constexpr lzw20::options lzw_options{
    .max_width = 12,
    .order = lzw20::bit_order::lsb_first,
    .alphabet_width = CodeSize,
    .control = lzw20::control_codes::clear_eoi,
    .growth = lzw20::code_growth::last_code,
    .on_full = lzw20::full_table::clear,
};

// Calls f(std::integral_constant<unsigned, code_size>{}), for code_size in
// [min_lzw_code_size, max_lzw_code_size], so that f can name lzw_options<code_size>
template<class F>
constexpr decltype(auto) with_lzw_code_size(unsigned code_size, F&& f)
{
  switch (code_size) {
    case 2:
      return std::forward<F>(f)(std::integral_constant<unsigned, 2>{});
    case 3:
      return std::forward<F>(f)(std::integral_constant<unsigned, 3>{});
    case 4:
      return std::forward<F>(f)(std::integral_constant<unsigned, 4>{});
    case 5:
      return std::forward<F>(f)(std::integral_constant<unsigned, 5>{});
    case 6:
      return std::forward<F>(f)(std::integral_constant<unsigned, 6>{});
    case 7:
      return std::forward<F>(f)(std::integral_constant<unsigned, 7>{});
    default:
      return std::forward<F>(f)(std::integral_constant<unsigned, 8>{});
  }
}

}  // namespace yk::gif20::detail

#endif  // YK_GIF20_DETAIL_FORMAT_HPP
