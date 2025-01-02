#include <arpa/inet.h>
#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include <atomic>
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

std::atomic<bool> resize_flag { false };

void HandleSignal(int32_t signal)
{
    curs_set(1);
    endwin();
    std::cerr << "Program terminated by signal " << signal << ". "
              << "Resources cleaned up." << std::endl;
    exit(signal);
}

bool StartCollector(const uint32_t port)
{
    ShellCmd::run("pidof collector | xargs kill");
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        std::exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // In child process
        const char* collector_path = "./collector";
        // Open /dev/null
        int32_t dev_null = open("/dev/null", O_WRONLY);
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
        execl(collector_path, collector_path, std::to_string(port).c_str(), nullptr);
        // If execl fails
        perror("Failed to start collector");
        std::exit(EXIT_FAILURE);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return true;
}

std::optional<Payload> ReceivePayload(int32_t sock)
{
    std::vector<char> buffer(1024);
    int32_t bytes_read = recv(sock, buffer.data(), buffer.size(), 0);
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

// Function to recreate windows after resizing
void RecreateWindows(std::map<PayloadType, WINDOW*>& windows)
{
    // Clear all windows
    for (auto& [type, win] : windows) {
        if (win)
            delwin(win);
    }
    windows.clear();

    int32_t screen_height, screen_width;
    getmaxyx(stdscr, screen_height, screen_width);

    // Recalculate window sizes and positions
    int32_t y_offset = 0;
    for (const auto type : kPayloadTypes) {
        auto win_width = std::min(kPayloadDims.at(type).min_x, screen_width);
        auto win_height = std::min(kPayloadDims.at(type).min_y, screen_height);
        windows[type] = newwin(win_height, win_width, y_offset, 0);
        y_offset += win_height;
        screen_height -= win_height;
    }
}

void RenderPayload(WINDOW* win, const Payload& payload)
{
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "%s", PayloadTypeToString(payload.type).c_str());

    int32_t line = 3;
    for (const auto& field : payload.fields) {
        if (field.value.has_value()) {
            mvwprintw(
                win, line++, 2, "%s: %.2f", field.label.c_str(), field.value.value());
        } else {
            mvwprintw(win, line++, 2, "%s", field.label.c_str());
        }
    }

    if (wrefresh(win) == ERR) {
        std::cerr << "Failed to refresh window!" << std::endl;
    }
}

void StartClient(const std::string& server_ip, int32_t port)
{
    int32_t ch;
    int32_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serv_addr {};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0); // Hide cursor
    keypad(stdscr, TRUE);

    // Create windows for each PayloadType
    std::map<PayloadType, WINDOW*> windows;
    RecreateWindows(windows);

    std::cerr << "Running" << std::endl;

    // Main loop
    while (true) {
        // Check for resize events
        if (resize_flag.exchange(false)) {
            clear();
            RecreateWindows(windows);
        }

        nodelay(stdscr, TRUE);
        ch = getch();
        if (ch == 'q')
            break;

        auto payload = ReceivePayload(sock);

        if (!payload)
            break;

        if (windows.count(payload->type)) {
            RenderPayload(windows[payload->type], *payload);
        }
    }

    // Cleanup
    for (auto& [type, win] : windows) {
        delwin(win);
    }
    curs_set(1); // Restore cursor
    endwin();
    close(sock);
}

void HandleResize(int)
{
    resize_flag.store(true);
}

int main()
{
    signal(SIGWINCH, HandleResize); // Terminal window resize
    signal(SIGINT, HandleSignal); // Ctrl+C
    signal(SIGTERM, HandleSignal); // termination signal

    if (!StartCollector(8080)) {
        return EXIT_FAILURE;
    }

    try {
        StartClient("127.0.0.1", 8080);
    } catch (const std::exception& ex) {
        curs_set(1);
        endwin();
        std::cerr << "Exception: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
