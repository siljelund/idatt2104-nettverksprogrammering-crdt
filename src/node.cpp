#include "node.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

// Byte order helpers
static uint32_t swap32(uint32_t v) {
  return  ((v & 0x000000FFu) << 24u) |
          ((v & 0x0000FF00u) << 8u) |
          ((v & 0x00FF0000u) >> 8u) |
          ((v & 0xFF000000u) >> 24u);
}

// constructor / destructor
Node::Node(uint8_t node_id, uint8_t num_nodes, uint16_t port, std::vector<Heartbeat::Peer> peers) : node_id_(node_id)
  , clock_()
  , document_(node_id,clock_)
  , counter_(num_nodes, node_id)
  , users_()
  , io_context_()
  , work_guard_(asio::make_work_guard(io_context_))
  , acceptor_(io_context_)
  , reconnect_timer_(io_context_)
  , running_(true)
  , heartbeat_(port, std::move(peers))
{
  asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), port);
  acceptor_.open(ep.protocol());
  acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(ep);
  acceptor_.listen();
}

Node::~Node() {
  if (running_) stop();
}

// Interface

void Node::start() {
  heartbeat_.start();
  start_accept();
  schedule_reconnect();
  io_thread_ = std::thread([this]() { io_context_.run(); });
}

void Node::connect(const std::string& host, uint16_t port) {
  auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
  asio::ip::tcp::endpoint ep(asio::ip::make_address(host), port);
  asio::post(io_context_, [this, socket, ep]() {
    socket->async_connect(ep, [this, socket](const asio::error_code& ec) {
      if (!ec) {
        {
          std::lock_guard<std::mutex> lock(sockets_mutex_);
          sockets_.push_back(socket);
        }
        start_receive(socket);
      }
    });
  });
}

void Node::sync_all() {
  std::vector<std::shared_ptr<asio::ip::tcp::socket>> snapshot;
  {
    std::lock_guard<std::mutex> lock(sockets_mutex_);
    snapshot = sockets_;
  }
  for (auto& s : snapshot) {
    send_state_to(s);
  }
}

void Node::insert(std::size_t pos, char value) {
  {
    std::lock_guard<std::mutex> lock(crdt_mutex_);
    document_.insert(pos, value);
  }
  sync_all();
}

void Node::remove(std::size_t pos) {
  {
    std::lock_guard<std::mutex> lock(crdt_mutex_);
    document_.remove(pos);
  }
  sync_all();
}

void Node::increment() {
  {
    std::lock_guard<std::mutex> lock(crdt_mutex_);
    counter_.increment();
  }
  sync_all();
}

std::string Node::document_value() const {
  std::lock_guard<std::mutex> lock(crdt_mutex_);
  return document_.value();
}

uint64_t Node::counter_value() const {
  std::lock_guard<std::mutex> lock(crdt_mutex_);
  return counter_.value();
}

std::vector<Heartbeat::Peer> Node::active_peers() const {
  return heartbeat_.active_peers();
}

std::vector<Heartbeat::Peer> Node::inactive_peers() const {
  return heartbeat_.inactive_peers();
}

void Node::stop() {
  running_ = false;
  sync_cv_.notify_all();

  asio::post(io_context_, [this]() {
    asio::error_code ec;
    acceptor_.cancel(ec);
    acceptor_.close(ec);
    reconnect_timer_.cancel(ec);
    std::vector<std::shared_ptr<asio::ip::tcp::socket>> snapshot;
    {
      std::lock_guard<std::mutex> lock(sockets_mutex_);
      snapshot = sockets_;
    }
    for (auto& s : snapshot) {
      asio::error_code se;
      s->cancel(se);
      s->shutdown(asio::ip::tcp::socket::shutdown_both, se);
      s->close(se);
    }
  });

  work_guard_.reset();
  heartbeat_.stop();
  if (io_thread_.joinable()) io_thread_.join();
}

bool Node::wait_for_sync() {
  std::unique_lock<std::mutex> lock(crdt_mutex_);
  sync_cv_.wait_for(lock, std::chrono::seconds(2),
    [this]() { return new_sync_received_ || !running_; });
  bool received = new_sync_received_;
  new_sync_received_ = false;
  return received;
}

// async accept
void Node::start_accept() {
  auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
  acceptor_.async_accept(*socket, [this, socket](const asio::error_code& ec) {
    if (!ec) {
      {
        std::lock_guard<std::mutex> lock(sockets_mutex_);
        sockets_.push_back(socket);
      }
      start_receive(socket);
    }
    if (running_) {
      start_accept();
    }
  });
}

