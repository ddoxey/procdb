#pragma once

#include <optional>
#include <vector>

#include "payload.hpp"

class Collect {
 public:
  static std::optional<std::vector<Field>> get(const PayloadType type);
};
