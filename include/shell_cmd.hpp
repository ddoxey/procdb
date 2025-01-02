#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

class ShellCmd {
 public:
  struct Result {
    std::optional<std::vector<std::string>> out;
    std::optional<std::vector<std::string>> err;
    int32_t status;
    explicit operator bool() const { return status == 0 && out.has_value(); }
  };

  // Utility to remove adjacent duplicate chars
  static void dedup(std::string &str, char extra = ' ');

  // Utility to split a string by a delimiter
  static std::vector<std::string> split(const std::string &str,
                                        char delimiter = '\n');

  // Executes a shell command and returns the stdout text split by lines
  static Result run(const std::string &cmd);
};
