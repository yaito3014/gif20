#include "fixtures.hpp"

#include <yk/gif20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

namespace gif = yk::gif20;
namespace fx = yk::gif20::test;

using bytes = std::vector<unsigned char>;

std::vector<gif::color> random_palette(std::mt19937& rng, std::size_t size)
{
  std::uniform_int_distribution<unsigned> channel(0, 255);
  std::vector<gif::color> palette(size);
  std::ranges::generate(palette, [&] {
    return gif::color{static_cast<std::uint8_t>(channel(rng)), static_cast<std::uint8_t>(channel(rng)), static_cast<std::uint8_t>(channel(rng))};
  });
  return palette;
}

gif::frame random_frame(std::mt19937& rng, gif::image const& img, std::size_t palette_size, bool local)
{
  std::uniform_int_distribution<std::uint16_t> width(1, img.width);
  std::uniform_int_distribution<std::uint16_t> height(1, img.height);
  gif::frame f{.width = width(rng), .height = height(rng)};
  f.left = std::uniform_int_distribution<std::uint16_t>(0, static_cast<std::uint16_t>(img.width - f.width))(rng);
  f.top = std::uniform_int_distribution<std::uint16_t>(0, static_cast<std::uint16_t>(img.height - f.height))(rng);
  if (local) {
    f.local_palette = random_palette(rng, palette_size);
  }
  // runs of one index, so that the LZW dictionary has long strings to find
  std::uniform_int_distribution<std::size_t> index(0, palette_size - 1);
  std::uniform_int_distribution<std::size_t> run(1, 8);
  while (f.pixels.size() < std::size_t{f.width} * f.height) {
    f.pixels.insert(f.pixels.end(), std::min(run(rng), std::size_t{f.width} * f.height - f.pixels.size()), static_cast<std::uint8_t>(index(rng)));
  }
  return f;
}

}  // namespace

TEST_CASE("encode lays out a GIF as the specification does", "[encode]")
{
  bytes const expected(fx::two_pixels.begin(), fx::two_pixels.end());
  CHECK(gif::encode(fx::two_pixels_image()) == expected);
}

TEST_CASE("encode lays out a graphic control extension as the specification does", "[encode]")
{
  bytes const expected(fx::two_pixels_controlled.begin(), fx::two_pixels_controlled.end());
  CHECK(gif::encode(fx::two_pixels_controlled_image()) == expected);
}

TEST_CASE("encode to any byte type", "[encode]")
{
  std::vector<std::byte> const out = gif::encode<std::byte>(fx::two_pixels_image());
  CHECK(std::ranges::equal(out, fx::two_pixels, {}, [](std::byte b) { return std::to_integer<unsigned char>(b); }));
  CHECK(gif::decode(gif::encode<char>(fx::two_pixels_image())) == fx::two_pixels_image());
}

TEST_CASE("decode reverses encode", "[encode]")
{
  std::mt19937 rng(20261122);
  // palette sizes on each side of a power of two, which decides the LZW minimum code size
  for (std::size_t const palette_size : {1u, 2u, 3u, 4u, 5u, 16u, 17u, 128u, 129u, 255u, 256u}) {
    for (bool const local : {false, true}) {
      CAPTURE(palette_size, local);
      gif::image img{.width = 40, .height = 30, .background_index = static_cast<std::uint8_t>(palette_size - 1), .pixel_aspect = 49};
      if (!local) {
        img.global_palette = random_palette(rng, palette_size);
      } else {
        img.background_index = 0;
      }
      for (int i = 0; i < 3; ++i) {
        img.frames.push_back(random_frame(rng, img, palette_size, local));
      }

      gif::image const decoded = gif::decode(gif::encode(img));
      // the color tables come back padded to a power of two, with black
      std::size_t padded = 2;
      while (padded < palette_size) {
        padded *= 2;
      }
      auto const pad = [&](std::vector<gif::color> palette) {
        palette.resize(palette.empty() ? 0 : padded, gif::color{0, 0, 0});
        return palette;
      };
      gif::image expected = img;
      expected.global_palette = pad(expected.global_palette);
      for (gif::frame& f : expected.frames) {
        f.local_palette = pad(f.local_palette);
      }
      CHECK(decoded == expected);
    }
  }
}

TEST_CASE("decode reverses encode for a large frame", "[encode]")
{
  // enough codes to fill the LZW dictionary several times
  std::mt19937 rng(42);
  gif::image img{.width = 300, .height = 200};
  img.global_palette = random_palette(rng, 256);
  gif::frame f{.width = 300, .height = 200};
  std::uniform_int_distribution<unsigned> index(0, 255);
  f.pixels.resize(std::size_t{300} * 200);
  std::ranges::generate(f.pixels, [&] { return static_cast<std::uint8_t>(index(rng)); });
  img.frames.push_back(f);
  CHECK(gif::decode(gif::encode(img)) == img);
}

TEST_CASE("decode reverses encode for graphic control extensions", "[encode]")
{
  gif::image img = fx::two_pixels_image();
  gif::frame const f = img.frames[0];
  img.frames.clear();
  for (std::optional<gif::graphic_control> const& control : std::initializer_list<std::optional<gif::graphic_control>>{
           std::nullopt,
           gif::graphic_control{},
           gif::graphic_control{.dispose = gif::disposal::keep},
           gif::graphic_control{.dispose = gif::disposal::background},
           gif::graphic_control{.dispose = gif::disposal::previous},
           gif::graphic_control{.user_input = true},
           gif::graphic_control{.delay = 0xFFFF},
           gif::graphic_control{.transparent_index = 0},
           // past the end of the palette, where it matches no pixel
           gif::graphic_control{.transparent_index = 255},
       }) {
    gif::frame g = f;
    g.control = control;
    img.frames.push_back(g);
  }
  CHECK(gif::decode(gif::encode(img)) == img);
}

TEST_CASE("encode rejects an image it cannot write", "[encode]")
{
  gif::image const valid = fx::two_pixels_image();
  auto const check_rejects = [](gif::image const& img) { CHECK_THROWS_AS(gif::encode(img), std::invalid_argument); };

  SECTION("a pixel count other than the frame size")
  {
    gif::image img = valid;
    img.frames[0].pixels.push_back(0);
    check_rejects(img);
  }

  SECTION("a pixel past the end of the color table")
  {
    gif::image img = valid;
    img.frames[0].pixels[1] = 2;
    check_rejects(img);
  }

  SECTION("a frame outside the logical screen")
  {
    gif::image img = valid;
    img.frames[0].left = 1;
    check_rejects(img);
  }

  SECTION("a frame of zero width or height")
  {
    gif::image img = valid;
    img.frames[0] = gif::frame{.width = 0, .height = 1};
    check_rejects(img);
    img.frames[0] = gif::frame{.width = 2, .height = 0};
    check_rejects(img);
  }

  SECTION("no color table")
  {
    gif::image img = valid;
    img.global_palette.clear();
    check_rejects(img);
  }

  SECTION("a color table of more than 256 colors")
  {
    gif::image img = valid;
    img.global_palette.resize(257);
    check_rejects(img);
  }

  SECTION("a background color past the end of the color table")
  {
    gif::image img = valid;
    img.background_index = 2;
    check_rejects(img);
  }

  SECTION("a disposal method that is none of the enumerators")
  {
    gif::image img = valid;
    img.frames[0].control = gif::graphic_control{.dispose = static_cast<gif::disposal>(4)};
    check_rejects(img);
  }
}
