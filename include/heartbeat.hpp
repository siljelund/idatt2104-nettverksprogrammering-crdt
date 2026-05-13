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
    struct Peer {
        std::string address;
        uint16_t port;
    };

    Heartbeat(
            uint8_t node_id,
            uint16_t listen_port,
            std::vector<Peer> peers
            )
            : node_id_(node_id),
            listen_port_(listen_port),
            peers_(std::move(peers)) {}

    ~Heartbeat() {
        stop();
    }

    void start() {
        running_ = true;
        sender_ = std::thread(&Heartbeat::send_loop, this);
        receiver_ = std::thread(&Heartbeat::receive_loop, this);
    }

    void stop() {
        running_ = false;
        if (sender_.joinable()) sender_.join();
        if (receiver_.joinable()) receiver_.join();
    }

    bool is_active(uint8_t node_id) const {
        std::lock_guard<std::mutex> lock(m_);
        auto it = last_active_.find(node_id);
        if (it == last_active_.end()) return false;
        auto time_since_active =
                std::chrono::steady_clock::now() - it -> second;
        return time_since_active < std::chrono::seconds(TIMEOUT_S);
    }

private:
    void send_loop() {
        asio::io_context io;
        asio::ip::udp::socket socket(io, asio::ip::udp::v4());

        while (running_) {
            nlohmann::json packet = {

                    {"node_id", node_id_},
                    {"type", "heartbeat"}
            };
            std::string msg = packet.dump();

            for (const auto& peer : peers_) {
                asio::ip::udp::endpoint endpoint(
                        asio::ip::make_address(peer.address),
                        peer.port
                        );
                socket.send_to(asio::buffer(msg), endpoint);
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    void receive_loop() {
        asio::io_context io;
        asio::ip::udp::socket socket(
                io,
                asio::ip::udp::endpoint(asio::ip::udp::v4(), listen_port_)
                );
        socket.non_blocking(true);
        std::array<char, 1024> buffer;

        while (running_) {
            asio::ip::udp::endpoint sender;
            asio::error_code error;

            std::size_t len = socket.receive_from(
                    asio::buffer(buffer),
                    sender,
                    0,
                    error);

            if (!error && len > 0) {
                try {
                    auto j = nlohmann::json::parse(
                            buffer.data(),
                            buffer.data() + len);

                    uint8_t id = j.at("node_id").get<uint8_t>();

                    std::lock_guard<std::mutex> lock(m_);
                    last_active_[id] = std::chrono::steady_clock::now();
                } catch(...) {}
            }
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