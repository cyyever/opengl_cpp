#pragma once

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include "buffer.hpp"
#include "error.hpp"
#include "texture.hpp"
#include "vertex_array.hpp"

namespace opengl {

class program final {
public:
  program() {
    *program_id = glCreateProgram();
    if (*program_id == 0) {
      throw_exception("glCreateProgram failed");
    }
  }

  program(const program &) = delete;
  program &operator=(const program &) = delete;

  program(program &&) noexcept = delete;
  program &operator=(program &&) noexcept = delete;

  ~program() noexcept = default;

  bool attach_shader_file(GLenum shader_type,
                          std::filesystem::path source_code) noexcept {
    std::ifstream source_file(source_code);
    std::stringstream sstream;
    sstream << source_file.rdbuf();
    if (source_file.fail() || source_file.bad()) {
      std::cerr << "read " << source_code << " failed";
      return false;
    }
    if (!sstream) {
      std::cerr << "store " << source_code << " in stringstream failed";
      return false;
    }
    return attach_shader(shader_type, sstream.str());
  }

  bool attach_shader(GLenum shader_type, std::string_view source_code,
                     bool replace = true) noexcept {

    std::unique_ptr<GLuint, std::function<void(GLuint *)>> shader_id(
        new GLuint(0), [this](auto ptr) {
          glDetachShader(*program_id, *ptr);
          glDeleteShader(*ptr);
          delete ptr;
        });

    *shader_id = glCreateShader(shader_type);
    if (*shader_id == 0) {
      std::cerr << "glCreateShader failed" << std::endl;
      return false;
    }

    const auto source_data = source_code.data();
    GLint source_size = source_code.size();

    glShaderSource(*shader_id, 1, &source_data, &source_size);
    glCompileShader(*shader_id);

    GLint success = 0;
    glGetShaderiv(*shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
      GLchar infoLog[512]{};
      glGetShaderInfoLog(*shader_id, sizeof(infoLog), nullptr, infoLog);
      std::cerr << "glCompileShader failed" << infoLog << std::endl;
      return false;
    }
    glAttachShader(*program_id, *shader_id);
    if (check_error()) {
      std::cerr << "glAttachShader failed" << std::endl;
      return false;
    }
    if (replace) {
      shaders.erase(shader_type);
    }
    shaders[shader_type].emplace_back(std::move(shader_id));
    assigned_uniform_variables.clear();
    clear_textures();
    linked = false;
    return true;
  }

  void clear_textures() { assigned_textures.clear(); }

  bool use() noexcept {
    if (!install()) {
      return false;
    }
    if (VAO) {
      if (!VAO.value().use()) {
        return false;
      }
    }

    GLenum next_texture_unit{GL_TEXTURE0};
    for (auto &[variable_name, texture] : assigned_textures) {
      if (!texture->use(next_texture_unit)) {
        return false;
      }

      if (!set_uniform_by_callback(
              variable_name, [next_texture_unit](auto location) {
                glUniform1i(location, next_texture_unit - GL_TEXTURE0);
              })) {
        return false;
      }

      next_texture_unit++;
    }

#ifndef NDEBUG
    if (!check_uniform_assignment()) {
      return false;
    }
#endif
    return true;
  }

  bool set_uniform_by_callback(
      const std::string &variable_name,
      std::function<void(GLint location)> set_function) noexcept {
    if (!install()) {
      return false;
    }
    auto location = glGetUniformLocation(*program_id, variable_name.c_str());
    if (location == -1) {
      std::cerr << "glGetUniformLocation failed:" << variable_name << std::endl;
      return false;
    }
    set_function(location);
    if (check_error()) {
      std::cerr << "set_function failed:" << variable_name << std::endl;
      return false;
    }
    assigned_uniform_variables.insert(variable_name);
    return true;
  }

