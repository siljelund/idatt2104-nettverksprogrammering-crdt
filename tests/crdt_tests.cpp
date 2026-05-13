#include <catch2/catch_test_macros.hpp>
#include "g_counter.hpp"
#include "pn_counter.hpp"

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