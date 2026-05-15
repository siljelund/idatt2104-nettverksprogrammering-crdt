#include <catch2/catch_test_macros.hpp>
#include "g_counter.hpp"
#include "pn_counter.hpp"
#include "g_set.hpp"
#include "or_set.hpp"
#include "lamport_clock.hpp"

// TODO: add tests when implementing CRDT-types

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