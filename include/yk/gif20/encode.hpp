#ifndef YK_GIF20_ENCODE_HPP
#define YK_GIF20_ENCODE_HPP

#include <yk/gif20/detail/format.hpp>

#include <yk/gif20/byte.hpp>
#include <yk/gif20/image.hpp>

#include <yk/lzw20/encode.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>

namespace yk::gif20 {

namespace detail {

constexpr void check_palette(std::vector<color> const& palette)
{
  if (palette.size() > max_palette_size) {
    throw std::invalid_argument("yk::gif20::encode: a palette has more than 256 colors");
  }
}

constexpr void check_frame(image const& img, frame const& f)
{
  if (f.width == 0 || f.height == 0) {
    throw std::invalid_argument("yk::gif20::encode: a frame has zero width or height");
  }
  if (std::uint32_t{f.left} + f.width > img.width || std::uint32_t{f.top} + f.height > img.height) {
    throw std::invalid_argument("yk::gif20::encode: a frame is outside the logical screen");
  }
  check_palette(f.local_palette);
  std::vector<color> const& palette = f.local_palette.empty() ? img.global_palette : f.local_palette;
  if (palette.empty()) {
    throw std::invalid_argument("yk::gif20::encode: a frame has no palette");
  }
  if (f.pixels.size() != std::size_t{f.width} * f.height) {
    throw std::invalid_argument("yk::gif20::encode: the pixel count is not the frame size");
  }
  if (std::ranges::any_of(f.pixels, [&](std::uint8_t index) { return index >= palette.size(); })) {
    throw std::invalid_argument("yk::gif20::encode: a pixel is past the end of its palette");
  }
}

constexpr void check_image(image const& img)
{
  check_palette(img.global_palette);
  if (!img.global_palette.empty() && img.background_index >= img.global_palette.size()) {
    throw std::invalid_argument("yk::gif20::encode: the background color is past the end of the global palette");
  }
  for (frame const& f : img.frames) {
    check_frame(img, f);
  }
}

template<byte_like Byte>
class writer {
public:
  constexpr void byte(unsigned char b) { out_.push_back(static_cast<Byte>(b)); }

  // little-endian
  constexpr void u16(std::uint16_t value)
  {
    byte(static_cast<unsigned char>(value & 0xFF));
    byte(static_cast<unsigned char>(value >> 8));
  }

  // padded with black up to the color table size of 2^bits entries
  constexpr void color_table(std::vector<color> const& palette, unsigned bits)
  {
    for (color const& c : palette) {
      byte(c.r);
      byte(c.g);
      byte(c.b);
    }
    for (std::size_t i = palette.size(); i < (std::size_t{1} << bits); ++i) {
      byte(0);
      byte(0);
      byte(0);
    }
  }

  constexpr void sub_blocks(std::vector<unsigned char> const& data)
  {
    for (std::size_t i = 0; i < data.size(); i += max_sub_block_size) {
      std::size_t const size = std::min(max_sub_block_size, data.size() - i);
      byte(static_cast<unsigned char>(size));
      for (std::size_t j = i; j < i + size; ++j) {
        byte(data[j]);
      }
    }
    byte(0);
  }

  [[nodiscard]] constexpr std::vector<Byte> finish() && { return std::move(out_); }

private:
  std::vector<Byte> out_;
};

[[nodiscard]] constexpr std::vector<unsigned char> encode_lzw(unsigned code_size, std::vector<std::uint8_t> const& pixels)
{
  return with_lzw_code_size(code_size, [&](auto size) { return lzw20::encode<lzw_options<decltype(size)::value>>(pixels); });
}

}  // namespace detail

// Throws std::invalid_argument on an image that GIF cannot hold: see image and
// frame for the requirements. During constant evaluation the throw makes the
// expression non-constant, so such an image becomes a compile error.
template<byte_like Byte = unsigned char>
[[nodiscard]] constexpr std::vector<Byte> encode(image const& img)
{
  detail::check_image(img);

  detail::writer<Byte> out;
  constexpr unsigned char header[] = {'G', 'I', 'F', '8', '7', 'a'};
  for (unsigned char const c : header) {
    out.byte(c);
  }

  out.u16(img.width);
  out.u16(img.height);
  if (img.global_palette.empty()) {
    out.byte(0);
  } else {
    // the color resolution is not kept; the table size stands in for it
    unsigned const bits = detail::color_table_bits(img.global_palette.size());
    out.byte(static_cast<unsigned char>(detail::color_table_flag | ((bits - 1) << detail::color_resolution_shift) | (bits - 1)));
  }
  out.byte(img.background_index);
  out.byte(img.pixel_aspect);
  if (!img.global_palette.empty()) {
    out.color_table(img.global_palette, detail::color_table_bits(img.global_palette.size()));
  }

  for (frame const& f : img.frames) {
    out.byte(detail::image_separator);
    out.u16(f.left);
    out.u16(f.top);
    out.u16(f.width);
    out.u16(f.height);
    std::vector<color> const& palette = f.local_palette.empty() ? img.global_palette : f.local_palette;
    unsigned const bits = detail::color_table_bits(palette.size());
    if (f.local_palette.empty()) {
      out.byte(0);
    } else {
      out.byte(static_cast<unsigned char>(detail::color_table_flag | (bits - 1)));
      out.color_table(f.local_palette, bits);
    }
    unsigned const code_size = std::max(detail::min_lzw_code_size, bits);
    out.byte(static_cast<unsigned char>(code_size));
    out.sub_blocks(detail::encode_lzw(code_size, f.pixels));
  }

  out.byte(detail::trailer);
  return std::move(out).finish();
}

}  // namespace yk::gif20

#endif  // YK_GIF20_ENCODE_HPP
