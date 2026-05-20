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

Node::Node(uint8_t node_id, uint8_t num_nodes, uint16_t port) : node_id_(node_id)
  , clock_()
  , document_(node_id,clock_)
  , counter_(num_nodes, node_id)
  , users_()
  , io_context_()
  , acceptor_(io_context_)
  , running_(true)
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
  std::lock_guard<std::mutex> lock(threads_mutex_);
  threads_.emplace_back([this]() { accept_loop(); });
}

void Node::connect(const std::string& host, uint16_t port) {
  auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
  asio::ip::tcp::endpoint ep(asio::ip::make_address(host), port);
  socket->connect(ep);

  {
    std::lock_guard<std::mutex> lock(sockets_mutex_);
    sockets_.push_back(socket);
  }
  {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    threads_.emplace_back([this, socket]() { receive_loop(socket); });
  }
}

void Node::send_state(asio::ip::tcp::socket& socket) {
  std::string msg;
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
    msg = j.dump();
  }
  write_message(socket, msg);
}

void Node::receive_state(asio::ip::tcp::socket& socket) {
  std::string msg = read_message(socket);
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
}

void Node::sync_all() {
  std::vector<std::shared_ptr<asio::ip::tcp::socket>> snapshot;
  {
    std::lock_guard<std::mutex> lock(sockets_mutex_);
    snapshot = sockets_;
  }
  for (auto& s : snapshot) {
    try {
      send_state(*s);
    } catch (...) {
      remove_socket(s);
    }
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

void Node::stop() {
  running_ = false;
  sync_cv_.notify_all();

  asio::error_code ec;
  acceptor_.close(ec);

  {
    std::lock_guard<std::mutex> lock(sockets_mutex_);
    for (auto& s : sockets_) {
      asio::error_code se;
      s->shutdown(asio::ip::tcp::socket::shutdown_both, se);
      s->close(se);
    }
  }

  std::vector<std::thread> to_join;
  {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    to_join = std::move(threads_);
  }
  for (auto& t : to_join) {
    if (t.joinable()) t.join();
  }
}

void Node::wait_for_sync() {
  std::unique_lock<std::mutex> lock(crdt_mutex_);
  sync_cv_.wait_for(lock, std::chrono::seconds(2),
    [this]() { return new_sync_received_ || !running_; });
  new_sync_received_ = false;
}

// Private helpers
void Node::accept_loop() {
  while (running_) {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    asio::error_code ec;
    acceptor_.accept(*socket, ec);
    if (ec) break;

    {
      std::lock_guard<std::mutex> lock(sockets_mutex_);
      sockets_.push_back(socket);
    }
    {
      std::lock_guard<std::mutex> lock(threads_mutex_);
      threads_.emplace_back([this, socket]() { receive_loop(socket); });
    }
  }
}

void Node::receive_loop(std::shared_ptr<asio::ip::tcp::socket> socket) {
  while (running_) {
    try {
      receive_state(*socket);
    } catch (...) {
      break;
    }
  }
  remove_socket(socket);
}

void Node::remove_socket(std::shared_ptr<asio::ip::tcp::socket> socket) {
  std::lock_guard<std::mutex> lock(sockets_mutex_);
  sockets_.erase(
    std::remove(sockets_.begin(), sockets_.end(), socket),
    sockets_.end());
}

std::string Node::read_message(asio::ip::tcp::socket& socket) {
  uint32_t len_net = 0;
  asio::read(socket, asio::buffer(&len_net, sizeof(len_net)));
  uint32_t len = swap32(len_net);
  std::string msg(len, '\0');
  asio::read(socket, asio::buffer(msg.data(), len));
  return msg;
}

void Node::write_message(asio::ip::tcp::socket& socket, const std::string& msg) {
  uint32_t len_net = swap32(static_cast<uint32_t>(msg.size()));
  asio::write(socket, asio::buffer(&len_net, sizeof(len_net)));
  asio::write(socket, asio::buffer(msg));
}
