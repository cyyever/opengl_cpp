#pragma once

#include <cstddef>
#include <functional>
#include <gsl/gsl>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "context.hpp"
#include "error.hpp"

namespace opengl {

template <GLenum target, typename data_type> class buffer final {
  static_assert((GL_ARRAY_BUFFER == target &&
                 std::is_same_v<GLfloat, data_type>) ||
                    (GL_ELEMENT_ARRAY_BUFFER == target &&
                     (std::is_same_v<GLubyte, data_type> ||
                      std::is_same_v<GLushort, data_type> ||
                      std::is_same_v<GLuint, data_type>)),
                "unsupported target/data type");

public:
  buffer() {
    if constexpr (opengl::context::gl_minor_version < 5) {
      glGenBuffers(1, buffer_id.get());
      if (check_error()) {
        throw_exception("glGenBuffers failed");
      }
    } else {
      glCreateBuffers(1, buffer_id.get());
      if (check_error()) {
        throw_exception("glCreateBuffers failed");
      }
    }
  }

  buffer(const buffer &) = delete;
  buffer &operator=(const buffer &) = delete;

  buffer(buffer &&) noexcept = default;
  buffer &operator=(buffer &&) noexcept = default;

  ~buffer() noexcept = default;

  template <size_t N> bool write(const data_type (&data)[N]) noexcept {
    static_assert(N != 0, "can't write empty array");
    return write(gsl::span<const data_type>(data));
  }

  template <typename T> bool write(const std::vector<T> &data) noexcept {
    return write(gsl::span<const T>(data.data(), data.size()));
  }

  template <typename T> bool write(gsl::span<const T> data_view) noexcept {
    static_assert(
        target == GL_ARRAY_BUFFER ||
            (target == GL_ELEMENT_ARRAY_BUFFER && std::is_same_v<T, data_type>),
        "unsupported method");
    if (data_view.empty()) {
      std::cerr << "can't write empty data" << std::endl;
      return false;
    }

    if constexpr (opengl::context::gl_minor_version < 5) {
      if (!bind()) {
        return false;
      }
    }
    if constexpr (opengl::context::gl_minor_version < 5) {
      glBufferData(target, data_view.size_bytes(), data_view.data(),
                   GL_STATIC_DRAW);
    } else {
      glNamedBufferData(*buffer_id, data_view.size_bytes(), data_view.data(),
                        GL_STATIC_DRAW);
    }
    if (check_error()) {
      std::cerr << "glBufferData failed" << std::endl;
      return false;
    }
    return true;
  }

  bool vertex_attribute_pointer_simple_offset(GLuint index, GLint size,
                                              GLsizei stride,
                                              size_t offset) noexcept {
    static_assert(GL_ARRAY_BUFFER == target, "unsupported target");
    return vertex_attribute_pointer(index, size, stride * sizeof(data_type),
                                    offset * sizeof(data_type));
  }

  bool vertex_attribute_pointer(GLuint index, GLint size, GLsizei stride,
                                size_t offset) noexcept {
    static_assert(GL_ARRAY_BUFFER == target, "unsupported target");
    if (!bind()) {
      return false;
    }

    GLenum type = GL_FLOAT;
    if constexpr (std::is_same_v<GLint, data_type>) {
      type = GL_INT;
    }
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

public:
  bool bind() noexcept {
    glBindBuffer(target, *buffer_id);
    if (check_error()) {
      std::cerr << "glBindBuffer failed" << std::endl;
      return false;
    }
    return true;
  }

private:
  std::unique_ptr<GLuint, std::function<void(GLuint *)>> buffer_id{
      new GLuint(0), [](auto ptr) {
        glDeleteBuffers(1, ptr);
        delete ptr;
      }};
}; // namespace opengl

class vertex_array final {

public:
  explicit vertex_array(bool use_after_create = true) {
    glGenVertexArrays(1, vertex_array_id.get());

    if (check_error()) {
      throw_exception("glGenVertexArrays failed");
    }
    if (use_after_create && !use()) {
      throw_exception("can't use vertex_array");
    }
  }

  vertex_array(const vertex_array &) = default;
  vertex_array &operator=(const vertex_array &) = default;

  vertex_array(vertex_array &&) noexcept = default;
  vertex_array &operator=(vertex_array &&) noexcept = default;

  ~vertex_array() noexcept = default;

  bool use() noexcept { return bind(*vertex_array_id); }
  bool unuse() noexcept { return bind(0); }

private:
  bool bind(GLuint id) noexcept {
    glBindVertexArray(id);
    if (check_error()) {
      std::cerr << "glBindVertexArray failed" << std::endl;
      return false;
    }
    return true;
  }

private:
  std::shared_ptr<GLuint> vertex_array_id{new GLuint(0), [](GLuint *ptr) {
                                            glDeleteVertexArrays(1, ptr);
                                            delete ptr;
                                          }};
};
} // namespace opengl
