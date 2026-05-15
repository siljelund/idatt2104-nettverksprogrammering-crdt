#pragma once

#include <asio.hpp>
#include <nlohmann/json.hpp>
#include <map>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <string>


class Heartbeat {

public:
    // destination ip address and port for a peer node
    struct Peer {
        std::string address;
        uint16_t port;
    };

    // constructor
    Heartbeat(
            uint8_t node_id,
            uint16_t listen_port,
            std::vector<Peer> peers
            )
            : node_id_(node_id),
            listen_port_(listen_port),
            peers_(std::move(peers)) {}

    // destructor; called automatically when the object goes out of scope
    ~Heartbeat() {
        stop();
    }

    // starts send and receive loops
    void start() {
        running_ = true;
        sender_ = std::thread(&Heartbeat::send_loop, this);
        receiver_ = std::thread(&Heartbeat::receive_loop, this);
    }

    // stops send and receive loops, waits for threads to finish before returning
    void stop() {
        running_ = false;
        if (sender_.joinable()) sender_.join();
        if (receiver_.joinable()) receiver_.join();
    }

    // checks if a node is active within TIMEOUT_S seconds
    bool is_active(uint8_t node_id) const {
        std::lock_guard<std::mutex> lock(m_); //lock before reading last_active_
        // finds last_active_ value of node
        auto it = last_active_.find(node_id);
        if (it == last_active_.end()) return false; // if node has never been active, returns false
        // retrieves last time node has been active
        auto time_since_active =
                std::chrono::steady_clock::now() - it -> second;
        //true if time sice active is less than TIMEOUT_S seconds ago
        return time_since_active < std::chrono::seconds(TIMEOUT_S);
    }

private:

    void send_loop() {
        asio::io_context io; //engine to drive the socket
        // unbound UDP/IPv4 socket for sending
        asio::ip::udp::socket socket(io, asio::ip::udp::v4());

        while (running_) {
            // build heartbeat payload
            nlohmann::json packet = {

                    {"node_id", node_id_},
                    {"type", "heartbeat"}
            };
            std::string msg = packet.dump(); //json to string for transmission

            // sends heartbeat to each peer
            for (const auto& peer : peers_) {
                // defines endpoint of payload using the node address and port
                asio::ip::udp::endpoint endpoint(
                        asio::ip::make_address(peer.address),
                        peer.port
                        );
                // sends payload through socket to endpoint
                socket.send_to(asio::buffer(msg), endpoint);
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    void receive_loop() {
        asio::io_context io; //engine to drive the socket
        // UDP/IPv4 socket for retrieval, bound to listen port
        asio::ip::udp::socket socket(
                io,
                asio::ip::udp::endpoint(asio::ip::udp::v4(),listen_port_)
                );

        // set socket non-blocking to true (avoids forever waiting on packets), affects receive_from
        socket.non_blocking(true);
        // buffer memory for incoming packets
        std::array<char, 1024> buffer;

        // receive loop
        while (running_) {
            // empty endpoint, automatically filled by receive_from
            asio::ip::udp::endpoint sender;
            // empty error code automatically filled by receive_from, if any
            asio::error_code error;

            // attempts retrieval of packet, stores the number of bytes
            std::size_t len = socket.receive_from(
                    asio::buffer(buffer), // writes incoming packet to buffer
                    sender, // fills source IP and port of sender
                    0,
                    error); // writes error, if any

            if (!error && len > 0) {
                try {
                    // parses payload string to json
                    auto j = nlohmann::json::parse(
                            buffer.data(), //start
                            buffer.data() + len); //end
                    uint8_t id = j.at("node_id").get<uint8_t>(); // retrieves node id

                    //lock when writing to last_active_
                    std::lock_guard<std::mutex> lock(m_);
                    //sets last active to current time for sender node
                    last_active_[id] = std::chrono::steady_clock::now();
                } catch(...) {} // catches invalid payload with no further action
            }
            // thread sleeps for 100ms; results in 10 checks every second
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    uint8_t node_id_;
    uint16_t listen_port_;
    std::vector<Peer> peers_;

    std::atomic<bool> running_{false};
    std::thread sender_;
    std::thread receiver_;

    mutable std::mutex m_;

    std::map<uint8_t, std::chrono::steady_clock::time_point> last_active_;
    static constexpr int TIMEOUT_S = 6;
};