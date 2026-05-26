#pragma once

#include "g_counter.hpp"

class PNCounter {
  public:
    PNCounter(uint8_t num_nodes, uint8_t node_id) :
    positive_(num_nodes, node_id),
    negative_(num_nodes, node_id) {}

    void increment() {
      positive_.increment();
    }

    void decrement() {
      negative_.increment();
    }

    [[nodiscard]] int64_t value() const {
      return static_cast<int64_t>(positive_.value()) - static_cast<int64_t>(negative_.value());
    }

    [[nodiscard]] PNCounter merge(const PNCounter& other) const {
      PNCounter result = *this;
      result.positive_ = positive_.merge(other.positive_);
      result.negative_ = negative_.merge(other.negative_);
      return result;
    }

    bool operator==(const PNCounter& other) const {
      return positive_ == other.positive_ && negative_ == other.negative_;
    }

    [[nodiscard]] nlohmann::json to_json() const {
      return nlohmann::json{
        {"positive", positive_.to_json()},
        {"negative", negative_.to_json()}
      };
    }

    static PNCounter from_json(const nlohmann::json& j) {
      auto pos = GCounter::from_json(j.at("positive"));
      auto neg = GCounter::from_json(j.at("negative"));
      uint8_t node_id = j.at("positive").at("node_id").get<uint8_t>();
      uint8_t num_nodes = static_cast<uint8_t>(j.at("positive").at("counts").size());

      PNCounter p(num_nodes, node_id);
      p.positive_ = std::move(pos);
      p.negative_ = std::move(neg);
      return p;
    }

  private:
    GCounter positive_;
    GCounter negative_;
};