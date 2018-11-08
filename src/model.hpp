#pragma once

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include "mesh.hpp"

namespace opengl {

class model final {

public:
  explicit model(std::filesystem::path model_file);

  model(const model &) = delete;
  model &operator=(const model &) = delete;

  model(model &&) noexcept = delete;
  model &operator=(model &&) noexcept = delete;

  ~model() noexcept;

  bool draw(opengl::program &prog,
            const std::map<texture_2D::type, std::vector<std::string>>
                &texture_variable_names);

private:
  class impl;
  std::unique_ptr<impl> pimpl;
};

} // namespace opengl
