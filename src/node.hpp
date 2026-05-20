#pragma once

#include <rga.hpp>
#include <g_counter.hpp>
#include <or_set.hpp>
#include <lamport_clock.hpp>

#include <asio.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <chrono>

class Node {
  public:
    Node(uint8_t node_id, uint8_t num_nodes, uint16_t port);
    ~Node();

    void start();
    void connect(const std::string& host, uint16_t port);
    void send_state(asio::ip::tcp::socket& socket);
    void receive_state(asio::ip::tcp::socket& socket);
    void sync_all();

    void insert(std::size_t pos, char value);
    void remove(std::size_t pos);
    void increment();

    [[nodiscard]] std::string document_value() const;
    [[nodiscard]] uint64_t counter_value() const;

    void stop();
    void wait_for_sync();

  private:
    uint8_t node_id_;

    LamportClock clock_;
    RGA document_;
    GCounter counter_;
    ORSet<std::string> users_;

    mutable std::mutex crdt_mutex_;
    std::condition_variable sync_cv_;
    bool new_sync_received_ = false;

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;

    std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets_;
    mutable std::mutex sockets_mutex_;

    std::vector<std::thread> threads_;
    std::mutex threads_mutex_;

    std::atomic<bool> running_;

    void accept_loop();
    void receive_loop(std::shared_ptr<asio::ip::tcp::socket> socket);
    void remove_socket(std::shared_ptr<asio::ip::tcp::socket> socket);

    static std::string read_message(asio::ip::tcp::socket& socket);
    static void write_message(asio::ip::tcp::socket& socket, const std::string& msg);
};