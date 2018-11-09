#pragma once

#include "buffer.hpp"

namespace opengl {

template <typename data_type> class element_array_buffer final : public buffer {
  static_assert(std::is_same_v<GLubyte, data_type> ||
                    std::is_same_v<GLushort, data_type> ||
                    std::is_same_v<GLuint, data_type>,
                "unsupported data type");

public:
  element_array_buffer() : buffer(GL_ELEMENT_ARRAY_BUFFER) {}

  element_array_buffer(const element_array_buffer &) = delete;
  element_array_buffer &operator=(const element_array_buffer &) = delete;

  element_array_buffer(element_array_buffer &&) noexcept = default;
  element_array_buffer &operator=(element_array_buffer &&) noexcept = default;

  ~element_array_buffer() override = default;

  template <size_t N> bool write(const data_type (&data)[N]) noexcept {
    static_assert(N != 0, "can't write empty array");
    return write_span(gsl::span<const data_type>(data));
  }

  template <typename T> bool write(const std::vector<T> &data) noexcept {
    return write_span(gsl::span<const T>(data.data(), data.size()));
  }

  bool use() noexcept { return bind(); }
};

} // namespace opengl
