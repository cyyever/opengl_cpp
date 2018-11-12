#pragma once

#include <gsl/gsl>
#include <iostream>

#include "context.hpp"
#include "error.hpp"

namespace opengl {

class buffer {
public:
  buffer(GLenum target_) : target{target_} {
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

  virtual ~buffer() noexcept = default;

protected:
  bool alloc(size_t size) noexcept {
    if (size == 0) {
      std::cerr << "can't alloc 0 bytes" << std::endl;
      return false;
    }

    if constexpr (opengl::context::gl_minor_version < 5) {
      if (!bind()) {
        return false;
      }
    }
    if constexpr (opengl::context::gl_minor_version < 5) {
      glBufferData(target, size, nullptr, GL_STATIC_DRAW);
    } else {
      glNamedBufferData(*buffer_id, size, nullptr, GL_STATIC_DRAW);
    }
    if (check_error()) {
      std::cerr << "glBufferData failed" << std::endl;
      return false;
    }
    return true;
  }

  template <typename T>
  bool write_part(gsl::span<const T> data_view, GLintptr offset) noexcept {
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
      glBufferSubData(target, offset, data_view.size_bytes(), data_view.data());
    } else {
      glNamedBufferSubData(*buffer_id, offset, data_view.size_bytes(),
                           data_view.data());
    }
    if (check_error()) {
      std::cerr << "glBufferSubData failed" << std::endl;
      return false;
    }
    return true;
  }

  template <typename T> bool write_all(gsl::span<const T> data_view) noexcept {
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

  bool bind() noexcept {
    glBindBuffer(target, *buffer_id);
    if (check_error()) {
      std::cerr << "glBindBuffer failed" << std::endl;
      return false;
    }
    return true;
  }

protected:
  std::unique_ptr<GLuint, std::function<void(GLuint *)>> buffer_id{
      new GLuint(0), [](auto ptr) {
        glDeleteBuffers(1, ptr);
        delete ptr;
      }};
  GLenum target;
};

} // namespace opengl
