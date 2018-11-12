#pragma once

#include "buffer.hpp"

namespace opengl {

class uniform_buffer final : public buffer {

public:
  uniform_buffer(size_t buffer_size) : buffer(GL_UNIFORM_BUFFER) {
    if (!alloc(buffer_size)) {
      throw_exception("alloc failed");
    }
  }

  uniform_buffer(const uniform_buffer &) = delete;
  uniform_buffer &operator=(const uniform_buffer &) = delete;

  uniform_buffer(uniform_buffer &&) noexcept = default;
  uniform_buffer &operator=(uniform_buffer &&) noexcept = default;

  ~uniform_buffer() override = default;

  template <typename T> bool write(const T &data, GLintptr offset) noexcept {
    return write_part(gsl::span<const T>{&data, 1}, offset);
  }

  bool use(GLuint binding_point) noexcept {
    if (!bind()) {
      return false;
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, *buffer_id);
    if (check_error()) {
      std::cerr << "glBindBufferBase failed" << std::endl;
      return false;
    }
    return true;
  }
};

} // namespace opengl
