#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include <map>
#include <stdexcept>
#include <vector>

#include "buffer.hpp"
#include "error.hpp"
#include "program.hpp"
#include "texture.hpp"

namespace opengl {

class mesh final {

public:
  struct vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texture_coord;
  };

public:
  mesh(std::vector<vertex> vertices_, std::vector<GLuint> indices_,
       std::map<texture_2D::type, std::vector<opengl::texture_2D>> textures_);

  mesh(const mesh &) = delete;
  mesh &operator=(const mesh &) = delete;

  mesh(mesh &&) = default;
  mesh &operator=(mesh &&) = default;

  ~mesh() noexcept = default;

  bool draw(opengl::program &prog,
            const std::map<texture_2D::type, std::vector<std::string>>
                &texture_variable_names);

private:
  std::vector<vertex> vertices;
  std::vector<GLuint> indices;
  std::map<texture_2D::type, std::vector<opengl::texture_2D>> textures;
  opengl::vertex_array VAO{true};
  opengl::buffer<GL_ARRAY_BUFFER, float> VBO;
  opengl::buffer<GL_ELEMENT_ARRAY_BUFFER, GLuint> EBO;
};

} // namespace opengl
