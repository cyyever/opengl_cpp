#pragma once

#include <array>
#include <filesystem>
#include <gsl/gsl>
#include <iostream>
#include <memory>
#include <stb_image.h>
#include <stdexcept>

#include "error.hpp"

namespace opengl {

class texture {

public:
  struct extra_config {
    extra_config() : generate_mipmap{true}, flip_y{true} {}
    bool generate_mipmap;
    bool flip_y;
  };

  enum class type {
    diffuse = 1,
    specular,
  };

public:
  virtual ~texture() noexcept = default;

  template <typename value_type>
  bool set_parameter(GLenum pname, value_type value) noexcept {
    if constexpr (std::is_same_v<value_type, GLint>) {
      glTexParameteri(target, pname, value);
    } else if constexpr (std::is_same_v<value_type, GLfloat>) {
      glTexParameterf(target, pname, value);
    } else {
      static_assert("not supported value type");
    }
    if (check_error()) {
      std::cerr << "glTexParameter failed" << std::endl;
      return false;
    }
    return true;
  }

  bool use(GLenum unit) {
    glActiveTexture(unit);
    if (check_error()) {
      std::cerr << "glActiveTexture failed" << std::endl;
      return false;
    }
    if (!bind()) {
      return false;
    }
    return true;
  }

protected:
  texture(GLenum target_) : target{target_} {
    glGenTextures(1, texture_id.get());
    if (check_error()) {
      throw_exception("glBindTexture failed");
    }
  }

  texture(const texture &) = default;
  texture &operator=(const texture &) = default;

  texture(texture &&) noexcept = default;
  texture &operator=(texture &&) noexcept = default;

  bool load_texture_image(std::filesystem::path image, GLenum loading_target,
                          const extra_config &config) noexcept {

    if (!std::filesystem::exists(image)) {
      std::cerr << "no image " << image << std::endl;
      return false;
    }
    stbi_set_flip_vertically_on_load(config.flip_y);

    int width, height, channel;
    auto *data =
        stbi_load(image.string().c_str(), &width, &height, &channel, 0);
    if (!data) {
      std::cerr << "stbi_load " << image << " failed" << std::endl;
      return false;
    }
    auto cleanup = gsl::finally([data]() { stbi_image_free(data); });
    GLenum format = 0;
    if (channel == 3) {
      format = GL_RGB;
    } else if (channel == 4) {
      format = GL_RGBA;
    } else {
      std::cerr << "unsupported channels" << std::endl;
      return false;
    }

    glTexImage2D(loading_target, 0, GL_RGBA, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    if (check_error()) {
      std::cerr << "glTexImage2D failed" << std::endl;
      return false;
    }

    return true;
  }
  bool bind() noexcept {
    glBindTexture(target, *texture_id);
    if (check_error()) {
      std::cerr << "glBindTexture failed" << std::endl;
      return false;
    }
    return true;
  }

protected:
  std::shared_ptr<GLuint> texture_id{new GLuint(0), [](GLuint *ptr) {
                                       glDeleteTextures(1, ptr);
                                       delete ptr;
                                     }};
  GLenum target{};
};

class frame_buffer;

class texture_2D final : public texture {
public:
  explicit texture_2D(std::filesystem::path image, extra_config config = {})
      : texture(GL_TEXTURE_2D) {

    if (!bind()) {
      throw_exception("bind failed");
    }

    if (!load_texture_image(image, GL_TEXTURE_2D, config)) {
      throw_exception("load_texture_image failed");
    }
    if (!set_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR)) {
      throw_exception("set GL_TEXTURE_MIN_FILTER failed");
    }
    if (!set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR)) {
      throw_exception("set GL_TEXTURE_MAG_FILTER failed");
    }
    if (config.generate_mipmap) {
      glGenerateMipmap(target);
      if (check_error()) {
        throw_exception("glGenerateMipmap failed");
      }
    }
  }

  explicit texture_2D(GLsizei width, GLsizei height) : texture(GL_TEXTURE_2D) {

    if (!bind()) {
      throw_exception("bind failed");
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    if (check_error()) {
      throw_exception("glTexImage2D failed");
    }

    if (!set_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR)) {
      throw_exception("set GL_TEXTURE_MIN_FILTER failed");
    }
    if (!set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR)) {
      throw_exception("set GL_TEXTURE_MAG_FILTER failed");
    }
  }

  texture_2D(const texture_2D &) = default;
  texture_2D &operator=(const texture_2D &) = default;

  texture_2D(texture_2D &&) noexcept = default;
  texture_2D &operator=(texture_2D &&) noexcept = default;

  ~texture_2D() override = default;

private:
  friend class frame_buffer;
  GLuint get_id() const { return *texture_id; }
};

class texture_cube_map final : public texture {
public:
  explicit texture_cube_map(std::array<std::filesystem::path, 6> images,
                            extra_config config = {})
      : texture(GL_TEXTURE_CUBE_MAP) {

    if (!bind()) {
      throw_exception("bind failed");
    }

    for (size_t i = 0; i < 6; i++) {
      if (!load_texture_image(images[i], GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                              config)) {
        throw_exception("load_texture_image failed");
      }
    }

    for (auto pname :
         {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R}) {
      if (!set_parameter(pname, GL_CLAMP_TO_EDGE)) {
        throw_exception("set_parameter failed");
      }
    }
    if (!set_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR)) {
      throw_exception("set GL_TEXTURE_MIN_FILTER failed");
    }
    if (!set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR)) {
      throw_exception("set GL_TEXTURE_MAG_FILTER failed");
    }
    if (config.generate_mipmap) {
      glGenerateMipmap(target);
      if (check_error()) {
        throw_exception("glGenerateMipmap failed");
      }
    }
  }

  texture_cube_map(const texture_cube_map &) = default;
  texture_cube_map &operator=(const texture_cube_map &) = default;

  texture_cube_map(texture_cube_map &&) noexcept = default;
  texture_cube_map &operator=(texture_cube_map &&) noexcept = default;

  ~texture_cube_map() override = default;
};

} // namespace opengl
