#include "shell_cmd.hpp"

#include <cstdio>
#include <sstream>

// Utility to remove duplicate adjacent chars
void ShellCmd::dedup(std::string &str, char extra) {
  auto newEnd =
      std::unique(str.begin(), str.end(), [extra](char left, char right) {
        return left == extra && right == extra;
      });
  str.erase(newEnd, str.end());
}

// Utility to split a string by a delimiter
std::vector<std::string> ShellCmd::split(const std::string &str,
                                         char delimiter) {
  std::vector<std::string> lines;
  std::istringstream stream(str);
  std::string token;

  while (std::getline(stream, token, delimiter)) {
    lines.push_back(token);
  }

  return lines;
}

// Executes a shell command and return a Result
ShellCmd::Result ShellCmd::run(const std::string &cmd) {
  char buffer[4096];
  std::string stdout_data, stderr_data;

  // Temporary file for stderr redirection
  FILE *stderr_file = tmpfile();
  if (!stderr_file) {
    perror("Failed to create temporary file for stderr");
    return {std::nullopt, std::nullopt, -1};
  }

  // Build the command string with stderr redirection
  std::string full_cmd = cmd + " 2>&" + std::to_string(fileno(stderr_file));
  FILE *pipe = popen(full_cmd.c_str(), "r");
  if (!pipe) {
    perror("popen failed");
    fclose(stderr_file);
    return {std::nullopt, std::nullopt, -1};
  }

  // Capture stdout
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    stdout_data += buffer;
  }

  int32_t exit_status = pclose(pipe);

  // Capture stderr
  rewind(stderr_file);
  while (fgets(buffer, sizeof(buffer), stderr_file) != nullptr) {
    stderr_data += buffer;
  }
  fclose(stderr_file);

  auto make_optional =
      [](const std::string &data) -> std::optional<std::vector<std::string>> {
    return data.empty() ? std::nullopt
                        : std::optional<std::vector<std::string>>(split(data));
  };

  return {make_optional(stdout_data), make_optional(stderr_data), exit_status};
}
