#include <catch2/catch_test_macros.hpp>
#include "g_counter.hpp"
#include "pn_counter.hpp"
#include "g_set.hpp"
#include "or_set.hpp"
#include "lamport_clock.hpp"
#include "rga.hpp"
#include "node.hpp"
#include <thread>
#include <chrono>

TEST_CASE("Placeholder, CI works") {
  REQUIRE(1 + 1 == 2);
}

// G-Counter

TEST_CASE("GCounter, commutativity: a.merge(b) == b.merge(a)") {
  GCounter a(3, 0);
  GCounter b(3, 1);
  GCounter c(3, 2);

  a.increment();
  a.increment();
  b.increment();
  c.increment();
  c.increment();
  c.increment();

  REQUIRE(a.merge(b) == b.merge(a));
  REQUIRE(a.merge(c) == c.merge(a));
  REQUIRE(b.merge(c) == c.merge(b));
}

TEST_CASE("GCounter, assoxiativity: a.merge(b).merge(c) == a.merge(b.merge(c))") {
  GCounter a(3, 0);
  GCounter b(3, 1);
  GCounter c(3, 2);

  a.increment();
  b.increment();
  b.increment();
  c.increment();

  GCounter lhs = a.merge(b).merge(c);
  GCounter rhs = a.merge(b.merge(c));

  REQUIRE(lhs == rhs);
}

TEST_CASE("GCounter, idempotency: a.merge(a) == a") {
  GCounter a(3, 0);

  a.increment();
  a.increment();
  a.increment();

  REQUIRE(a.merge(a) == a);
}

TEST_CASE("GCounter, value() returns correct sum after increments on multiple nodes") {
  GCounter a(3, 0);
  GCounter b(3, 1);
  GCounter c(3, 2);

  a.increment(); // node 0: 1
  b.increment(); // node 1: 1
  b.increment(); // ndoe 1: 1
  c.increment(); // node 2: 1
  c.increment(); // node 2: 2
  c.increment(); // node 2: 3

  GCounter merged = a.merge(b).merge(c);
  REQUIRE(merged.value() == 6);
}

// PN-COUNTER

TEST_CASE("PNCounter, commutativity: a.merge(b) == b.merge(a)") {
  PNCounter a(3, 0);
  PNCounter b(3, 1);

  a.increment();
  a.increment();
  a.decrement();
  b.increment();
  b.decrement();
  b.decrement();

  REQUIRE(a.merge(b) == b.merge(a));
}

TEST_CASE("PNCounter, associativity: a.merge(b).merge(c) == a.merge(b.merge(c))") {
  PNCounter a(3, 0);
  PNCounter b(3, 1);
  PNCounter c(3, 2);

  a.increment();
  b.increment();
  b.increment();
  c.increment();

  REQUIRE(a.merge(b).merge(c) == a.merge(b.merge(c)));
}

TEST_CASE("PNCounter, idempotency: a.merge(a) == a") {
  PNCounter a(3, 0);

  a.increment();
  a.increment();
  a.decrement();

  REQUIRE(a.merge(a) == a);
}

TEST_CASE("PNCounter, value() returns correct result after mixed increments and decrements") {
  PNCounter a(3, 0);
  PNCounter b(3, 1);
  PNCounter c(3, 2);

  a.increment();
  a.increment(); // node 0: +2
  b.increment();
  b.decrement();
  b.decrement(); // node 1: +1-2 = -1
  c.increment(); // node 2: +1

  PNCounter merged = a.merge(b).merge(c);
  // positive: 4, negative: 2, value = 2
  REQUIRE(merged.value() == 2);
}

TEST_CASE("PNCounter, value() can be negative") {
  PNCounter a(2, 0);
  PNCounter b(2, 1);

  a.decrement();
  a.decrement();
  b.decrement();

  PNCounter merged = a.merge(b);
  REQUIRE(merged.value() == -3);
  REQUIRE(merged.value() < 0);
}

// G-Set

TEST_CASE("GSet<int>, commutativity: a.merge(b) == b.merge(a)") {
  GSet<int> a;
  GSet<int> b;

  a.add(1);
  a.add(2);
  b.add(2);
  b.add(3);

  REQUIRE(a.merge(b) == b.merge(a));
}

TEST_CASE("GSet<int>, associativity: a.merge(b).merge(c) == a.merge(b.merge(c))") {
  GSet<int> a;
  GSet<int> b;
  GSet<int> c;

  a.add(1);
  b.add(2);
  c.add(3);

  REQUIRE(a.merge(b).merge(c) == a.merge(b.merge(c)));
}

