#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace opengl {
class camera final {
public:
  // Constructor with vectors
  camera(glm::vec3 position_, glm::vec3 up_, glm::vec3 front_)
      : position(position_), world_up(up_), front(front_) {

    auto tmp = front;
    tmp.x = std::fabs(tmp.x);
    tmp.y = 0;
    yaw = glm::degrees(
        glm::acos(glm::dot(glm::normalize(tmp), glm::vec3(1.0f, 0.0f, 0.0f))));

    if (front.z < 0) {
      yaw = -yaw;
    }

    tmp = front;
    tmp.x = 0;
    tmp.z = std::fabs(tmp.z);

    pitch = glm::degrees(
        glm::acos(glm::dot(glm::normalize(tmp), glm::vec3(0.0f, 0.0f, 1.0f))));

    if (front.y < 0) {
      pitch = -pitch;
    }

    up = world_up;
    update_coordinate_system();
  }

  camera(const camera &) = delete;
  camera &operator=(const camera &) = delete;

  camera(camera &&) noexcept = delete;
  camera &operator=(camera &&) noexcept = delete;

  // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
  glm::mat4 get_view_matrix() {
    return glm::lookAt(position, position + front, up);
  }

  enum class movement { forward, backward, left, right };

  void move(movement direction, float delta_time) {
    float velocity = movement_speed * delta_time;
    switch (direction) {
    case movement::forward:
      position += glm::normalize(front) * velocity;
      break;
    case movement::backward:
      position -= glm::normalize(front) * velocity;
      break;
    case movement::left:
      position -= right_unit_vector * velocity;
      break;
    case movement::right:
      position += right_unit_vector * velocity;
    }
  }

  void lookat(float xoffset, float yoffset, bool constrain_pitch = true) {
    xoffset *= lookat_sensitivity;
    yoffset *= lookat_sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrain_pitch) {
      if (pitch > 89.0f)
        pitch = 89.0f;
      if (pitch < -89.0f)
        pitch = -89.0f;
    }

    // Update front, Right and Up Vectors using the updated Euler angles
    update_coordinate_system();
  }

  void add_fov(float yoffset) {
    if (fov >= 1.0f && fov <= 45.0f)
      fov -= yoffset;
    if (fov <= 1.0f)
      fov = 1.0f;
    if (fov >= 45.0f)
      fov = 45.0f;
  }

  float get_fov() { return glm::radians(fov); }

  const auto &get_position() const { return position; }

  const auto &get_front() const { return front; }

private:
  void update_coordinate_system() {
    // Calculate the new front vector
    glm::vec3 new_front;
    new_front.x =
        front.length() * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    new_front.y = front.length() * sin(glm::radians(pitch));
    new_front.z =
        front.length() * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = new_front;
    right_unit_vector = glm::normalize(glm::cross(front, world_up));
    up = glm::normalize(glm::cross(right_unit_vector, front));
  }

private:
  // camera Attributes
  glm::vec3 position;
  glm::vec3 world_up;
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right_unit_vector;
  float fov{45.0f};
  float movement_speed{2.5f};
  float lookat_sensitivity{0.05f};
  // Euler Angles
  float yaw{};
  float pitch{};
};
} // namespace opengl
