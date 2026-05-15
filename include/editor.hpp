#pragma once

#include "heartbeat.hpp"
#include <string>
#include <algorithm>
#include <iostream>

class Editor {

public:
    Editor(
            uint16_t listen_port,
            std::vector<Heartbeat::Peer> peers)
            : heartbeat_(listen_port, std::move(peers)) {}
    ~Editor() {
        heartbeat_.stop();
    }

    void run() {
        heartbeat_.start();
        std::string line;
        handle_help();
        while (true) {
            std::cout << ">";
            if (!std::getline(std::cin, line)) break;
            std::transform(line.begin(),
                           line.end(),
                           line.begin(),
                           ::tolower);

            if (line == "quit") break;
            else if (line == "help") handle_help();
            else if (line == "status") handle_status();
            else if (line == "insert") handle_insert();
            else std::cout << "Unknown command.\n "
                              "Type 'help' for possible commands\n";
        }
    }

private:

    Heartbeat heartbeat_;

    void handle_status() {
        std::cout << "Active:\n";
        for (const Heartbeat::Peer& p : heartbeat_.active_peers()) {
            std::cout << p.address << "\n";
        }
        std::cout << "Inactive:\n";
        for (const Heartbeat::Peer& p : heartbeat_.inactive_peers()) {
            std::cout << p.address << "\n";
        }
    }
    void handle_insert() {
        std::string line;
        std::cout << "Ready for insertion\n";
        std::getline(std::cin, line);
        buffer_ += line;
        std::cout << "buffer: " << buffer_ << "\n";

    }
    static void handle_help() {
        std::cout << "Commands: \n" <<
        "'quit': Quits the program\n" <<
        "'help': Provides an overview of all commands and their actions\n" <<
        "'status': Displays a list of all active and inactive users\n" <<
        "'insert': Enables you to create an entry in the program yourself\n";
    }

    std::string buffer_;
};