TEST_CASE("GSet<int>, idempotency: a.merge(a) == a") {
  GSet<int> a;

  a.add(1);
  a.add(2);

  REQUIRE(a.merge(a) == a);
}

TEST_CASE("GSet<int>, contains() returns correct results after merge") {
  GSet<int> a;
  GSet<int> b;

  a.add(1);
  b.add(2);

  GSet<int> merged = a.merge(b);

  REQUIRE(merged.contains(1));
  REQUIRE(merged.contains(2));
  REQUIRE_FALSE(merged.contains(3));
}

TEST_CASE("GSet<int>, adding same element twice does not duplicate it") {
  GSet<int> a;

  a.add(42);
  a.add(42);

  REQUIRE(a.size() == 1);
}

TEST_CASE("GSet<std::string>, commutativity: a.merge(b) == b.merge(a)") {
  GSet<std::string> a;
  GSet<std::string> b;

  a.add("hello");
  a.add("world");
  b.add("world");
  b.add("crdt");

  REQUIRE(a.merge(b) == b.merge(a));
}

TEST_CASE("GSet<std::string>, adding same element twice does not duplicate it") {
  GSet<std::string> a;

  a.add("duplicate");
  a.add("duplicate");

  REQUIRE(a.size() == 1);
}

TEST_CASE("GSet, remove() throws std::logic_error") {
  GSet<int> a;
  a.add(1);

  REQUIRE_THROWS_AS(a.remove(1), std::logic_error);
}

// ORSet

TEST_CASE("ORSet<std::string>, commutativity: a.merge(b) == b.merge(a)") {
  ORSet<std::string> a;
  ORSet<std::string> b;

  a.add("x");
  a.add("y");
  b.add("y");
  b.add("z");

  REQUIRE(a.merge(b) == b.merge(a));
}

TEST_CASE("ORSet<std::string>, associativity: a.merge(b).merge(c) == a.merge(b.merge(c))") {
  ORSet<std::string> a;
  ORSet<std::string> b;
  ORSet<std::string> c;

  a.add("x");
  b.add("y");
  c.add("z");

  REQUIRE(a.merge(b).merge(c) == a.merge(b.merge(c)));
}

TEST_CASE("ORSet<std::string>, idempotency: a.merge(a) == a") {
  ORSet<std::string> a;

  a.add("x");
  a.add("y");

  REQUIRE(a.merge(a) == a);
}

TEST_CASE("ORSet<std::string>, add and remove works correctly") {
  ORSet<std::string> a;

  a.add("x");
  REQUIRE(a.contains("x"));
  REQUIRE(a.size() == 1);

  a.remove("x");
  REQUIRE_FALSE(a.contains("x"));
  REQUIRE(a.size() == 0);
}

TEST_CASE("ORSet<std::string>, re-add after remove works correctly") {
  ORSet<std::string> a;

  a.add("x");
  a.remove("x");
  REQUIRE_FALSE(a.contains("x"));

  a.add("x");
  REQUIRE(a.contains("x"));
}

TEST_CASE("ORSet<std::string>, concurrent add survives remove on other node") {
  // Node A adds "x" → tag t1
  ORSet<std::string> a;
  a.add("x");

  // Node B syncs state from A → also has tag t1
  ORSet<std::string> b = a;

  // Node A concurrently adds "x" again → tag t2
  a.add("x");

  // Node B removes "x" → clears t1, unaware of t2
  b.remove("x");
  REQUIRE_FALSE(b.contains("x"));

  // After merge: t2 from A survives → "x" is still in set
  ORSet<std::string> merged = a.merge(b);
  REQUIRE(merged.contains("x"));
}

TEST_CASE("ORSet<int>, commutativity: a.merge(b) == b.merge(a)") {
  ORSet<int> a;
  ORSet<int> b;

  a.add(1);
  a.add(2);
  b.add(2);
  b.add(3);

  REQUIRE(a.merge(b) == b.merge(a));
}

TEST_CASE("ORSet<int>, adding same element twice does not duplicate it") {
  ORSet<int> a;

  a.add(42);
  a.add(42);

  // Both adds give separate tags, so size is still 1 (same element)
  REQUIRE(a.size() == 1);
  REQUIRE(a.contains(42));
}

