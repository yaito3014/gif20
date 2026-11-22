#ifndef YK_GIF20_DETAIL_READER_HPP
#define YK_GIF20_DETAIL_READER_HPP

#include <yk/gif20/error.hpp>

#include <iterator>
#include <utility>
#include <vector>

#include <cstdint>

namespace yk::gif20::detail {

template<std::input_iterator I, std::sentinel_for<I> S>
class reader {
public:
  constexpr reader(I first, S last) : it_(std::move(first)), last_(std::move(last)) {}

  [[nodiscard]] constexpr unsigned char byte()
  {
    if (it_ == last_) {
      throw decode_error("yk::gif20::decode: the input ends early");
    }
    unsigned char const b = static_cast<unsigned char>(*it_);
    ++it_;
    return b;
  }

  // little-endian
  [[nodiscard]] constexpr std::uint16_t u16()
  {
    unsigned const low = byte();
    unsigned const high = byte();
    return static_cast<std::uint16_t>(low | (high << 8));
  }

  // the data of the sub-blocks up to the block terminator
  [[nodiscard]] constexpr std::vector<unsigned char> sub_blocks()
  {
    std::vector<unsigned char> data;
    for (unsigned char size = byte(); size != 0; size = byte()) {
      for (; size != 0; --size) {
        data.push_back(byte());
      }
    }
    return data;
  }

  constexpr void skip_sub_blocks()
  {
    for (unsigned char size = byte(); size != 0; size = byte()) {
      for (; size != 0; --size) {
        static_cast<void>(byte());
      }
    }
  }

private:
  I it_;
  S last_;
};

}  // namespace yk::gif20::detail

#endif  // YK_GIF20_DETAIL_READER_HPP
