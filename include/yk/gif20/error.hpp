#ifndef YK_GIF20_ERROR_HPP
#define YK_GIF20_ERROR_HPP

#include <stdexcept>

namespace yk::gif20 {

class decode_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

}  // namespace yk::gif20

#endif  // YK_GIF20_ERROR_HPP
