#include <arpa/inet.h>
#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <thread>
#include <vector>

#include "payload.hpp"
#include "shell_cmd.hpp"

void HandleSignal(int signal) {
  curs_set(1);
  endwin();
  std::cerr << "Program terminated by signal " << signal << ". "
            << "Resources cleaned up." << std::endl;
  exit(signal);
}

bool StartCollector() {
  ShellCmd::run("pidof collector | xargs kill");
  pid_t pid = fork();
  if (pid == -1) {
    perror("Failed to fork");
    std::exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // In child process
    const char *collector_path = "./collector";
    // Open /dev/null
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
      perror("Failed to open /dev/null");
      std::exit(EXIT_FAILURE);
    }

    // Redirect stdout to /dev/null
    if (dup2(dev_null, STDOUT_FILENO) == -1) {
      perror("Failed to redirect stdout to /dev/null");
      std::exit(EXIT_FAILURE);
    }

    // Close /dev/null (not needed after dup2)
    close(dev_null);
    execl(collector_path, collector_path, nullptr);
    // If execl fails
    perror("Failed to start collector");
    std::exit(EXIT_FAILURE);
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return true;
}

std::optional<Payload> ReceivePayload(int sock) {
  std::vector<char> buffer(1024);  // Preallocate buffer with a reasonable size
  int bytes_read = recv(sock, buffer.data(), buffer.size(), 0);
  if (bytes_read < 0) {
    // No data available, return an empty optional
    return std::nullopt;
  } else if (bytes_read == 0) {
    // Connection closed by the server
    return std::nullopt;
  }

  // Resize buffer to the actual size of data received
  buffer.resize(bytes_read);

  // Deserialize and return the payload
  return Payload::Deserialize(buffer);
}

void RenderPayload(WINDOW *win, const Payload &payload) {
  werase(win);     // Clear the window
  box(win, 0, 0);  // Draw a box around the window
  mvwprintw(win, 1, 2, "%s", PayloadTypeToString(payload.type).c_str());

  int line = 2;
  for (const auto &field : payload.fields) {
    if (field.value.has_value()) {
      mvwprintw(win, ++line, 2, "%s: %.2f", field.label.c_str(),
                field.value.value());
    } else {
      mvwprintw(win, ++line, 2, "%s", field.label.c_str());
    }
  }

  wrefresh(win);  // Refresh the window to display updates
}

void StartClient(const std::string &server_ip, int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
    perror("Invalid address/Address not supported");
    exit(EXIT_FAILURE);
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Connection failed");
    exit(EXIT_FAILURE);
  }

  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  curs_set(0);  // Hide cursor

  // Create windows for each PayloadType
  [[maybe_unused]] int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  std::map<PayloadType, WINDOW *> windows = {
      {PayloadType::PROCESS_STATS, newwin(8,             // height
                                          screen_width,  // width
                                          0,             // y
                                          0              // x
                                          )},
      {PayloadType::NETWORK_STATS,
       newwin(5,             // height
              screen_width,  // width
              8,             // y
              0              // x
              )},
      {PayloadType::PROCESS_LIST,
       newwin(25,            // height
              screen_width,  // width
              13,            // y
              0              // x
              )},
  };

  // Main loop
  while (true) {
    auto payload = ReceivePayload(sock);

    if (!payload) break;

    if (windows.count(payload->type)) {
      RenderPayload(windows[payload->type], *payload);
    }
  }

  // Cleanup
  for (auto &[type, win] : windows) {
    delwin(win);
  }
  curs_set(1);  // Restore cursor
  endwin();
  close(sock);
}

int main() {
  signal(SIGINT, HandleSignal);   // Ctrl+C
  signal(SIGTERM, HandleSignal);  // termination signal

  if (!StartCollector()) {
    return EXIT_FAILURE;
  }

  try {
    StartClient("127.0.0.1", 8080);
  } catch (const std::exception &ex) {
    curs_set(1);
    endwin();
    std::cerr << "Exception: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
