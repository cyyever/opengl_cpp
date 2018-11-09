#pragma once

#include <iostream>

#include "error.hpp"

namespace opengl {

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
