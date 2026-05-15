#pragma once

#include <asio.hpp>
#include <map>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <string>
#include <stdexcept>


class Heartbeat {

public:
    // destination ip address and port for a peer node
    struct Peer {
        std::string address;
        uint16_t port;
    };

    // constructor
    Heartbeat(
            uint16_t listen_port,
            std::vector<Peer> peers
            )
            : listen_port_(listen_port),
            peers_(std::move(peers)) {
        if (listen_port_ == 0) throw std::invalid_argument("Port cannot be 0");
        for (const Peer& p : peers_) {
            if (p.address.empty()) throw std::invalid_argument("Peer address cannot be empty");
            if (p.port == 0) throw std::invalid_argument("Peer port cannot be 0");
        }
    }

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
    [[nodiscard]] bool is_active(const Peer& p) const {
        std::lock_guard<std::mutex> lock(m_); //lock before reading last_active_
        // finds last_active_ value of node
        auto it = last_active_.find(peer_key(p));
        if (it == last_active_.end()) return false; // if node has never been active, returns false
        // retrieves last time node has been active
        auto time_since_active =
                std::chrono::steady_clock::now() - it -> second;
        //true if time sice active is less than TIMEOUT_S seconds ago
        return time_since_active < std::chrono::seconds(TIMEOUT_S);
    }

    // retrieves vector of active peers
    [[nodiscard]] std::vector<Peer> active_peers() const {
        // empty vector of peers
        std::vector<Peer> active_peers;

        // iterates through all peers
        for (const Peer& p : peers_) {
            // push back active peer
            if (is_active(p)) active_peers.push_back(p);
        }
        return active_peers;
    }

    // retrieves vector of inactive peers
    [[nodiscard]] std::vector<Peer> inactive_peers() const {
        // empty vector of peers
        std::vector<Peer> inactive_peers;

        // iterates through all peers
        for (const Peer& p : peers_) {
            // push back inactive peer
            if (!is_active(p)) inactive_peers.push_back(p);
        }
        return inactive_peers;
    }

private:

    void send_loop() {
        asio::io_context io; //engine to drive the socket
        // unbound UDP/IPv4 socket for sending
        asio::ip::udp::socket socket(io, asio::ip::udp::v4());

        while (running_) {

            // heartbeat payload
            std::string msg = "heartbeat:" + std::to_string(listen_port_);

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
        std::array<char, 1024> buffer{};

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
                // construct string from raw buffer bytes
                std::string payload(buffer.data(), len);
                // find seperator between type and port
                auto seperator = payload.find(':');
                if (seperator == std::string::npos) continue; //skip packets without seperator
                // extract port from payload (after seperator)
                std::string port_str = payload.substr(seperator + 1);
                //build key (address + port)
                std::string key = sender.address().to_string() + ":" + port_str;
                // retrieves the peer sending the data
                auto it = std::find_if(
                        peers_.begin(),
                        peers_.end(),
                        [&] (const Peer& p) {
                    return peer_key(p) == key;
                });
                if (it == peers_.end()) continue; //unknown sender
                //lock when writing to last_active_
                std::lock_guard<std::mutex> lock(m_);
                //sets last active to current time for sender node
                last_active_[key] = std::chrono::steady_clock::now();
            }
            // thread sleeps for 100ms; results in 10 checks every second
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // helper: converts a peer to a key identifier (address + port)
    static std::string peer_key(const Peer& p) { return p.address + ":" + std::to_string(p.port); }

    uint16_t listen_port_;
    std::vector<Peer> peers_;

    std::atomic<bool> running_{false};
    std::thread sender_;
    std::thread receiver_;

    mutable std::mutex m_;

    std::map<std::string, std::chrono::steady_clock::time_point> last_active_;
    static constexpr int TIMEOUT_S = 6;
};