// Lamport Clock

TEST_CASE("LamportClock, tick() increments the clock by 1 each time") {
  LamportClock c;

  REQUIRE(c.tick() == 1);
  REQUIRE(c.tick() == 2);
  REQUIRE(c.tick() == 3);
}

TEST_CASE("LamportClock, update() takes max of local and received, then adds 1") {
  LamportClock c;
  c.tick(); // time_ = 1

  // received > local: jumps to received + 1
  REQUIRE(c.update(5) == 6);
  REQUIRE(c.value() == 6);
}

TEST_CASE("LamportClock, update() with lower value still increments by 1") {
  LamportClock c;
  c.tick();
  c.tick();
  c.tick(); // time_ = 3

  // received < local: max(3, 1) + 1 = 4
  REQUIRE(c.update(1) == 4);
  REQUIRE(c.value() == 4);
}

TEST_CASE("LamportClock, update() with higher value jumps to received + 1") {
  LamportClock c;
  c.tick(); // time_ = 1

  // received >> local: max(1, 100) + 1 = 101
  REQUIRE(c.update(100) == 101);
  REQUIRE(c.value() == 101);
}

TEST_CASE("LamportClock, two clocks that ticked the same number of times are equal") {
  LamportClock a;
  LamportClock b;

  a.tick();
  a.tick();
  b.tick();
  b.tick();

  REQUIRE(a == b);
}

TEST_CASE("LamportClock, to_json() and from_json() preserves clock value") {
  LamportClock original;
  original.tick();
  original.tick();
  original.tick(); // time_ = 3

  nlohmann::json j = original.to_json();
  LamportClock restored = LamportClock::from_json(j);

  REQUIRE(restored.value() == original.value());
  REQUIRE(restored == original);
}

// RGA
TEST_CASE("RGA, basic insert and value()") {
  LamportClock clock;
  RGA rga(0, clock);
  rga.insert(0, 'H');
  rga.insert(1, 'i');
  REQUIRE(rga.value() == "Hi");
  REQUIRE(rga.size() == 2);
}

TEST_CASE("RGA, basic remove tombstones character") {
  LamportClock clock;
  RGA rga(0, clock);
  rga.insert(0, 'a');
  rga.insert(1, 'b');
  rga.insert(2, 'c');
  rga.remove(1);
  REQUIRE(rga.value() == "ac");
  REQUIRE(rga.size() == 2);
}

TEST_CASE("RGA, value() does not include tombstoned characters") {
  LamportClock clock;
  RGA rga(0, clock);
  rga.insert(0, 'x');
  rga.remove(0);
  REQUIRE(rga.value() == "");
  REQUIRE(rga.size() == 0);
}

TEST_CASE("RGA, commutativity: a.merge(b).value() == b.merge(a).value()") {
  LamportClock ca, cb;
  RGA a(0, ca); RGA b(1, cb);
  a.insert(0, 'A'); a.insert(1, 'B');
  b.insert(0, 'X'); b.insert(1, 'Y');
  REQUIRE(a.merge(b).value() == b.merge(a).value());
}

TEST_CASE("RGA, associativity: a.merge(b).merge(c).value() == a.merge(b.merge(c)).value()") {
  LamportClock ca, cb, cc;
  RGA a(0, ca); RGA b(1, cb); RGA c(2, cc);
  a.insert(0, 'A');
  b.insert(0, 'B');
  c.insert(0, 'C');
  REQUIRE(a.merge(b).merge(c).value() == a.merge(b.merge(c)).value());
}

TEST_CASE("RGA, idempotency: a.merge(a).value() == a.value()") {
  LamportClock clock;
  RGA a(0, clock);
  a.insert(0, 'H'); a.insert(1, 'i');
  REQUIRE(a.merge(a).value() == a.value());
}

TEST_CASE("RGA, concurrent inserts at same position are resolved deterministically") {
  LamportClock ca, cb;
  RGA a(0, ca); RGA b(1, cb);
  a.insert(0, 'A');
  b.insert(0, 'B');
  std::string from_a = a.merge(b).value();
  std::string from_b = b.merge(a).value();
  REQUIRE(from_a == from_b);
  REQUIRE(from_a.size() == 2);
}

