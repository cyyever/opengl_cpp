#pragma once

#include "buffer.hpp"

namespace opengl {

template <typename data_type> class array_buffer final : public buffer {
  static_assert(std::is_same_v<GLfloat, data_type>, "unsupported data type");

public:
  array_buffer() : buffer(GL_ARRAY_BUFFER) {}

  array_buffer(const array_buffer &) = delete;
  array_buffer &operator=(const array_buffer &) = delete;

  array_buffer(array_buffer &&) noexcept = default;
  array_buffer &operator=(array_buffer &&) noexcept = default;

  ~array_buffer() override = default;

  template <size_t N> bool write(const data_type (&data)[N]) noexcept {
    static_assert(N != 0, "can't write empty array");
    return write_span(gsl::span<const data_type>(data));
  }

  template <typename T> bool write(const std::vector<T> &data) noexcept {
    return write_span(gsl::span<const T>(data.data(), data.size()));
  }

  bool vertex_attribute_pointer_simple_offset(GLuint index, GLint size,
                                              GLsizei stride,
                                              size_t offset) noexcept {
    return vertex_attribute_pointer(index, size, stride * sizeof(data_type),
                                    offset * sizeof(data_type));
  }

  bool vertex_attribute_pointer(GLuint index, GLint size, GLsizei stride,
                                size_t offset) noexcept {
    if (!bind()) {
      return false;
    }

    GLenum type = GL_FLOAT;
    glVertexAttribPointer(index, size, type, GL_FALSE, stride,
                          reinterpret_cast<void *>(offset));
    if (check_error()) {
      std::cerr << "glVertexAttribPointer failed" << std::endl;
      return false;
    }

    glEnableVertexAttribArray(index);
    if (check_error()) {
      std::cerr << "glEnableVertexAttribArray failed" << std::endl;
      return false;
    }
    return true;
  }
};

} // namespace opengl
