#pragma once

#include <vector>
#include <numeric>
#include <stdexcept>
#include <nlohmann/json.hpp>

class GCounter {
  public:
    GCounter(uint8_t num_nodes, uint8_t node_id) : counts_(num_nodes), node_id_(node_id) {
      if (node_id >= num_nodes) {
        throw std::invalid_argument("Node ID must be less than the number of nodes");
      }
    }

    void increment() {
      counts_[node_id_]++;
    }

    [[nodiscard]] uint64_t value() const {
      return std::accumulate(counts_.begin(), counts_.end(), uint64_t{0});
    }

    [[nodiscard]] GCounter merge(const GCounter& other) const {
      if (counts_.size() != other.counts_.size()) {
        throw std::invalid_argument("Cannot merge GCounters with different sizes");
      }

      GCounter result(static_cast<uint8_t>(counts_.size()), node_id_);
      for (std::size_t i = 0; i < counts_.size(); ++i) {
        result.counts_[i] = std::max(counts_[i], other.counts_[i]);
      }
      return result;
    }

    bool operator==(const GCounter& other) const {
      return counts_ == other.counts_;
    }

    [[nodiscard]] nlohmann::json to_json() const {
      return nlohmann::json{
        {"node_id", node_id_},
        {"counts", counts_}
      };
    }

    static GCounter from_json(const nlohmann::json& j) {
      auto node_id = j.at("node_id").get<uint8_t>();
      auto counts = j.at("counts").get<std::vector<uint64_t>>();
      GCounter g(static_cast<uint8_t>(counts.size()), node_id);
      g.counts_= std::move(counts);
      return g;
    }

  private:
    std::vector<uint64_t> counts_;
    uint8_t node_id_;
};