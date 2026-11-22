#ifndef YK_GIF20_DECODE_HPP
#define YK_GIF20_DECODE_HPP

#include <yk/gif20/detail/format.hpp>
#include <yk/gif20/detail/reader.hpp>

#include <yk/gif20/byte.hpp>
#include <yk/gif20/error.hpp>
#include <yk/gif20/image.hpp>

#include <yk/lzw20/decode.hpp>

#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdint>

namespace yk::gif20 {

namespace detail {

template<class Reader>
[[nodiscard]] constexpr std::vector<color> read_palette(Reader& in, unsigned char packed)
{
  std::vector<color> palette(std::size_t{2} << (packed & color_table_size_mask));
  for (color& c : palette) {
    c.r = in.byte();
    c.g = in.byte();
    c.b = in.byte();
  }
  return palette;
}

[[nodiscard]] constexpr std::vector<std::uint8_t> decode_lzw(unsigned code_size, std::vector<unsigned char> const& data)
{
  try {
    return with_lzw_code_size(code_size, [&](auto size) { return lzw20::decode<lzw_options<decltype(size)::value>>(data); });
  } catch (lzw20::decode_error const& e) {
    throw decode_error(std::string("yk::gif20::decode: malformed image data: ") + e.what());
  }
}

// after the label
template<class Reader>
[[nodiscard]] constexpr graphic_control read_graphic_control(Reader& in)
{
  if (in.byte() != graphic_control_size) {
    throw decode_error("yk::gif20::decode: a graphic control extension is not 4 bytes");
  }
  unsigned char const packed = in.byte();
  unsigned const method = (packed >> disposal_shift) & disposal_mask;
  if (method > max_disposal) {
    throw decode_error("yk::gif20::decode: a reserved disposal method");
  }
  graphic_control control{.dispose = static_cast<disposal>(method), .user_input = (packed & user_input_flag) != 0, .delay = in.u16()};
  unsigned char const transparent_index = in.byte();
  if (packed & transparent_color_flag) {
    control.transparent_index = transparent_index;
  }
  if (in.byte() != 0) {
    throw decode_error("yk::gif20::decode: a graphic control extension is not 4 bytes");
  }
  return control;
}

template<class Reader>
[[nodiscard]] constexpr frame read_frame(Reader& in, image const& img)
{
  frame f;
  f.left = in.u16();
  f.top = in.u16();
  f.width = in.u16();
  f.height = in.u16();
  if (f.width == 0 || f.height == 0) {
    throw decode_error("yk::gif20::decode: a frame has zero width or height");
  }
  if (std::uint32_t{f.left} + f.width > img.width || std::uint32_t{f.top} + f.height > img.height) {
    throw decode_error("yk::gif20::decode: a frame is outside the logical screen");
  }
  unsigned char const packed = in.byte();
  if (packed & interlace_flag) {
    throw decode_error("yk::gif20::decode: interlaced frames are not supported yet");
  }
  if (packed & color_table_flag) {
    f.local_palette = read_palette(in, packed);
  }
  std::vector<color> const& palette = f.local_palette.empty() ? img.global_palette : f.local_palette;
  if (palette.empty()) {
    throw decode_error("yk::gif20::decode: a frame has no color table");
  }

  unsigned const code_size = in.byte();
  if (code_size < min_lzw_code_size || max_lzw_code_size < code_size) {
    throw decode_error("yk::gif20::decode: the LZW minimum code size is not in [2, 8]");
  }
  f.pixels = decode_lzw(code_size, in.sub_blocks());
  if (f.pixels.size() != std::size_t{f.width} * f.height) {
    throw decode_error("yk::gif20::decode: the pixel count is not the frame size");
  }
  if (std::ranges::any_of(f.pixels, [&](std::uint8_t index) { return index >= palette.size(); })) {
    throw decode_error("yk::gif20::decode: a pixel is past the end of the color table");
  }
  return f;
}

}  // namespace detail

// Throws decode_error on input that is not a GIF this library reads. During
// constant evaluation the throw makes the expression non-constant, so malformed
// input becomes a compile error. Extensions other than graphic control are
// skipped, and the input after the trailer is not read.
template<byte_input_range R>
[[nodiscard]] constexpr image decode(R&& input)
{
  detail::reader in(std::ranges::begin(input), std::ranges::end(input));

  constexpr unsigned char signature[] = {'G', 'I', 'F', '8'};
  for (unsigned char const expected : signature) {
    if (in.byte() != expected) {
      throw decode_error("yk::gif20::decode: the input does not start with GIF87a or GIF89a");
    }
  }
  if (unsigned char const version = in.byte(); (version != '7' && version != '9') || in.byte() != 'a') {
    throw decode_error("yk::gif20::decode: the input does not start with GIF87a or GIF89a");
  }

  image img;
  img.width = in.u16();
  img.height = in.u16();
  unsigned char const packed = in.byte();
  img.background_index = in.byte();
  img.pixel_aspect = in.byte();
  if (packed & detail::color_table_flag) {
    img.global_palette = detail::read_palette(in, packed);
    if (img.background_index >= img.global_palette.size()) {
      throw decode_error("yk::gif20::decode: the background color is past the end of the color table");
    }
  }

  // the graphic control extension for the next graphic rendering block
  std::optional<graphic_control> control;
  // an if chain, not a switch whose break MSVC fails to constant-evaluate inside the loop
  for (;;) {
    unsigned char const introducer = in.byte();
    if (introducer == detail::trailer) {
      return img;
    }
    if (introducer == detail::image_separator) {
      img.frames.push_back(detail::read_frame(in, img));
      img.frames.back().control = control;
      control.reset();
    } else if (introducer == detail::extension_introducer) {
      unsigned char const label = in.byte();
      if (label == detail::graphic_control_label) {
        if (control) {
          throw decode_error("yk::gif20::decode: two graphic control extensions before one graphic rendering block");
        }
        control = detail::read_graphic_control(in);
      } else {
        // plain text is a graphic rendering block too, the one the extension was for
        if (label == detail::plain_text_label) {
          control.reset();
        }
        in.skip_sub_blocks();
      }
    } else {
      throw decode_error("yk::gif20::decode: an unknown block");
    }
  }
}

}  // namespace yk::gif20

#endif  // YK_GIF20_DECODE_HPP
