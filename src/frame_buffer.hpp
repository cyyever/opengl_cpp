#pragma once

#include <cstddef>
#include <iostream>

#include "context.hpp"
#include "error.hpp"
#include "render_buffer.hpp"
#include "texture.hpp"

namespace opengl {

class frame_buffer final {

public:
  frame_buffer() {
    if constexpr (opengl::context::gl_minor_version < 5) {
      glGenFramebuffers(1, frame_buffer_id.get());
      if (check_error()) {
        throw_exception("glGenFramebuffers failed");
      }
    } else {
      glCreateFramebuffers(1, frame_buffer_id.get());
      if (check_error()) {
        throw_exception("glCreateFramebuffers failed");
      }
    }
  }

  frame_buffer(const frame_buffer &) = delete;
  frame_buffer &operator=(const frame_buffer &) = delete;

  frame_buffer(frame_buffer &&) noexcept = default;
  frame_buffer &operator=(frame_buffer &&) noexcept = default;

  ~frame_buffer() noexcept = default;

  void add_color_attachment(opengl::texture_2D texture_image) {
    color_textures.emplace_back(texture_image);
    is_complete = false;
  }

  void
  add_depth_and_stencil_attachment(opengl::depth_stencil_render_buffer buffer) {
    depth_stencil_buffer = buffer;
    is_complete = false;
  }

  bool use() {
    if (!attach()) {
      return false;
    }

    if (!bind()) {
      return false;
    }
    return true;
  }

  static bool use_default() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (check_error()) {
      std::cerr << "glBindFramebuffer failed" << std::endl;
      return false;
    }
    return true;
  }

private:
  bool bind() noexcept {
    glBindFramebuffer(GL_FRAMEBUFFER, *frame_buffer_id);
    if (check_error()) {
      std::cerr << "glBindFramebuffer failed" << std::endl;
      return false;
    }
    return true;
  }

  bool attach() noexcept {
    if (is_complete) {
      return true;
    }
    if constexpr (opengl::context::gl_minor_version < 5) {
      if (!bind()) {
        return false;
      }
    }

    for (size_t i = 0; i < color_textures.size(); i++) {
      if constexpr (opengl::context::gl_minor_version < 5) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, color_textures[i].get_id(), 0);
      } else {
        glNamedFramebufferTexture(*frame_buffer_id, GL_COLOR_ATTACHMENT0 + i,
                                  color_textures[i].get_id(), 0);
      }
      if (check_error()) {
        std::cerr << "glFramebufferTexture2D failed" << std::endl;
        return false;
      }
    }

    if (depth_stencil_buffer) {
      if constexpr (opengl::context::gl_minor_version < 5) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  depth_stencil_buffer.value().get_id());
      } else {
        glNamedFramebufferRenderbuffer(
            *frame_buffer_id, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
            depth_stencil_buffer.value().get_id());
      }
      if (check_error()) {
        std::cerr << "glFramebufferRenderbuffer failed" << std::endl;
        return false;
      }
    }

    GLenum status{};

    if constexpr (opengl::context::gl_minor_version < 5) {
      status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    } else {
      status = glCheckNamedFramebufferStatus(*frame_buffer_id, GL_FRAMEBUFFER);
    }
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      std::cerr << "Framebuffer is not complete!" << std::endl;
      return false;
    }

    return true;
  }

private:
  std::vector<opengl::texture_2D> color_textures;

  std::optional<opengl::depth_stencil_render_buffer> depth_stencil_buffer;

  std::unique_ptr<GLuint, std::function<void(GLuint *)>> frame_buffer_id{
      new GLuint(0), [](auto ptr) {
        glDeleteFramebuffers(1, ptr);
        delete ptr;
      }};
  bool is_complete{false};
}; // namespace opengl

} // namespace opengl
