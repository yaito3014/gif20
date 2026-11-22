#include "fixtures.hpp"

#include <yk/gif20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
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

// the parts of two_pixels_controlled
bytes const screen(fx::two_pixels_controlled.begin(), fx::two_pixels_controlled.begin() + 19);
bytes const control_block(fx::two_pixels_controlled.begin() + 19, fx::two_pixels_controlled.begin() + 27);
bytes const image_block(fx::two_pixels_controlled.begin() + 27, fx::two_pixels_controlled.end() - 1);
// a plain text extension, whose text gif20 skips
bytes const plain_text_block{0x21, 0x01, 0x0C, 0, 0, 0, 0, 8, 0, 8, 0, 8, 8, 1, 0, 0x01, 'A', 0x00};

bytes concat(std::initializer_list<bytes> parts)
{
  bytes b;
  for (bytes const& part : parts) {
    b.insert(b.end(), part.begin(), part.end());
  }
  b.push_back(0x3B);
  return b;
}

// the leading colors of palette, which Pillow pads to a power of two
bool starts_with(std::vector<gif::color> const& palette, std::vector<gif::color> const& colors)
{
  return palette.size() >= colors.size() && std::ranges::equal(colors, palette | std::views::take(colors.size()));
}

}  // namespace

TEST_CASE("decode a GIF laid out by hand", "[decode]") { CHECK(gif::decode(fx::two_pixels) == fx::two_pixels_image()); }

TEST_CASE("decode a graphic control extension laid out by hand", "[decode]")
{
  CHECK(gif::decode(fx::two_pixels_controlled) == fx::two_pixels_controlled_image());
}

TEST_CASE("decode each field of a graphic control extension", "[decode]")
{
  auto const control_of = [](unsigned char packed) {
    bytes b = to_bytes(fx::two_pixels_controlled);
    b[22] = packed;
    return gif::decode(b).frames.at(0).control;
  };
  CHECK(control_of(0x00) == gif::graphic_control{.delay = 0x1234});
  CHECK(control_of(0x04) == gif::graphic_control{.dispose = gif::disposal::keep, .delay = 0x1234});
  CHECK(control_of(0x08) == gif::graphic_control{.dispose = gif::disposal::background, .delay = 0x1234});
  CHECK(control_of(0x0C) == gif::graphic_control{.dispose = gif::disposal::previous, .delay = 0x1234});
  CHECK(control_of(0x02) == gif::graphic_control{.user_input = true, .delay = 0x1234});
  CHECK(control_of(0x01) == gif::graphic_control{.delay = 0x1234, .transparent_index = 1});
  // the three reserved bits are ignored
  CHECK(control_of(0xE0) == gif::graphic_control{.delay = 0x1234});
}

TEST_CASE("decode gives a graphic control extension to the frame after it", "[decode]")
{
  SECTION("not to the frames after that")
  {
    gif::image const img = gif::decode(concat({screen, control_block, image_block, image_block}));
    REQUIRE(img.frames.size() == 2);
    CHECK(img.frames[0].control == fx::two_pixels_controlled_image().frames[0].control);
    CHECK(img.frames[1].control == std::nullopt);
  }

  SECTION("not to a frame after the plain text it is for")
  {
    gif::image const img = gif::decode(concat({screen, control_block, plain_text_block, image_block}));
    REQUIRE(img.frames.size() == 1);
    CHECK(img.frames[0].control == std::nullopt);
  }

  SECTION("to none when the trailer follows")
  {
    gif::image const img = gif::decode(concat({screen, image_block, control_block}));
    REQUIRE(img.frames.size() == 1);
    CHECK(img.frames[0].control == std::nullopt);
  }
}

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
  CHECK(f.control == std::nullopt);
}

TEST_CASE("decode an animation written by Pillow", "[decode]")
{
  // its NETSCAPE2.0 extension is skipped
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
    // duration=100 and disposal=1
    CHECK(f.control == gif::graphic_control{.dispose = gif::disposal::keep, .delay = 10});
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
    for (bytes const& whole : {valid, to_bytes(fx::two_pixels_controlled)}) {
      for (std::size_t size = 0; size < whole.size(); ++size) {
        CAPTURE(whole.size(), size);
        CHECK_THROWS_AS(gif::decode(bytes(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(size))), gif::decode_error);
      }
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

  SECTION("a frame of zero width or height")
  {
    // with the image data of no pixels: CLEAR EOI
    for (std::size_t const dimension : {24u, 26u}) {
      CAPTURE(dimension);
      bytes b = valid;
      b[dimension] = 0x00;
      b[30] = 0x01;
      b[31] = 0x2C;
      b.erase(b.begin() + 32);
      CHECK_THROWS_AS(gif::decode(b), gif::decode_error);
    }
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

  SECTION("a reserved disposal method")
  {
    for (unsigned method = 4; method < 8; ++method) {
      CAPTURE(method);
      bytes b = to_bytes(fx::two_pixels_controlled);
      b[22] = static_cast<unsigned char>(method << 2);
      CHECK_THROWS_AS(gif::decode(b), gif::decode_error);
    }
  }

  SECTION("a graphic control extension of other than 4 bytes")
  {
    bytes longer = control_block;
    longer[2] = 0x05;
    longer.insert(longer.end() - 1, 0x00);
    CHECK_THROWS_AS(gif::decode(concat({screen, longer, image_block})), gif::decode_error);
    bytes shorter = control_block;
    shorter[2] = 0x03;
    shorter.erase(shorter.end() - 2);
    CHECK_THROWS_AS(gif::decode(concat({screen, shorter, image_block})), gif::decode_error);
  }

  SECTION("two graphic control extensions before a frame")
  {
    CHECK_THROWS_AS(gif::decode(concat({screen, control_block, control_block, image_block})), gif::decode_error);
  }
}
