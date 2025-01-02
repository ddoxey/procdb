#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "collect.hpp"
#include "payload.hpp"

constexpr std::chrono::milliseconds kDwellTime(750);

int server_fd{-1}, client_fd{-1};

void HandleSignal(int signal) {
  std::cout << "[" << signal << "] Shutting down server..." << std::endl;
  if (client_fd >= 0) close(client_fd);
  if (server_fd >= 0) close(server_fd);
  exit(0);
}

void SendPayload(int client_fd, const Payload &payload) {
  std::vector<char> buffer;
  payload.Serialize(buffer);
  ssize_t bytes_sent = send(client_fd, buffer.data(), buffer.size(), 0);
  if (bytes_sent < 0) {
    perror("Failed to send data");
    exit(EXIT_FAILURE);  // Handle gracefully if needed
  }
}

void StartServer(int port) {
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 1) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  client_fd = accept(server_fd, nullptr, nullptr);
  if (client_fd < 0) {
    perror("Accept failed");
    exit(EXIT_FAILURE);
  }

  uint32_t it_count{0};
  std::cout << "Client connected. Sending payloads...\n";

  while (true) {
    auto start_time = std::chrono::steady_clock::now();

    for (const auto type :
         {PayloadType::PROCESS_STATS, PayloadType::NETWORK_STATS,
          PayloadType::PROCESS_LIST}) {
      auto fields = Collect::get(type);

      if (fields) {
        Payload payload(type, fields->size(), *fields);
        std::cout << it_count++ << ") Sending "
                  << PayloadTypeToString(payload.type) << " ... ";
        SendPayload(client_fd, payload);
        std::cout << "OK" << std::endl;
      }
    }

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed < kDwellTime) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kDwellTime) -
                                  elapsed);
    }
  }

  close(client_fd);
  close(server_fd);
}

int main() {
  signal(SIGINT, HandleSignal);  // Handle Ctrl+C
  StartServer(8080);
  return 0;
}
