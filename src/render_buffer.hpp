#pragma once

#include <cstddef>
#include <iostream>

#include "context.hpp"
#include "error.hpp"

namespace opengl {

class render_buffer {

protected:
  render_buffer() {
    if constexpr (opengl::context::gl_minor_version < 5) {
      glGenRenderbuffers(1, render_buffer_id.get());
      if (check_error()) {
        throw_exception("glGenRenderbuffers failed");
      }
    } else {
      glCreateRenderbuffers(1, render_buffer_id.get());
      if (check_error()) {
        throw_exception("glCreateRenderbuffers failed");
      }
    }
  }

  render_buffer(const render_buffer &) = default;
  render_buffer &operator=(const render_buffer &) = default;

  render_buffer(render_buffer &&) noexcept = default;
  render_buffer &operator=(render_buffer &&) noexcept = default;

  ~render_buffer() noexcept = default;

  bool bind() noexcept {
    glBindRenderbuffer(GL_RENDERBUFFER, *render_buffer_id);
    if (check_error()) {
      std::cerr << "glBindRenderbuffer failed" << std::endl;
      return false;
    }
    return true;
  }

protected:
  std::shared_ptr<GLuint> render_buffer_id{new GLuint(0), [](auto ptr) {
                                             glDeleteRenderbuffers(1, ptr);
                                             delete ptr;
                                           }};
};

class frame_buffer;

class depth_stencil_render_buffer : public render_buffer {

public:
  depth_stencil_render_buffer(GLsizei width, GLsizei height) {
    if constexpr (opengl::context::gl_minor_version < 5) {
      if (!bind()) {
        throw_exception("bind failed");
      }
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width,
                            height);
      if (check_error()) {
        throw_exception("glRenderbufferStorage failed");
      }
    } else {
      glNamedRenderbufferStorage(*render_buffer_id, GL_DEPTH24_STENCIL8, width,
                                 height);
      if (check_error()) {
        throw_exception("glNamedRenderbufferStorage failed");
      }
    }
  }

  depth_stencil_render_buffer(const depth_stencil_render_buffer &) = default;
  depth_stencil_render_buffer &
  operator=(const depth_stencil_render_buffer &) = default;

  depth_stencil_render_buffer(depth_stencil_render_buffer &&) noexcept =
      default;
  depth_stencil_render_buffer &
  operator=(depth_stencil_render_buffer &&) noexcept = default;

  ~depth_stencil_render_buffer() noexcept = default;

private:
  friend class frame_buffer;
  GLuint get_id() const { return *render_buffer_id; }
};
} // namespace opengl
