#include "node.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

static void print_help(uint8_t node_id, uint16_t port, const Node& node) {
    auto peer_count = node.active_peers().size();
    std::cout <<
        "╔══════════════════════════════════════════════════════╗\n"
        "║              crdtpp – CRDT Demo                      ║\n"
        "║         Collaborative data structures in C++         ║\n"
        "╚══════════════════════════════════════════════════════╝\n"
        "\n"
        "Node " << static_cast<int>(node_id) << " started on port " << port << "\n"
        "Connected to " << peer_count << " peer(s)\n"
        "\n"
        "Available commands:\n"
        "  ins <pos> <text>   – insert text at position\n"
        "  del <pos> <len>    – delete characters at position\n"
        "  inc                – increment shared counter\n"
        "  dec                – decrement shared counter\n"
        "  show               – show current state\n"
        "  sync               – force sync with all peers\n"
        "  help               – show this message again\n"
        "  quit               – exit\n"
        "\n"
        "──────────────────────────────────────────────────────\n"
        "Try these examples to demonstrate each CRDT type:\n"
        "\n"
        "  G-Counter / PN-Counter (shared counter):\n"
        "    Run on any node:  inc\n"
        "    Run on any node:  dec\n"
        "    Run on any node:  show\n"
        "    → Counter reflects net sum across all nodes\n"
        "\n"
        "  RGA (collaborative text editing):\n"
        "    Run on node 0:    ins 0 Hello\n"
        "    Run on node 1:    ins 5 World\n"
        "    Run on any node:  show\n"
        "    → Both edits survive and all nodes converge to same text\n"
        "\n"
        "──────────────────────────────────────────────────────\n"
        << std::flush;
}

static void print_state(Node& node) {
    std::cout << "Document : \"" << node.document_value() << "\"\n";
    std::cout << "Counter : " << node.counter_value() << "\n";

    auto active = node.active_peers();

    std::cout << "Peers online : ";
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

    std::this_thread::sleep_for(std::chrono::seconds(1));
    print_help(node_id, port, node);

    // backgroun thread to print notifications on incoming sync
    std::atomic<bool> demo_running{true};
    std::thread sync_watcher([&]() {
        while (demo_running) {
            if (node.wait_for_sync() && demo_running) {
                std::cout << "\n[sync] document: \"" << node.document_value() << "\" counter: " << node.counter_value() << "\n> " << std::flush;
            }
        }
    });

    std::cout << "> " << std::flush;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        try {
            if (cmd == "quit") {
                break;
            } else if (cmd == "help") {
                print_help(node_id, port, node);
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
            } else if (cmd == "dec") {
                node.decrement();
                std::cout << "counter: \"" << node.counter_value() << "\"\n";
            } else if (cmd == "show") {
                print_state(node);
            } else if (cmd == "sync") {
                node.sync_all();
                std::cout << "synced.\n";
            } else if (!cmd.empty()) {
                std::cout << "Unknown command. Type 'help' to see available commands.\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }

        std::cout << "> " << std::flush;
    }

    demo_running = false;
    node.stop();
    sync_watcher.join();

    std::cout << "Goodbye\n";
    return 0;
}