// async receive
void Node::start_receive(std::shared_ptr<asio::ip::tcp::socket> socket) {
  auto len_buf = std::make_shared<uint32_t>(0);
  asio::async_read(*socket, asio::buffer(len_buf.get(), sizeof(uint32_t)),
    [this, socket, len_buf](const asio::error_code& ec, std::size_t) {
      if (ec) {
        remove_socket(socket);
        return;
      }
      uint32_t len = swap32(*len_buf);
      auto msg = std::make_shared<std::string>(len, '\0');
      asio::async_read(*socket, asio::buffer(msg->data(), len),
        [this, socket, msg](const asio::error_code& ec, std::size_t) {
          if (ec) {
            remove_socket(socket);
            return;
          }
          process_message(*msg);
          if (running_) {
            start_receive(socket);
          }
    });
  });
}

// async write
void Node::send_state_to(std::shared_ptr<asio::ip::tcp::socket> socket) {
  std::string body;
  {
    std::lock_guard<std::mutex> lock(crdt_mutex_);
    nlohmann::json j = {
      {"type", "state_sync"},
      {"node_id", node_id_},
      {"lamport", clock_.value()},
      {"document", document_.to_json()},
      {"counter", counter_.to_json()},
      {"users", users_.to_json()},
    };
    body = j.dump();
  }

  asio::post(io_context_, [this, socket, body = std::move(body)]() mutable {
    enqueue_write(socket, std::move(body));
  });
}

void Node::enqueue_write(std::shared_ptr<asio::ip::tcp::socket> socket, std::string msg) {
  auto& q = write_queues_[socket.get()];
  q.pending.push_back(std::move(msg));
  if (!q.writing) {
    do_write(socket);
  }
}

void Node::do_write(std::shared_ptr<asio::ip::tcp::socket> socket) {
  auto it = write_queues_.find(socket.get());
  if (it == write_queues_.end() || it->second.pending.empty()) {
    if (it != write_queues_.end()) {
      it->second.writing = false;
    }
    return;
  }
  auto& q = it->second;
  q.writing = true;

  const std::string& body = q.pending.front();
  uint32_t len_net = swap32(static_cast<uint32_t>(body.size()));
  auto frame = std::make_shared<std::string>(4 + body.size(), '\0');
  std::memcpy(frame->data(), &len_net, 4);
  std::memcpy(frame->data() + 4, body.data(), body.size());
  q.pending.pop_front();

  asio::async_write(*socket, asio::buffer(*frame), [this, socket, frame](const asio::error_code& ec, std::size_t) {
    if (ec) {
      remove_socket(socket);
      return;
    }
    do_write(socket);
  });
}

void Node::schedule_reconnect() {
  reconnect_timer_.expires_after(std::chrono::milliseconds(500));
  reconnect_timer_.async_wait([this](const asio::error_code& ec) {
    if (ec || !running_) return;
    for (const auto& peer : heartbeat_.active_peers()) {
      if (!is_connected_to(peer.address, peer.port)) {
        auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
        asio::ip::tcp::endpoint ep(asio::ip::make_address(peer.address), peer.port);
        socket->async_connect(ep, [this, socket](const asio::error_code& ec) {
          if (!ec) {
            {
              std::lock_guard<std::mutex> lock(sockets_mutex_);
              sockets_.push_back(socket);
            }
            start_receive(socket);
          }
        });
      }
    }
    schedule_reconnect();
  });
}

//helpers

void Node::process_message(const std::string& msg) {
  try {
    auto j = nlohmann::json::parse(msg);
    {
      std::lock_guard<std::mutex> lock(crdt_mutex_);
      clock_.update(j.at("lamport").get<uint64_t>());
      auto remote_doc = RGA::from_json(j.at("document"), node_id_, clock_);
      document_ = document_.merge(remote_doc);
      auto remote_counter = GCounter::from_json(j.at("counter"));
      counter_ = counter_.merge(remote_counter);
      auto remote_users = ORSet<std::string>::from_json(j.at("users"));
      users_ = users_.merge(remote_users);
      new_sync_received_ = true;
    }
    sync_cv_.notify_all();
  } catch (...) {}
}


bool Node::is_connected_to(const std::string& host, uint16_t port) {
  std::lock_guard<std::mutex> lock(sockets_mutex_);
  for (const auto& s : sockets_) {
    if (!s->is_open()) continue;
    asio::error_code ec;
    auto ep = s->remote_endpoint(ec);
    if (!ec && ep.port() == port && ep.address().to_string() == host) {
      return true;
    }
  }
  return false;
}

void Node::remove_socket(std::shared_ptr<asio::ip::tcp::socket> socket) {
  write_queues_.erase(socket.get());
  std::lock_guard<std::mutex> lock(sockets_mutex_);
  sockets_.erase(
    std::remove(sockets_.begin(), sockets_.end(), socket),
    sockets_.end());
}

