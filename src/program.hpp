#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "error.hpp"
#include "texture.hpp"
#include "uniform_buffer.hpp"
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

  program(program &&) = default;
  program &operator=(program &&) = default;

  ~program() noexcept {
    for (auto const &[block_name, _] : uniform_blocks) {
      auto it = cross_program_uniform_blocks.find(block_name);
      if (it != cross_program_uniform_blocks.end()) {
        if (it->second.use_count() == 1) {
          cross_program_uniform_blocks.erase(it);
        }
      }
    }
  }

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
          if (program_id) {
            glDetachShader(*program_id, *ptr);
          }
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

    auto block_names_opt = get_uniform_block_names();
    if (!block_names_opt) {
      return false;
    }
    for (auto const &block_name : block_names_opt.value()) {
      if (uniform_blocks.count(block_name)) {
        continue;
      }
      auto it = cross_program_uniform_blocks.find(block_name);
      if (it == cross_program_uniform_blocks.end()) {
        std::cerr << "uniform block\"" << block_name << "\" is not assigned"
                  << std::endl;
        return false;
      }
      uniform_blocks.insert(*it);
    }

    GLuint binding_point = 0;
    for (const auto &[uniform_block_name, uniform_buffer] : uniform_blocks) {
      auto block_index =
          glGetUniformBlockIndex(*program_id, uniform_block_name.c_str());
      if (block_index == GL_INVALID_INDEX) {
        std::cerr << "glGetUniformBlockIndex failed" << std::endl;
        return false;
      }
      glUniformBlockBinding(*program_id, block_index, binding_point);
      if (check_error()) {
        std::cerr << "glUniformBlockBinding failed" << std::endl;
        return false;
      }

      if (!uniform_buffer->use(binding_point)) {
        puts("fdsfds");
        return false;
      }
      binding_point++;
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

  template <typename... value_types>
  bool set_uniform_of_block(const std::string &block_name,
                            const std::string &variable_name,
                            value_types &&... values) noexcept {
    static_assert(sizeof...(values) != 0, "no value specified");
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
      if constexpr (std::is_same_v<real_value_type, glm::mat4>) {
        return set_uniform_of_block_by_callback(
            block_name, variable_name, [&value](auto &UBO, auto offset) {
              return UBO.write(std::forward<decltype(value)>(value), offset);
            });
      }
    }
    std::cerr << "unsupported value types" << std::endl;
    return false;
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

  std::optional<std::vector<std::string>> get_uniform_block_names() noexcept {
    GLint count = 0;
    glGetProgramiv(*program_id, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    if (check_error()) {
      std::cerr << "glGetProgramiv failed" << std::endl;
      return {};
    }

    std::vector<std::string> block_names;
    GLchar name[512];
    for (GLint i = 0; i < count; i++) {
      GLsizei size;
      glGetActiveUniformBlockName(*program_id, static_cast<GLuint>(i),
                                  sizeof(name), &size, name);
      if (check_error()) {
        std::cerr << "glGetActiveUniformBlockName failed" << std::endl;
        return {};
      }
      block_names.push_back(name);
    }
    return block_names;
  }

  bool check_uniform_assignment() noexcept {
    GLint count = 0;
    glGetProgramiv(*program_id, GL_ACTIVE_UNIFORMS, &count);
    if (check_error()) {
      std::cerr << "glGetProgramiv failed" << std::endl;
      return false;
    }

    auto assigned_total_uniform_variables = assigned_uniform_variables;

    for (auto const &[block_name, _] : uniform_blocks) {
      assigned_total_uniform_variables.insert(
          assigned_uniform_variables_of_blocks[block_name].begin(),
          assigned_uniform_variables_of_blocks[block_name].end());
    }

    GLchar name[512];
    for (GLint i = 0; i < count; i++) {
      GLint size;
      GLenum type;
      glGetActiveUniform(*program_id, static_cast<GLuint>(i), sizeof(name),
                         nullptr, &size, &type, name);
      if (check_error()) {
        std::cerr << "glGetActiveUniform failed" << std::endl;
        return {};
      }
      if (assigned_total_uniform_variables.count(name) == 0) {
        std::cerr << "uniform variable \"" << name << "\" is not assigned"
                  << std::endl;
        return false;
      }
    }
    return true;
  }

  bool set_uniform_of_block_by_callback(
      const std::string &block_name, const std::string &variable_name,
      std::function<void(opengl::uniform_buffer &UBO, GLint offset)>
          set_function) noexcept {
    if (!install()) {
      return false;
    }

    auto UBO_ptr = get_uniform_block(block_name);
    if (!UBO_ptr) {
      return false;
    }

    GLuint index = 0;
    auto variable_name_ptr = variable_name.c_str();
    glGetUniformIndices(*program_id, 1, &variable_name_ptr, &index);
    if (check_error()) {
      std::cerr << "glGetUniformIndices failed:" << variable_name << std::endl;
      return false;
    }

    GLint offset = -1;
    glGetActiveUniformsiv(*program_id, 1, &index, GL_UNIFORM_OFFSET, &offset);
    if (check_error()) {
      std::cerr << "get GL_UNIFORM_OFFSET failed:" << variable_name
                << std::endl;
      return false;
    }

    set_function(*UBO_ptr, offset);
    if (check_error()) {
      std::cerr << "set_function failed:" << variable_name << std::endl;
      return false;
    }

    assigned_uniform_variables_of_blocks[block_name].push_back(variable_name);
    return true;
  }

  std::shared_ptr<::opengl::uniform_buffer>
  get_uniform_block(const std::string &block_name) noexcept {
    if (!install()) {
      return {};
    }

    auto block_index = glGetUniformBlockIndex(*program_id, block_name.c_str());
    if (block_index == GL_INVALID_INDEX) {
      std::cerr << "glGetUniformBlockIndex failed" << std::endl;
      return {};
    }

    if (auto it = uniform_blocks.find(block_name); it != uniform_blocks.end()) {
      return it->second;
    }

    auto it = cross_program_uniform_blocks.find(block_name);
    if (it != cross_program_uniform_blocks.end()) {
      auto UBO_ptr = it->second.lock();
      if (UBO_ptr) {
        return UBO_ptr;
      }
    }

    GLint data_size = 0;
    glGetActiveUniformBlockiv(*program_id, block_index,
                              GL_UNIFORM_BLOCK_DATA_SIZE, &data_size);
    if (check_error()) {
      std::cerr << "glGetActiveUniformBlockiv failed" << std::endl;
      return {};
    }
    assert(data_size > 0);

    auto [it2, _] = uniform_blocks.emplace(
        block_name, std::make_shared<opengl::uniform_buffer>(
                        static_cast<size_t>(data_size)));
    cross_program_uniform_blocks.emplace(block_name, it2->second);
    return it2->second;
  }

private:
  std::set<std::string> assigned_uniform_variables;
  std::map<std::string, std::unique_ptr<::opengl::texture>> assigned_textures;
  std::map<GLenum,
           std::vector<std::unique_ptr<GLuint, std::function<void(GLuint *)>>>>
      shaders;
  std::optional<::opengl::vertex_array> VAO;
  std::map<std::string, std::shared_ptr<opengl::uniform_buffer>> uniform_blocks;
  inline static std::map<std::string, std::weak_ptr<opengl::uniform_buffer>>
      cross_program_uniform_blocks;
  inline static std::map<std::string, std::vector<std::string>>
      assigned_uniform_variables_of_blocks;
  bool linked{false};
  std::unique_ptr<GLuint, std::function<void(GLuint *)>> program_id{
      new GLuint(0), [](auto ptr) {
        glDeleteProgram(*ptr);
        delete ptr;
      }};
};
} // namespace opengl
