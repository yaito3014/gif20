#ifndef YK_GIF20_TEST_FIXTURES_HPP
#define YK_GIF20_TEST_FIXTURES_HPP

#include <yk/gif20.hpp>

#include <array>
#include <vector>

#include <cstddef>
#include <cstdint>

namespace yk::gif20::test {

#include "pillow_fixtures.inc"

// the images of tools/gen_fixtures.py

[[nodiscard]] constexpr std::vector<color> still_palette() { return {{0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}}; }

[[nodiscard]] constexpr std::vector<std::uint8_t> still_pixels()
{
  std::vector<std::uint8_t> pixels;
  for (std::size_t i = 0; i < 64; ++i) {
    pixels.push_back(static_cast<std::uint8_t>((i * i + i / 3) % 4));
  }
  return pixels;
}

inline constexpr std::size_t animation_width = 16;
inline constexpr std::size_t animation_height = 12;

[[nodiscard]] constexpr std::vector<color> animation_palette(std::size_t k)
{
  std::vector<color> palette;
  for (std::size_t c = 0; c < 3 * (3 + k); c += 3) {
    auto const channel = [&](std::size_t i) { return static_cast<std::uint8_t>(i * (k + 1) * 37 % 256); };
    palette.push_back(color{channel(c), channel(c + 1), channel(c + 2)});
  }
  return palette;
}

[[nodiscard]] constexpr std::vector<std::uint8_t> animation_pixels(std::size_t k)
{
  std::vector<std::uint8_t> pixels;
  for (std::size_t y = 0; y < animation_height; ++y) {
    for (std::size_t x = 0; x < animation_width; ++x) {
      pixels.push_back(static_cast<std::uint8_t>((x * (k + 2) + y * 3 + k) % (3 + k)));
    }
  }
  return pixels;
}

// a 2x1 image of a black and a white pixel, laid out by hand from the GIF89a specification
inline constexpr std::array<unsigned char, 35> two_pixels{
    0x47, 0x49, 0x46, 0x38, 0x37, 0x61,              // "GIF87a"
    0x02, 0x00, 0x01, 0x00,                          // logical screen 2x1
    0x80,                                            // a global color table of 2 entries
    0x00, 0x00,                                      // background color index, pixel aspect ratio
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,              // black, white
    0x2C, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01,  // image descriptor: at 0,0, 2x1,
    0x00, 0x00,                                      //   no local color table, not interlaced
    0x02,                                            // LZW minimum code size
    0x02, 0x44, 0x0A,                                // CLEAR 0 1 EOI, 3 bits each, in one sub-block
    0x00,                                            // block terminator
    0x3B,                                            // trailer
};

[[nodiscard]] constexpr image two_pixels_image()
{
  return image{
      .width = 2,
      .height = 1,
      .global_palette = {{0, 0, 0}, {255, 255, 255}},
      .frames = {frame{.width = 2, .height = 1, .pixels = {0, 1}}},
  };
}

}  // namespace yk::gif20::test

#endif  // YK_GIF20_TEST_FIXTURES_HPP