TEST_CASE("RGA, classic conflict: Node A inserts ' world', Node B inserts ' everyone' after 'Hello'") {
  LamportClock ca, cb;
  RGA a(0, ca);

  a.insert(0, 'H'); a.insert(1, 'e'); a.insert(2, 'l'); a.insert(3, 'l'); a.insert(4, 'o');
  REQUIRE(a.value() == "Hello");

  RGA b = RGA::from_json(a.to_json(), 1, cb);
  REQUIRE(b.value() == "Hello");

  // Concurrent: A appends " world", B appends " everyone"
  for (char ch : std::string(" world")) a.insert(a.size(), ch);
  for (char ch : std::string(" everyone"))   b.insert(b.size(), ch);

  RGA merged_ab = a.merge(b);
  RGA merged_ba = b.merge(a);

  // Both must converge to the same text
  REQUIRE(merged_ab.value() == merged_ba.value());

  // Both words must be present
  std::string result = merged_ab.value();
  REQUIRE(result.find("Hello")    != std::string::npos);
  REQUIRE(result.find("world") != std::string::npos);
  REQUIRE(result.find("everyone")   != std::string::npos);
}

TEST_CASE("RGA, delete on one node propagates through merge") {
  LamportClock ca, cb;
  RGA a(0, ca);
  a.insert(0, 'a'); a.insert(1, 'b'); a.insert(2, 'c');

  RGA b = RGA::from_json(a.to_json(), 1, cb);
  b.remove(1);
  REQUIRE(b.value() == "ac");

  REQUIRE(a.merge(b).value() == "ac");
}

// Node
TEST_CASE("Node, two nodes can connect to each other") {
  Node node0(0, 2, 19000);
  Node node1(1, 2, 19001);
  node0.start();
  node1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  node0.connect("127.0.0.1", 19001);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  REQUIRE(node0.document_value() == "");
  REQUIRE(node1.document_value() == "");

  node0.stop();
  node1.stop();
}

TEST_CASE("Node, after insert on Node 0, Node 1 receives and merges correctly") {
  Node node0(0, 2, 19002);
  Node node1(1, 2, 19003);
  node0.start();
  node1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  node0.connect("127.0.0.1", 19003);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  node0.insert(0, 'A');
  (void)node1.wait_for_sync();

  REQUIRE(node1.document_value() == "A");

  node0.stop();
  node1.stop();
}

TEST_CASE("Node, after increment on Node 1, Node 0 receives and merges correctly") {
  Node node0(0, 2, 19004);
  Node node1(1, 2, 19005);
  node0.start();
  node1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  node0.connect("127.0.0.1", 19005);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  node1.increment();
  (void)node0.wait_for_sync();

  REQUIRE(node0.counter_value() == 1);

  node0.stop();
  node1.stop();
}

TEST_CASE("Node, both nodes converge to same document_value after concurrent inserts") {
  Node node0(0, 2, 19006);
  Node node1(1, 2, 19007);
  node0.start();
  node1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  node0.connect("127.0.0.1", 19007);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  node0.insert(0, 'A');
  node1.insert(0, 'B');

  (void)node0.wait_for_sync();
  (void)node1.wait_for_sync();

  REQUIRE(node0.document_value() == node1.document_value());
  REQUIRE(node0.document_value().size() == 2);

  node0.stop();
  node1.stop();
}

TEST_CASE("Node, stop() joins all threads without deadlock") {
  Node node0(0, 2, 19008);
  Node node1(1, 2, 19009);
  node0.start();
  node1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  node0.connect("127.0.0.1", 19009);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  node0.insert(0, 'X');
  (void)node1.wait_for_sync();

  node0.stop();
  node1.stop();

  REQUIRE(true);
}

TEST_CASE("Node, three nodes converge to same document_value after concurrent inserts") {
  Node node0(0, 3, 19010);
  Node node1(1, 3, 19011);
  Node node2(2, 3, 19012);
  node0.start(); node1.start(); node2.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  node0.connect("127.0.0.1", 19011);
  node0.connect("127.0.0.1", 19012);
  node1.connect("127.0.0.1", 19012);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  node0.insert(0, 'A');
  node1.insert(0, 'B');
  node2.insert(0, 'C');

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    auto v0 = node0.document_value();
    auto v1 = node1.document_value();
    auto v2 = node2.document_value();
    if (v0 == v1 && v1 == v2 && v0.size() == 3) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  REQUIRE(node0.document_value() == node1.document_value());
  REQUIRE(node1.document_value() == node2.document_value());
  REQUIRE(node0.document_value().size() == 3);

  node0.stop(); node1.stop(); node2.stop();
}