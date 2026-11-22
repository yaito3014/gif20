#include "fixtures.hpp"

#include <yk/gif20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace {

namespace gif = yk::gif20;
namespace fx = yk::gif20::test;

static_assert(gif::decode(fx::two_pixels) == fx::two_pixels_image());
static_assert(std::ranges::equal(gif::encode(fx::two_pixels_image()), fx::two_pixels));
static_assert(gif::decode(fx::two_pixels_controlled) == fx::two_pixels_controlled_image());
static_assert(std::ranges::equal(gif::encode(fx::two_pixels_controlled_image()), fx::two_pixels_controlled));

static_assert([] {
  gif::image const img = gif::decode(fx::pillow_still);
  return img.width == 8 && img.height == 8 && img.global_palette == fx::still_palette() && img.frames.size() == 1 && img.frames[0].pixels == fx::still_pixels();
}());

static_assert([] {
  gif::image const img = gif::decode(fx::pillow_animation);
  return img.frames.size() == 3 && img.frames[2].pixels == fx::animation_pixels(2);
}());

static_assert([] {
  gif::image const img = gif::decode(fx::pillow_still);
  return gif::decode(gif::encode(img)) == img;
}());

}  // namespace

TEST_CASE("the compile-time tests compiled", "[compile_time]") { SUCCEED(); }