  template <typename... value_types>
  bool set_uniform(const std::string &variable_name,
                   value_types &&... values) noexcept {
    static_assert(sizeof...(values) != 0, "no value specified");
    static_assert(sizeof...(values) == 1 || sizeof...(values) == 3,
                  "unsupported number of values");
    using first_value_type =
        typename std::tuple_element<0, std::tuple<value_types...>>::type;
    static_assert(
        std::conjunction_v<std::is_same<first_value_type, value_types>...>,
        "values should be the same type");

    using real_value_type = typename std::remove_const<
        typename std::remove_reference<first_value_type>::type>::type;

    if constexpr (sizeof...(values) == 1) {
      auto &&value = std::get<0>(
          std::forward_as_tuple(std::forward<value_types>(values)...));
      if constexpr (std::is_same_v<real_value_type, GLint>) {
        return set_uniform_by_callback(variable_name, [value](auto location) {
          glUniform1i(location, value);
        });
      } else if constexpr (std::is_same_v<real_value_type, GLfloat>) {
        return set_uniform_by_callback(variable_name, [value](auto location) {
          glUniform1f(location, value);
        });
      } else if constexpr (std::is_same_v<real_value_type,
                                          ::opengl::texture_2D> ||
                           std::is_same_v<real_value_type,
                                          ::opengl::texture_cube_map>) {
        std::unique_ptr<real_value_type> texture_ptr =
            std::make_unique<real_value_type>(value);
        assigned_textures.insert_or_assign(variable_name,
                                           std::move(texture_ptr));
        return true;
      } else if constexpr (std::is_same_v<real_value_type, glm::vec3>) {
        return set_uniform_by_callback(variable_name, [&value](auto location) {
          glUniform3fv(location, 1, &value[0]);
        });
      } else if constexpr (std::is_same_v<real_value_type, glm::mat4>) {
        return set_uniform_by_callback(variable_name, [&value](auto location) {
          glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
        });
      }
    } else if constexpr (sizeof...(values) == 3) {
      auto &&value1 = std::get<0>(
          std::forward_as_tuple(std::forward<value_types>(values)...));
      auto &&value2 = std::get<1>(
          std::forward_as_tuple(std::forward<value_types>(values)...));
      auto &&value3 = std::get<2>(
          std::forward_as_tuple(std::forward<value_types>(values)...));

      if constexpr (std::is_same_v<real_value_type, GLint>) {
        return set_uniform_by_callback(
            variable_name, [value1, value2, value3](auto location) {
              glUniform3i(location, value1, value2, value3);
            });
      } else if constexpr (std::is_same_v<real_value_type, GLfloat>) {
        return set_uniform_by_callback(
            variable_name, [value1, value2, value3](auto location) {
              glUniform3f(location, value1, value2, value3);
            });
      }
    }
    std::cerr << "unsupported value types" << std::endl;
    return false;
  }

  void set_vertex_array(opengl::vertex_array array) noexcept {
    VAO = std::move(array);
  }

private:
  bool link() {
    if (!linked) {
      glLinkProgram(*program_id);

      GLint success = 0;
      glGetProgramiv(*program_id, GL_LINK_STATUS, &success);
      if (!success) {
        GLchar infoLog[512]{};
        glGetProgramInfoLog(*program_id, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "glLinkProgram failed:" << infoLog << std::endl;
        return false;
      }
      linked = true;
    }
    return true;
  }

  bool install() noexcept {
    if (!link()) {
      return false;
    }
    glUseProgram(*program_id);
    if (check_error()) {
      std::cerr << "glUseProgram failed" << std::endl;
      return false;
    }
    return true;
  }

  bool check_uniform_assignment() noexcept {
    GLint count = 0;
    glGetProgramiv(*program_id, GL_ACTIVE_UNIFORMS, &count);
    if (check_error()) {
      std::cerr << "glGetProgramiv failed" << std::endl;
      return false;
    }

    GLchar name[512];
    for (GLint i = 0; i < count; i++) {
      GLint size;
      GLenum type;
      glGetActiveUniform(*program_id, static_cast<GLuint>(i), sizeof(name),
                         nullptr, &size, &type, name);
      if (assigned_uniform_variables.count(name) == 0) {
        std::cerr << "uniform variable \"" << name << "\" is not assigned"
                  << std::endl;
        return false;
      }
    }
    return true;
  }

private:
  std::unique_ptr<GLuint, std::function<void(GLuint *)>> program_id{
      new GLuint(0), [](auto ptr) {
        glDeleteProgram(*ptr);
        delete ptr;
      }};
  std::set<std::string> assigned_uniform_variables;
  std::map<std::string, std::unique_ptr<::opengl::texture>> assigned_textures;
  std::map<GLenum,
           std::vector<std::unique_ptr<GLuint, std::function<void(GLuint *)>>>>
      shaders;
  std::optional<::opengl::vertex_array> VAO;
  bool linked{false};
};
} // namespace opengl
