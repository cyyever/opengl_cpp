#pragma once

#include "glad/glad.h"

#include <GLFW/glfw3.h>
#include <functional>
#include <gsl/gsl>
#include <optional>

namespace opengl {

class context final {
public:
  class window final {

  public:
    window() = delete;
    window(const window &) = delete;
    window &operator=(const window &) = delete;

    window(window &&rhs) { operator=(std::move(rhs)); }
    window &operator=(window &&rhs) {
      if (this != &rhs) {
        if (handler) {
          glfwDestroyWindow(handler);
          handler = nullptr;
        }
        std::swap(handler, rhs.handler);
      }
      return *this;
    }

    ~window() {
      if (handler) {
        glfwDestroyWindow(handler);
      }
    }

    operator GLFWwindow *() const { return handler; }

  private:
    friend class context;
    window(GLFWwindow *handler_) : handler(handler_) {}

  private:
    GLFWwindow *handler{nullptr};
  };
  static std::optional<window> create(int window_width, int window_height,
                                      const std::string &title);

private:
  static void APIENTRY debug_callback(GLenum source, GLenum type, GLuint id,
                                      GLenum severity,
                                      [[maybe_unused]] GLsizei length,
                                      const GLchar *message,
                                      [[maybe_unused]] const void *userParam);

public:
  static constexpr GLint gl_minor_version = 5;

private:
  inline static gsl::final_action cleanup{
      gsl::finally([]() { glfwTerminate(); })};
};

} // namespace opengl
