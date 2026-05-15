#include "editor.hpp"
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        // error for invalid arguments TODO maybe solve this a different way?
        std::cerr << "Missing port. Run program like this (peers is optional): \n"
        << "./editor_demo <port> <peer> <peer>\n" << "Example: ./editor_demo 9000 127.0.0.1:9001 192.168.1.10:9003\n";
        return 1;
    }
    // convert string to int and cast it as uint16
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[1]));

    // empty vector of peers
    std::vector<Heartbeat::Peer> peers;

    // for every peer argument
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        // stream argument for a peer
        std::stringstream ss(arg);

        // empty strings to hold address and port
        std::string address;
        std::string port_str;

        // store address as input before :
        std::getline(ss, address, ':');
        // store port as input after :
        std::getline(ss, port_str);

        // initialize a peer and push back
        peers.push_back({address, static_cast<uint16_t>(std::stoi(port_str))});

    }
    // initialize and run editor with port and peers values
    Editor editor(port, std::move(peers));
    editor.run();
    return 0;
}