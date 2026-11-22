#include "fixtures.hpp"

#include <yk/gif20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace gif = yk::gif20;
namespace fx = yk::gif20::test;

using bytes = std::vector<unsigned char>;

template<class R>
bytes to_bytes(R const& r)
{
  return bytes(r.begin(), r.end());
}

// the leading colors of palette, which Pillow pads to a power of two
bool starts_with(std::vector<gif::color> const& palette, std::vector<gif::color> const& colors)
{
  return palette.size() >= colors.size() && std::ranges::equal(colors, palette | std::views::take(colors.size()));
}

}  // namespace

TEST_CASE("decode a GIF laid out by hand", "[decode]") { CHECK(gif::decode(fx::two_pixels) == fx::two_pixels_image()); }

TEST_CASE("decode a still image written by Pillow", "[decode]")
{
  gif::image const img = gif::decode(fx::pillow_still);
  CHECK(img.width == 8);
  CHECK(img.height == 8);
  CHECK(img.global_palette == fx::still_palette());
  CHECK(img.background_index == 0);
  CHECK(img.pixel_aspect == 0);
  REQUIRE(img.frames.size() == 1);
  gif::frame const& f = img.frames[0];
  CHECK(f.left == 0);
  CHECK(f.top == 0);
  CHECK(f.width == 8);
  CHECK(f.height == 8);
  CHECK(f.local_palette.empty());
  CHECK(f.pixels == fx::still_pixels());
}

TEST_CASE("decode an animation written by Pillow", "[decode]")
{
  // its extensions are skipped
  gif::image const img = gif::decode(fx::pillow_animation);
  CHECK(img.width == fx::animation_width);
  CHECK(img.height == fx::animation_height);
  CHECK(img.global_palette.size() == 4);
  CHECK(starts_with(img.global_palette, fx::animation_palette(0)));
  REQUIRE(img.frames.size() == 3);
  for (std::size_t k = 0; k < 3; ++k) {
    CAPTURE(k);
    gif::frame const& f = img.frames[k];
    CHECK(f.left == 0);
    CHECK(f.top == 0);
    CHECK(f.width == fx::animation_width);
    CHECK(f.height == fx::animation_height);
    CHECK(f.pixels == fx::animation_pixels(k));
  }
  CHECK(img.frames[0].local_palette.empty());
  CHECK(img.frames[1].local_palette == fx::animation_palette(1));
  CHECK(img.frames[2].local_palette.size() == 8);
  CHECK(starts_with(img.frames[2].local_palette, fx::animation_palette(2)));
}

TEST_CASE("decode accepts GIF89a and ignores the input after the trailer", "[decode]")
{
  bytes input = to_bytes(fx::two_pixels);
  input[4] = '9';
  input.push_back(0xFF);
  CHECK(gif::decode(input) == fx::two_pixels_image());
}

TEST_CASE("decode accepts any byte type", "[decode]")
{
  std::vector<std::byte> input;
  for (unsigned char const b : fx::two_pixels) {
    input.push_back(static_cast<std::byte>(b));
  }
  CHECK(gif::decode(input) == fx::two_pixels_image());
  CHECK(gif::decode(std::vector<char>(fx::two_pixels.begin(), fx::two_pixels.end())) == fx::two_pixels_image());
}

TEST_CASE("decode does not read a single-pass input past the trailer", "[decode]")
{
  std::istringstream stream(std::string(fx::two_pixels.begin(), fx::two_pixels.end()) + "XYZ");
  stream >> std::noskipws;
  CHECK(gif::decode(std::views::istream<unsigned char>(stream)) == fx::two_pixels_image());
  CHECK(stream.peek() == 'X');
}

TEST_CASE("decode rejects malformed input", "[decode]")
{
  bytes const valid = to_bytes(fx::two_pixels);
  auto const with = [&](std::size_t at, unsigned char value) {
    bytes b = valid;
    b[at] = value;
    return b;
  };

  SECTION("signature and version")
  {
    CHECK_THROWS_AS(gif::decode(with(0, 'J')), gif::decode_error);
    CHECK_THROWS_AS(gif::decode(with(4, '8')), gif::decode_error);
  }

  SECTION("truncated")
  {
    for (std::size_t size = 0; size < valid.size(); ++size) {
      CAPTURE(size);
      CHECK_THROWS_AS(gif::decode(bytes(valid.begin(), valid.begin() + static_cast<std::ptrdiff_t>(size))), gif::decode_error);
    }
  }

  SECTION("an unknown block")
  {
    // where the image descriptor was
    CHECK_THROWS_AS(gif::decode(with(19, 0x00)), gif::decode_error);
  }

  SECTION("a frame outside the logical screen")
  {
    // left 1
    CHECK_THROWS_AS(gif::decode(with(20, 0x01)), gif::decode_error);
    // logical screen 1 wide
    CHECK_THROWS_AS(gif::decode(with(6, 0x01)), gif::decode_error);
  }

  SECTION("no color table")
  {
    bytes b = valid;
    b[10] = 0x00;
    b.erase(b.begin() + 13, b.begin() + 19);
    CHECK_THROWS_AS(gif::decode(b), gif::decode_error);
  }

  SECTION("a pixel past the end of the color table")
  {
    // CLEAR 0 2 EOI
    bytes b = valid;
    b[31] = 0x84;
    CHECK_THROWS_AS(gif::decode(b), gif::decode_error);
  }

  SECTION("a background color past the end of the color table") { CHECK_THROWS_AS(gif::decode(with(11, 0x02)), gif::decode_error); }

  SECTION("a pixel count other than the frame size")
  {
    // CLEAR 0 EOI
    bytes b = valid;
    b[31] = 0x44;
    b[32] = 0x01;
    CHECK_THROWS_AS(gif::decode(b), gif::decode_error);
  }

  SECTION("an LZW minimum code size outside [2, 8]")
  {
    CHECK_THROWS_AS(gif::decode(with(29, 0x01)), gif::decode_error);
    CHECK_THROWS_AS(gif::decode(with(29, 0x09)), gif::decode_error);
  }

  SECTION("malformed LZW data")
  {
    // CLEAR 7: not a literal
    bytes b = valid;
    b[31] = 0x3C;
    b[32] = 0x00;
    CHECK_THROWS_AS(gif::decode(b), gif::decode_error);
  }

  SECTION("an interlaced frame, not supported yet") { CHECK_THROWS_AS(gif::decode(with(28, 0x40)), gif::decode_error); }
}
