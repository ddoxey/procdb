#include "collect.hpp"

#include <netinet/in.h>
#include <sys/socket.h>

#include "shell_cmd.hpp"

std::optional<std::vector<Field>>
Collect::get(const PayloadType type)
{
    std::vector<Field> fields;

    switch (type) {
    case PayloadType::PROCESS_STATS: {
        auto process_stats = ShellCmd::run(
            "ps -ec -o pcpu,pmem,comm | awk '"
            "BEGIN{getnp=\"sysctl -n hw.ncpu\"; getnp | getline nproc; "
            "close(getnp)} "
            "/Google Chrome/{mem += $1; cpu += $2} "
            "END { printf(\"%0.2f,%0.2f\\n\", (cpu / nproc), mem) }'");
        auto tab_count = ShellCmd::run("pgrep 'Google Chrome Helper' | wc -l");
        auto of_count = ShellCmd::run("lsof -p $(pidof 'Google Chrome' | "
                                      "sed -e 's/[ ]*$//' -e 's/ /,/g') | wc -l");
        if (process_stats || tab_count || of_count) {
            if (process_stats) {
                auto stats = ShellCmd::split(process_stats.out->at(0), ',');
                if (stats.size() == 2) {
                    fields.push_back({ "CPU Usage", std::stof(stats.at(0)) });
                    fields.push_back({ "MEM Usage", std::stof(stats.at(1)) });
                }
            }
            if (tab_count) {
                fields.push_back({ "Browser Tabs", std::stof(tab_count.out->at(0)) });
            }
            if (of_count) {
                fields.push_back({ "Open Files", std::stof(of_count.out->at(0)) });
            }
        }
        break;
    }

    case PayloadType::NETWORK_STATS: {
        auto net_stats = ShellCmd::run("nettop -m tcp -l 1 | grep 'Google Chrome'");
        if (net_stats) {
            for (auto& net_stat : *net_stats.out) {
                ShellCmd::dedup(net_stat, ' ');
                fields.push_back({ net_stat, {} });
            }
        }
        break;
    }

    case PayloadType::PROCESS_LIST: {
        auto processes = ShellCmd::run("ps -ec -o pcpu,pmem,comm | grep 'Google Chrome'");
        if (processes) {
            for (const auto& process : *processes.out) {
                fields.push_back({ process, {} });
            }
        }
        break;
    }
    }

    if (fields.empty())
        return std::nullopt;

    return fields;
}
