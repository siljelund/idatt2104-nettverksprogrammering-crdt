#include "node.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

static void print_state(Node& node) {
    std::cout << "document : \"" << node.document_value() << "\"\n";
    std::cout << "counter : " << node.counter_value() << "\n";

    auto active = node.active_peers();
    auto inactive = node.inactive_peers();

    std::cout << "peers online : ";
    if (active.empty()) std::cout << "(none)";
    for (const auto& p : active) {
        std::cout << p.address << ":" << p.port << " ";
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    uint8_t node_id = 0;
    uint16_t port = 0;
    std::vector<uint16_t> peer_ports;

    for (int i = 1; i < argc; i++) {
        std::string flag = argv[i];
        if ((flag == "--node") && i + 1 < argc) {
            node_id = static_cast<uint8_t>(std::stoi(argv[++i]));
        } else if ((flag == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((flag == "--peers") && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string token;
            while (std::getline(ss, token, ',')) {
                peer_ports.push_back(static_cast<uint16_t>(std::stoi(token)));
            }
        }
    }

    if (port == 0) {
        std::cerr << "Usage: ./editior_demo --node <id> --port <port> " "[--peers <port1,port2,...>]\n"
        << "Example: ./editor_demo --node 0 --port 9000 --peers 9001,9002\n";
        return 1;
    }

    // empty vector of peers
    std::vector<Heartbeat::Peer> peers;

    for (uint16_t p : peer_ports) {
        peers.push_back({"127.0.0.1", p});
    }

    auto num_nodes = static_cast<uint8_t>(1 + peer_ports.size());
    Node node(node_id, num_nodes, port, std::move(peers));
    node.start();

    std::cout << "Node " << static_cast<int>(node_id) << " listening on port " << port << "\n";
    std::cout << "Waiting 1s for peers to come up...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // backgroun thread to print notifications on incoming sync
    std::atomic<bool> demo_running{true};
    std::thread sync_watcher([&]() {
        while (demo_running) {
            if (node.wait_for_sync() && demo_running) {
                std::cout << "\n[sync] document: \"" << node.document_value() << "\" counter: " << node.counter_value() << "\n> " << std::flush;
            }
        }
    });

    std::cout << "Commands: ins <pos> <text> | del <pos> <len> " "| inc | show | sync | quit\n> " << std::flush;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        try {
            if (cmd == "quit") {
                break;
            } else if (cmd == "ins") {
                std::size_t pos;
                std::string text;
                if (!(iss >> pos >> text)) {
                    std::cout << "Usage: ins <pos> <text>\n";
                } else {
                    for (std::size_t i = 0; i < text.size(); ++i) {
                        node.insert(pos + i, text[i]);
                    }
                    std::cout << "document: \"" << node.document_value() << "\" counter: " << node.counter_value() << "\n";
                }
            } else if (cmd == "del") {
                std::size_t pos, len;
                if (!(iss >> pos >> len)) {
                    std::cout << "Usage: del <pos> <len>\n";
                } else {
                    for (std::size_t i = 0; i < len; ++i) {
                        node.remove(pos);
                    }
                    std::cout << "document: \"" << node.document_value() << "\"\n";
                }
            } else if (cmd == "inc") {
                node.increment();
                std::cout << "counter: \"" << node.counter_value() << "\n";
            } else if (cmd == "show") {
                print_state(node);
            } else if (cmd == "sync") {
                node.sync_all();
                std::cout << "synced.\n";
            } else if (!cmd.empty()) {
                std::cout << "Unknown command. " "Try: ins | del | inc | show | sync | quit\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }

        std::cout << std::flush;
    }

    demo_running = false;
    node.stop();
    sync_watcher.join();

    std::cout << "Goodbye\n";
    return 0;
}