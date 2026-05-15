#include <catch2/catch_test_macros.hpp>
#include "g_counter.hpp"
#include "pn_counter.hpp"
#include "g_set.hpp"

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