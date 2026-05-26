#pragma once

#include <rga.hpp>
#include <pn_counter.hpp>
#include <or_set.hpp>
#include <lamport_clock.hpp>
#include <heartbeat.hpp>

#include <asio.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <chrono>
#include <deque>
#include <map>

class Node {
  public:
    Node(uint8_t node_id, uint8_t num_nodes, uint16_t port, std::vector<Heartbeat::Peer> peers = {});
    ~Node();

    void start();
    void connect(const std::string& host, uint16_t port);
    void sync_all();

    void insert(std::size_t pos, char value);
    void remove(std::size_t pos);
    void increment();
    void decrement();

    [[nodiscard]] std::string document_value() const;
    [[nodiscard]] int64_t counter_value() const;

    [[nodiscard]] std::vector<Heartbeat::Peer> active_peers() const;
    [[nodiscard]] std::vector<Heartbeat::Peer> inactive_peers() const;

    void stop();
    [[nodiscard]] bool wait_for_sync();

  private:
    uint8_t node_id_;

    LamportClock clock_;
    RGA document_;
    PNCounter counter_;
    ORSet<std::string> users_;

    mutable std::mutex crdt_mutex_;
    std::condition_variable sync_cv_;
    bool new_sync_received_ = false;

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::ip::tcp::acceptor acceptor_;
    asio::steady_timer reconnect_timer_;

    struct WriteQueue {
      std::deque<std::string> pending;
      bool writing = false;
    };

    std::map<asio::ip::tcp::socket*, WriteQueue> write_queues_;

    std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets_;
    mutable std::mutex sockets_mutex_;

    std::thread io_thread_;
    std::atomic<bool> running_;
    Heartbeat heartbeat_;

    void start_accept();
    void start_receive(std::shared_ptr<asio::ip::tcp::socket> socket);
    void send_state_to(std::shared_ptr<asio::ip::tcp::socket> socket);
    void enqueue_write(std::shared_ptr<asio::ip::tcp::socket> socket, std::string msg);
    void do_write(std::shared_ptr<asio::ip::tcp::socket> socket);
    void schedule_reconnect();
    void process_message(const std::string& msg);
    void remove_socket(std::shared_ptr<asio::ip::tcp::socket> socket);
    bool is_connected_to(const std::string& host, uint16_t port);
};