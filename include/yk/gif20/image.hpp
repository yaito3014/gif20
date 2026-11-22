#ifndef YK_GIF20_IMAGE_HPP
#define YK_GIF20_IMAGE_HPP

#include <vector>

#include <cstdint>

namespace yk::gif20 {

struct color {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;

  friend constexpr bool operator==(color const&, color const&) = default;
};

// An image in the GIF stream, drawn on the logical screen
struct frame {
  std::uint16_t left = 0;
  std::uint16_t top = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  // empty to use the global palette; at most 256 colors
  std::vector<color> local_palette{};
  // width * height palette indices, row by row from the top
  std::vector<std::uint8_t> pixels{};

  friend constexpr bool operator==(frame const&, frame const&) = default;
};

// A whole GIF file. A decoded palette has the size of the color table in the
// file, a power of two; encode pads a palette with black up to one.
struct image {
  // the logical screen
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  // empty if there is none; at most 256 colors
  std::vector<color> global_palette{};
  // an index into global_palette, if there is one
  std::uint8_t background_index = 0;
  // as stored: 0, or the width-to-height ratio as (pixel_aspect + 15) / 64
  std::uint8_t pixel_aspect = 0;
  std::vector<frame> frames{};

  friend constexpr bool operator==(image const&, image const&) = default;
};

}  // namespace yk::gif20

#endif  // YK_GIF20_IMAGE_HPP
