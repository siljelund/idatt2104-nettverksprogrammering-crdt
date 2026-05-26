#pragma once

#include <cstdint>
#include <algorithm>
#include <nlohmann/json.hpp>

class LamportClock {
  public:
    LamportClock() : time_(0) {}

    [[nodiscard]] uint64_t tick() {
      return ++time_;
    }

    [[nodiscard]] uint64_t update(uint64_t received_time) {
      time_ = std::max(time_, received_time) + 1;
      return time_;
    }

    [[nodiscard]] uint64_t value() const {
      return time_;
    }

    bool operator==(const LamportClock& other) const {
      return time_ == other.time_;
    }

    [[nodiscard]] nlohmann::json to_json() const {
      return nlohmann::json{{"time", time_}};
    }

    static LamportClock from_json(const nlohmann::json &j) {
      LamportClock clock;
      clock.time_ = j.at("time").get<uint64_t>();
      return clock;
    }

  private:
    uint64_t time_;
};