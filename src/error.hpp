#pragma once

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "glad/glad.h"

namespace opengl {

// copy from <experimental/source_location> and modify some code to make clang
// success.

struct source_location {

  // 14.1.2, source_location creation
  static constexpr source_location current(
#if defined(__GNUG__) && !defined(__clang__)
      const char *__file = __builtin_FILE(), int __line = __builtin_LINE()
#else
      const char *__file = "unknown", int __line = 0
#endif
          ) noexcept {
    source_location __loc;
    __loc._M_file = __file;
    __loc._M_line = __line;
    return __loc;
  }

  constexpr source_location() noexcept : _M_file("unknown"), _M_line(0) {}

  // 14.1.3, source_location field access
  constexpr uint_least32_t line() const noexcept { return _M_line; }
  constexpr const char *file_name() const noexcept { return _M_file; }

private:
  const char *_M_file;
  uint_least32_t _M_line;
};

std::optional<GLenum>
check_error(source_location error_location = source_location::current());

inline void throw_exception [[noreturn]] (const std::string &what_arg) {
  std::cerr << what_arg << std::endl;
  throw std::runtime_error(what_arg);
}

} // namespace opengl
