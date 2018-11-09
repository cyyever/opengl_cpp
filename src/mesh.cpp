#pragma once

#include <iostream>

#include "mesh.hpp"

namespace opengl {

mesh::mesh(
    std::vector<vertex> vertices_, std::vector<GLuint> indices_,
    std::map<texture_2D::type, std::vector<opengl::texture_2D>> textures_)
    : vertices(std::move(vertices_)), indices(std::move(indices_)),
      textures(std::move(textures_)) {

  if (!VAO.use()) {
    throw_exception("use VAO failed");
  }
  if (!EBO.write(indices)) {
    throw_exception("EBO write failed");
  }

  if (!EBO.bind()) {
    throw_exception("use EBO failed");
  }

  if (!VBO.write(vertices)) {
    throw_exception("VBO write failed");
  }

  if (!VBO.vertex_attribute_pointer(0, 3, sizeof(vertex),
                                    offsetof(vertex, position))) {
    throw_exception("VBO vertex_attribute_pointer failed");
  }
  if (!VBO.vertex_attribute_pointer(1, 3, sizeof(vertex),
                                    offsetof(vertex, normal))) {
    throw_exception("VBO vertex_attribute_pointer failed");
  }

  if (!VBO.vertex_attribute_pointer(2, 2, sizeof(vertex),
                                    offsetof(vertex, texture_coord))) {
    throw_exception("VBO vertex_attribute_pointer failed");
  }

  if (!VAO.unuse()) {
    throw_exception("unuse VAO failed");
  }
}

bool mesh::draw(opengl::program &prog,
                const std::map<texture_2D::type, std::vector<std::string>>
                    &texture_variable_names) {
  prog.set_vertex_array(VAO);
  prog.clear_textures();
  for (auto const &[type, variable_names] : texture_variable_names) {
    auto it = textures.find(type);
    if (it == textures.end()) {
      std::cerr << "no texture for type " << static_cast<int>(type)
                << std::endl;
      return false;
    }
    if (variable_names.size() > it->second.size()) {
      std::cerr << "more variable then texture:" << variable_names.size() << ' '
                << it->second.size() << std::endl;
      return false;
    }
    if (variable_names.size() < it->second.size()) {
      std::cerr << "less variable then texture:" << variable_names.size() << ' '
                << it->second.size() << std::endl;
    }
    for (size_t i = 0; i < variable_names.size(); i++) {
      if (!prog.set_uniform(variable_names[i], it->second[i])) {
        return false;
      }
    }
  }

  if (!prog.use()) {
    return false;
  }
  glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
  if (check_error()) {
    std::cerr << "glDrawElements failed" << std::endl;
    return false;
  }
  return true;
}

} // namespace opengl
