
#include "error.hpp"

namespace opengl {

std::optional<GLenum> check_error(source_location error_location) {
  auto error_code = glGetError();
  if (error_code == GL_NO_ERROR) {
    return {};
  }

  std::string error_msg;
  switch (error_code) {
  case GL_INVALID_ENUM:
    error_msg = "INVALID_ENUM";
    break;
  case GL_INVALID_VALUE:
    error_msg = "INVALID_VALUE";
    break;
  case GL_INVALID_OPERATION:
    error_msg = "INVALID_OPERATION";
    break;
  case GL_STACK_OVERFLOW:
    error_msg = "STACK_OVERFLOW";
    break;
  case GL_STACK_UNDERFLOW:
    error_msg = "STACK_UNDERFLOW";
    break;
  case GL_OUT_OF_MEMORY:
    error_msg = "OUT_OF_MEMORY";
    break;
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    error_msg = "INVALID_FRAMEBUFFER_OPERATION";
    break;
  }
  std::cerr << error_location.file_name() << '(' << error_location.line() << ')'
            << ' ' << error_msg << std::endl;
  return {error_code};
}

} // namespace opengl
